/*
 * MaizRadio — Bluetooth LE and Wi-Fi Direct for voidmaiz/radio.hpp (2026-10-05).
 *
 * Owned by MaizActivity, which forwards its maizRadio* methods here (native code
 * calls the activity object, never this class by name). Everything a callback
 * learns is queued as an event; native code drains the queue once a frame.
 * Decisions (who to connect, what to advertise) stay in native code.
 *
 * BLUETOOTH LE. Every device is both a PERIPHERAL (a GATT server with our
 * service: RX written by the other side, TX notified to it) and a CENTRAL
 * (scanning for the service, connecting to a peer native code chose). The
 * scan response carries the application's tag as service data, so a device can
 * tell a member of its own group from a stranger before connecting. A
 * connection is one byte stream both ways; bytes are cut to the negotiated MTU
 * and written ONE AT A TIME, the next only after the radio confirmed the last
 * (onCharacteristicWrite / onNotificationSent), so nothing is dropped and a
 * slow link backs up into `backlog` where a progress bar can see it.
 *
 * WI-FI DIRECT. The service is published by DNS-SD (with the tag in its TXT
 * record) and discovered the same way, so only devices running a Void
 * application appear. connect() asks to form a group (the other phone's system
 * may ask its person to accept); when the group exists, native code is told
 * "owner <ip>" or "client <owner ip>" and carries IP over it itself.
 *
 * PERMISSIONS are asked only from request(), which native code calls from a
 * person's tap: "Nearby devices" on Android 12+ (13+ for Wi-Fi Direct), the
 * location permission before that, as Android requires for scanning.
 */
package org.voidmaiz;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.wifi.WifiManager;
import android.net.wifi.WpsInfo;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.provider.Settings;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

final class MaizRadio {
    // the service and its two characteristics: "maiz" in the first bytes
    static final UUID SERVICE = UUID.fromString("6d61697a-0001-4c45-8000-00805f9b34fb");
    static final UUID RX = UUID.fromString("6d61697a-0002-4c45-8000-00805f9b34fb"); // written TO the peripheral
    static final UUID TX = UUID.fromString("6d61697a-0003-4c45-8000-00805f9b34fb"); // notified BY the peripheral
    static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    static final int BLE = 0, WD = 1;
    static final int REQUEST = 0x5200; // permission request codes this class owns (+kind)
    // RadioEvent::Type, in order
    static final int FOUND = 0, LOST = 1, CONNECTED = 2, DISCONNECTED = 3, DATA = 4, GROUP = 5, GROUP_GONE = 6, ERROR = 7;
    static final int BACKLOG_LIMIT = 512 * 1024;

    private final Activity act;
    private final Handler ui = new Handler(Looper.getMainLooper());

    // ── the event queue, drained by native code ─────────────────────────────
    private final ArrayDeque<Object[]> events = new ArrayDeque<>();
    private byte[] lastBytes = null;

    private void emit(int type, int kind, String peer, String name, String tag, String detail, int rssi, byte[] data) {
        synchronized (events) {
            if (events.size() > 4096) events.pollFirst(); // a native side that stopped reading
            events.add(new Object[] {new String[] {Integer.toString(type), Integer.toString(kind), s(peer), s(name),
                                                   s(tag), s(detail), Integer.toString(rssi)}, data});
        }
    }
    private static String s(String v) { return v == null ? "" : v; }

    String[] take() {
        synchronized (events) {
            Object[] e = events.poll();
            if (e == null) return null;
            lastBytes = (byte[]) e[1];
            return (String[]) e[0];
        }
    }
    byte[] takeBytes() { return lastBytes; }

    MaizRadio(Activity a) { act = a; }

    // ── permissions ─────────────────────────────────────────────────────────
    private final boolean[] asking = {false, false};

    private String[] permissions(int kind) {
        if (kind == BLE) {
            if (Build.VERSION.SDK_INT >= 31)
                return new String[] {Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT,
                                     Manifest.permission.BLUETOOTH_ADVERTISE};
            return new String[] {Manifest.permission.ACCESS_FINE_LOCATION};
        }
        if (Build.VERSION.SDK_INT >= 33) return new String[] {Manifest.permission.NEARBY_WIFI_DEVICES};
        return new String[] {Manifest.permission.ACCESS_FINE_LOCATION};
    }
    private boolean granted(int kind) {
        for (String p : permissions(kind))
            if (act.checkSelfPermission(p) != PackageManager.PERMISSION_GRANTED) return false;
        return true;
    }
    private SharedPreferences prefs() { return act.getSharedPreferences("org.voidmaiz.radio", Context.MODE_PRIVATE); }

    /* RadioAccess: unasked 0, asking 1, ready 2, off 3, denied 4, unavailable 5 */
    int access(int kind) {
        PackageManager pm = act.getPackageManager();
        if (kind == BLE && (!pm.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE) || adapter() == null)) return 5;
        if (kind == WD && !pm.hasSystemFeature(PackageManager.FEATURE_WIFI_DIRECT)) return 5;
        if (granted(kind)) {
            if (kind == BLE) return adapter().isEnabled() ? 2 : 3;
            WifiManager wm = (WifiManager) act.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            return wm != null && wm.isWifiEnabled() ? 2 : 3;
        }
        if (asking[kind]) return 1;
        if (prefs().getBoolean("refused" + kind, false) && !act.shouldShowRequestPermissionRationale(permissions(kind)[0]))
            return 4;
        return 0;
    }

    boolean request(final int kind) {
        ui.post(() -> {
            if (!granted(kind)) {
                asking[kind] = true;
                act.requestPermissions(permissions(kind), REQUEST + kind);
                return;
            }
            try { // granted, but the radio is off: ask the system to turn it on
                if (kind == BLE && !adapter().isEnabled())
                    act.startActivity(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE));
                else if (kind == WD && Build.VERSION.SDK_INT >= 29)
                    act.startActivity(new Intent(Settings.Panel.ACTION_WIFI));
            } catch (Exception e) {
                emit(ERROR, kind, "", "", "", "could not ask to turn the radio on: " + e.getMessage(), 0, null);
            }
        });
        return true;
    }

    /* MaizActivity hands us its permission results. */
    boolean onPermissions(int code) {
        if (code != REQUEST + BLE && code != REQUEST + WD) return false;
        int kind = code - REQUEST;
        asking[kind] = false;
        prefs().edit().putBoolean("refused" + kind, !granted(kind)).apply();
        if (granted(kind)) request(kind); // and on to "turn it on", if it is off
        return true;
    }

    // ── Bluetooth LE ────────────────────────────────────────────────────────
    private BluetoothAdapter adapter() {
        BluetoothManager bm = (BluetoothManager) act.getSystemService(Context.BLUETOOTH_SERVICE);
        return bm == null ? null : bm.getAdapter();
    }

    private volatile boolean bleOn = false, wdOn = false;
    private BluetoothGattServer server;
    private BluetoothGattCharacteristic txChar;
    private BluetoothLeAdvertiser advertiser;
    private BluetoothLeScanner scanner;
    private final Map<String, BluetoothGatt> asCentral = new HashMap<>();       // we connected to them
    private final Map<String, BluetoothDevice> asPeripheral = new HashMap<>();  // they subscribed to us
    private final Map<String, Integer> mtu = new HashMap<>();
    private final Map<String, ArrayDeque<byte[]>> queue = new HashMap<>();
    private final Map<String, Integer> queued = new HashMap<>();               // bytes waiting, per peer
    private final Map<String, Boolean> busy = new HashMap<>();                  // a write in flight
    private final Map<String, Long> lastFound = new HashMap<>();

    boolean running(int kind) { return kind == BLE ? bleOn : wdOn; }

    boolean start(final int kind, final String tag) {
        if (!granted(kind)) return false;
        if (kind == WD) return startWifiDirect(tag);
        final BluetoothAdapter ba = adapter();
        if (ba == null || !ba.isEnabled()) return false;
        ui.post(() -> {
            try {
                BluetoothManager bm = (BluetoothManager) act.getSystemService(Context.BLUETOOTH_SERVICE);
                server = bm.openGattServer(act, serverCallback);
                BluetoothGattService svc = new BluetoothGattService(SERVICE, BluetoothGattService.SERVICE_TYPE_PRIMARY);
                BluetoothGattCharacteristic rx = new BluetoothGattCharacteristic(RX,
                        BluetoothGattCharacteristic.PROPERTY_WRITE | BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
                        BluetoothGattCharacteristic.PERMISSION_WRITE);
                txChar = new BluetoothGattCharacteristic(TX, BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                        BluetoothGattCharacteristic.PERMISSION_READ);
                txChar.addDescriptor(new BluetoothGattDescriptor(CCCD,
                        BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE));
                svc.addCharacteristic(rx);
                svc.addCharacteristic(txChar);
                server.addService(svc);

                advertiser = ba.getBluetoothLeAdvertiser();
                if (advertiser != null) {
                    AdvertiseSettings st = new AdvertiseSettings.Builder()
                            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
                            .setConnectable(true).build();
                    AdvertiseData data = new AdvertiseData.Builder().addServiceUuid(new ParcelUuid(SERVICE))
                            .setIncludeDeviceName(false).build();
                    AdvertiseData resp = new AdvertiseData.Builder()
                            .addServiceData(new ParcelUuid(SERVICE), tagBytes(tag)).build();
                    advertiser.startAdvertising(st, data, resp, advertiseCallback);
                }
                scanner = ba.getBluetoothLeScanner();
                if (scanner != null) {
                    List<ScanFilter> f = new ArrayList<>();
                    f.add(new ScanFilter.Builder().setServiceUuid(new ParcelUuid(SERVICE)).build());
                    ScanSettings ss = new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
                    scanner.startScan(f, ss, scanCallback);
                }
                bleOn = true;
            } catch (SecurityException e) {
                emit(ERROR, BLE, "", "", "", "Bluetooth was not allowed: " + e.getMessage(), 0, null);
            } catch (Exception e) {
                emit(ERROR, BLE, "", "", "", "Bluetooth did not start: " + e.getMessage(), 0, null);
            }
        });
        return true;
    }

    /* The tag as service data: at most 10 bytes fit in a scan response beside a
     * 128-bit UUID. Hex in, bytes out. */
    static byte[] tagBytes(String tag) {
        int n = Math.min(10, tag.length() / 2);
        byte[] b = new byte[n];
        for (int i = 0; i < n; ++i) b[i] = (byte) Integer.parseInt(tag.substring(2 * i, 2 * i + 2), 16);
        return b;
    }
    static String hex(byte[] b) {
        if (b == null) return "";
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format("%02x", x));
        return sb.toString();
    }

    void stop(final int kind) {
        if (kind == WD) {
            stopWifiDirect();
            return;
        }
        bleOn = false;
        ui.post(() -> {
            try {
                if (scanner != null) scanner.stopScan(scanCallback);
                if (advertiser != null) advertiser.stopAdvertising(advertiseCallback);
                for (BluetoothGatt g : asCentral.values()) { g.disconnect(); g.close(); }
                asCentral.clear();
                if (server != null) server.close();
                server = null;
                asPeripheral.clear();
                synchronized (queue) { queue.clear(); queued.clear(); busy.clear(); }
            } catch (SecurityException e) {
                // permission withdrawn meanwhile: nothing more to stop
            }
        });
    }

    private final AdvertiseCallback advertiseCallback = new AdvertiseCallback() {
        @Override public void onStartFailure(int code) {
            emit(ERROR, BLE, "", "", "", "could not advertise over Bluetooth (" + code + ")", 0, null);
        }
    };

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int type, ScanResult r) {
            BluetoothDevice d = r.getDevice();
            String addr = d.getAddress();
            long now = System.currentTimeMillis();
            Long last = lastFound.get(addr);
            if (last != null && now - last < 3000) return; // a found per device every few seconds is plenty
            lastFound.put(addr, now);
            ScanRecord rec = r.getScanRecord();
            String tag = rec == null ? "" : hex(rec.getServiceData(new ParcelUuid(SERVICE)));
            String name = "";
            try { name = d.getName(); } catch (SecurityException ignored) { }
            emit(FOUND, BLE, addr, name, tag, "", r.getRssi(), null);
        }
        @Override public void onScanFailed(int code) {
            emit(ERROR, BLE, "", "", "", "could not scan for Bluetooth devices (" + code + ")", 0, null);
        }
    };

    boolean connect(final int kind, final String peer) {
        if (kind == WD) return connectWifiDirect(peer);
        final BluetoothAdapter ba = adapter();
        if (ba == null || !bleOn) return false;
        ui.post(() -> {
            if (asCentral.containsKey(peer) || asPeripheral.containsKey(peer)) return; // already one stream
            try {
                BluetoothDevice d = ba.getRemoteDevice(peer);
                BluetoothGatt g = d.connectGatt(act, false, centralCallback, BluetoothDevice.TRANSPORT_LE);
                if (g != null) asCentral.put(peer, g);
            } catch (Exception e) {
                emit(ERROR, BLE, peer, "", "", "could not connect: " + e.getMessage(), 0, null);
            }
        });
        return true;
    }

    boolean disconnect(final int kind, final String peer) {
        if (kind == WD) {
            stopGroup();
            return true;
        }
        ui.post(() -> {
            try {
                BluetoothGatt g = asCentral.remove(peer);
                if (g != null) { g.disconnect(); g.close(); }
                BluetoothDevice d = asPeripheral.remove(peer);
                if (d != null && server != null) server.cancelConnection(d);
            } catch (SecurityException ignored) { }
            gone(peer, "disconnected here");
        });
        return true;
    }

    private void gone(String peer, String why) {
        synchronized (queue) { queue.remove(peer); queued.remove(peer); busy.remove(peer); }
        mtu.remove(peer);
        emit(DISCONNECTED, BLE, peer, "", "", why, 0, null);
    }

    // as a central: connect, bigger MTU, find the service, subscribe to TX
    private final BluetoothGattCallback centralCallback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt g, int status, int state) {
            String peer = g.getDevice().getAddress();
            try {
                if (state == BluetoothProfile.STATE_CONNECTED) {
                    if (!g.requestMtu(517)) g.discoverServices();
                } else if (state == BluetoothProfile.STATE_DISCONNECTED) {
                    g.close();
                    ui.post(() -> asCentral.remove(peer));
                    gone(peer, "the link dropped (" + status + ")");
                }
            } catch (SecurityException e) {
                emit(ERROR, BLE, peer, "", "", "Bluetooth was not allowed", 0, null);
            }
        }
        @Override public void onMtuChanged(BluetoothGatt g, int m, int status) {
            mtu.put(g.getDevice().getAddress(), m);
            try { g.discoverServices(); } catch (SecurityException ignored) { }
        }
        @Override public void onServicesDiscovered(BluetoothGatt g, int status) {
            BluetoothGattService svc = g.getService(SERVICE);
            if (svc == null) {
                try { g.disconnect(); } catch (SecurityException ignored) { }
                return;
            }
            BluetoothGattCharacteristic tx = svc.getCharacteristic(TX);
            try {
                g.setCharacteristicNotification(tx, true);
                BluetoothGattDescriptor cccd = tx.getDescriptor(CCCD);
                writeDescriptor(g, cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            } catch (SecurityException ignored) { }
        }
        @Override public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor d, int status) {
            emit(CONNECTED, BLE, g.getDevice().getAddress(), "", "", "central", 0, null);
        }
        @SuppressWarnings("deprecation")
        @Override public void onCharacteristicChanged(BluetoothGatt g, BluetoothGattCharacteristic c) {
            if (Build.VERSION.SDK_INT < 33 && TX.equals(c.getUuid()))
                emit(DATA, BLE, g.getDevice().getAddress(), "", "", "", 0, c.getValue());
        }
        @Override public void onCharacteristicChanged(BluetoothGatt g, BluetoothGattCharacteristic c, byte[] value) {
            if (Build.VERSION.SDK_INT >= 33 && TX.equals(c.getUuid()))
                emit(DATA, BLE, g.getDevice().getAddress(), "", "", "", 0, value);
        }
        @Override public void onCharacteristicWrite(BluetoothGatt g, BluetoothGattCharacteristic c, int status) {
            sent(g.getDevice().getAddress());
        }
    };

    @SuppressWarnings("deprecation")
    private static void writeDescriptor(BluetoothGatt g, BluetoothGattDescriptor d, byte[] v) {
        if (Build.VERSION.SDK_INT >= 33) g.writeDescriptor(d, v);
        else { d.setValue(v); g.writeDescriptor(d); }
    }

    // as a peripheral: someone writes RX, subscribes to TX
    private final BluetoothGattServerCallback serverCallback = new BluetoothGattServerCallback() {
        @Override public void onConnectionStateChange(BluetoothDevice d, int status, int state) {
            if (state == BluetoothProfile.STATE_DISCONNECTED && asPeripheral.remove(d.getAddress()) != null)
                gone(d.getAddress(), "the link dropped (" + status + ")");
        }
        @Override public void onMtuChanged(BluetoothDevice d, int m) { mtu.put(d.getAddress(), m); }
        @Override public void onCharacteristicWriteRequest(BluetoothDevice d, int id, BluetoothGattCharacteristic c,
                                                           boolean prepared, boolean needsResponse, int offset, byte[] value) {
            try {
                if (needsResponse && server != null) server.sendResponse(d, id, BluetoothGatt.GATT_SUCCESS, 0, null);
            } catch (SecurityException ignored) { }
            if (RX.equals(c.getUuid())) emit(DATA, BLE, d.getAddress(), "", "", "", 0, value);
        }
        @Override public void onDescriptorWriteRequest(BluetoothDevice d, int id, BluetoothGattDescriptor desc,
                                                       boolean prepared, boolean needsResponse, int offset, byte[] value) {
            try {
                if (needsResponse && server != null) server.sendResponse(d, id, BluetoothGatt.GATT_SUCCESS, 0, null);
            } catch (SecurityException ignored) { }
            if (CCCD.equals(desc.getUuid())) {
                asPeripheral.put(d.getAddress(), d);
                emit(CONNECTED, BLE, d.getAddress(), "", "", "peripheral", 0, null);
            }
        }
        @Override public void onNotificationSent(BluetoothDevice d, int status) { sent(d.getAddress()); }
    };

    // ── sending: cut to the MTU, one write in flight per peer ────────────────
    boolean send(String peer, byte[] bytes) {
        synchronized (queue) {
            int q = queued.containsKey(peer) ? queued.get(peer) : 0;
            if (!asCentral.containsKey(peer) && !asPeripheral.containsKey(peer)) return false;
            if (q + bytes.length > BACKLOG_LIMIT) return false;
            int m = mtu.containsKey(peer) ? mtu.get(peer) : 23;
            int chunk = Math.max(20, m - 3);
            ArrayDeque<byte[]> dq = queue.get(peer);
            if (dq == null) { dq = new ArrayDeque<>(); queue.put(peer, dq); }
            for (int i = 0; i < bytes.length; i += chunk) {
                int n = Math.min(chunk, bytes.length - i);
                byte[] part = new byte[n];
                System.arraycopy(bytes, i, part, 0, n);
                dq.add(part);
            }
            queued.put(peer, q + bytes.length);
        }
        ui.post(() -> next(peer));
        return true;
    }

    int backlog(String peer) {
        synchronized (queue) { return queued.containsKey(peer) ? queued.get(peer) : 0; }
    }

    private void sent(String peer) {
        synchronized (queue) { busy.put(peer, false); }
        ui.post(() -> next(peer));
    }

    @SuppressWarnings("deprecation")
    private void next(String peer) {
        byte[] part;
        synchronized (queue) {
            Boolean b = busy.get(peer);
            if (b != null && b) return;
            ArrayDeque<byte[]> dq = queue.get(peer);
            if (dq == null || dq.isEmpty()) return;
            part = dq.poll();
            queued.put(peer, Math.max(0, queued.get(peer) - part.length));
            busy.put(peer, true);
        }
        try {
            BluetoothGatt g = asCentral.get(peer);
            if (g != null) {
                BluetoothGattService svc = g.getService(SERVICE);
                BluetoothGattCharacteristic rx = svc == null ? null : svc.getCharacteristic(RX);
                if (rx == null) { busy.put(peer, false); return; }
                if (Build.VERSION.SDK_INT >= 33) {
                    g.writeCharacteristic(rx, part, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                } else {
                    rx.setValue(part);
                    rx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                    g.writeCharacteristic(rx);
                }
                return;
            }
            BluetoothDevice d = asPeripheral.get(peer);
            if (d != null && server != null) {
                if (Build.VERSION.SDK_INT >= 33) {
                    server.notifyCharacteristicChanged(d, txChar, false, part);
                } else {
                    txChar.setValue(part);
                    server.notifyCharacteristicChanged(d, txChar, false);
                }
                return;
            }
            synchronized (queue) { busy.put(peer, false); }
        } catch (SecurityException e) {
            synchronized (queue) { busy.put(peer, false); }
            emit(ERROR, BLE, peer, "", "", "Bluetooth was not allowed", 0, null);
        }
    }

    // ── Wi-Fi Direct ────────────────────────────────────────────────────────
    private WifiP2pManager p2p;
    private WifiP2pManager.Channel channel;
    private BroadcastReceiver p2pReceiver;
    private boolean groupUp = false;
    private final Runnable rediscover = new Runnable() {
        @Override public void run() {
            if (!wdOn) return;
            discoverServices();
            ui.postDelayed(this, 20000); // discovery stops on its own after a while
        }
    };

    private boolean startWifiDirect(final String tag) {
        ui.post(() -> {
            try {
                p2p = (WifiP2pManager) act.getSystemService(Context.WIFI_P2P_SERVICE);
                if (p2p == null) {
                    emit(ERROR, WD, "", "", "", "this phone has no Wi-Fi Direct", 0, null);
                    return;
                }
                channel = p2p.initialize(act, Looper.getMainLooper(), null);
                IntentFilter f = new IntentFilter();
                f.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
                p2pReceiver = new BroadcastReceiver() {
                    @Override public void onReceive(Context c, Intent i) {
                        if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(i.getAction())) checkGroup();
                    }
                };
                if (Build.VERSION.SDK_INT >= 33) act.registerReceiver(p2pReceiver, f, Context.RECEIVER_NOT_EXPORTED);
                else act.registerReceiver(p2pReceiver, f);
                Map<String, String> txt = new HashMap<>();
                txt.put("tag", tag);
                WifiP2pDnsSdServiceInfo info = WifiP2pDnsSdServiceInfo.newInstance("maiz", "_maiz._udp", txt);
                p2p.addLocalService(channel, info, null);
                p2p.setDnsSdResponseListeners(channel, (instance, type, device) -> { },
                        (domain, record, device) -> {
                            String t = record == null ? "" : record.get("tag");
                            emit(FOUND, WD, device.deviceAddress, device.deviceName, t, "", 0, null);
                        });
                p2p.addServiceRequest(channel, WifiP2pDnsSdServiceRequest.newInstance(), null);
                wdOn = true;
                ui.post(rediscover);
            } catch (SecurityException e) {
                emit(ERROR, WD, "", "", "", "Wi-Fi Direct was not allowed: " + e.getMessage(), 0, null);
            } catch (Exception e) {
                emit(ERROR, WD, "", "", "", "Wi-Fi Direct did not start: " + e.getMessage(), 0, null);
            }
        });
        return true;
    }

    private void discoverServices() {
        try {
            if (p2p != null) p2p.discoverServices(channel, null);
        } catch (SecurityException ignored) { }
    }

    private boolean connectWifiDirect(final String peer) {
        if (!wdOn || p2p == null) return false;
        ui.post(() -> {
            try {
                WifiP2pConfig cfg = new WifiP2pConfig();
                cfg.deviceAddress = peer;
                cfg.wps.setup = WpsInfo.PBC;
                p2p.connect(channel, cfg, new WifiP2pManager.ActionListener() {
                    @Override public void onSuccess() { }
                    @Override public void onFailure(int reason) {
                        emit(ERROR, WD, peer, "", "", "Wi-Fi Direct could not connect (" + reason + ")", 0, null);
                    }
                });
            } catch (SecurityException e) {
                emit(ERROR, WD, peer, "", "", "Wi-Fi Direct was not allowed", 0, null);
            }
        });
        return true;
    }

    private void checkGroup() {
        if (p2p == null) return;
        p2p.requestConnectionInfo(channel, (WifiP2pInfo info) -> {
            if (info != null && info.groupFormed && info.groupOwnerAddress != null) {
                groupUp = true;
                String ip = info.groupOwnerAddress.getHostAddress();
                emit(GROUP, WD, "", "", "", (info.isGroupOwner ? "owner " : "client ") + ip, 0, null);
            } else if (groupUp) {
                groupUp = false;
                emit(GROUP_GONE, WD, "", "", "", "", 0, null);
            }
        });
    }

    private void stopGroup() {
        ui.post(() -> {
            if (p2p != null) p2p.removeGroup(channel, null);
        });
    }

    private void stopWifiDirect() {
        wdOn = false;
        ui.post(() -> {
            try {
                if (p2p != null) {
                    p2p.removeGroup(channel, null);
                    p2p.clearLocalServices(channel, null);
                    p2p.clearServiceRequests(channel, null);
                    p2p.stopPeerDiscovery(channel, null);
                }
                if (p2pReceiver != null) act.unregisterReceiver(p2pReceiver);
            } catch (Exception ignored) { }
            p2pReceiver = null;
        });
    }
}
