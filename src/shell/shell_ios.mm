/* shell_ios.mm — a portable App (voidmaiz/app.hpp) on iOS: UIKit, Metal, and
 * the library's touch gate (2026-10-17). okf/concepts/ios.md is the design.
 *
 * WRITTEN ON WINDOWS, COMPILED BY CI. No machine in the family runs macOS, so
 * this file is compiled and run by the `ios` job in .github/workflows/ci.yml
 * (a GitHub macOS runner: Xcode, the iOS Simulator, a screenshot of the app).
 * Until that job is green, treat every line here as reasoned, not witnessed.
 *
 * What it does, and the Android line it mirrors:
 *   - UIApplicationMain + a scene delegate (the UIScene life cycle Apple now
 *     requires) + one view controller whose view is an MTKView;
 *   - Metal through ImGui's vendored Metal backend (OpenGL ES is deprecated);
 *   - fingers through the touch gate with UIKit's coalesced samples, the way
 *     Android's historical samples go in (a fling's speed depends on them);
 *   - Back is the edge swipe (the gate does it: Device::edge_back is true) and
 *     an unclaimed Back does nothing: an iOS app never exits;
 *   - the safe area from safeAreaInsets, every frame (it is cheap here);
 *   - Background on sceneDidEnterBackground, which is where an app saves, and
 *     the view PAUSES there: iOS ends an app that draws in the background;
 *   - the keyboard: a hidden UIKeyInput view under the textinput.hpp pivot
 *     (stage 1: no marked text, so no CJK composition and no autocorrect
 *     context yet; accents and emoji work, because they arrive as insertText);
 *   - PHPicker for photos (no permission, as on Android), the document picker
 *     for files and for saving, "Open in" through the scene, the share sheet,
 *     haptics, openURL, NSURLSession for http, CoreLocation for location.
 *
 * Units: framebuffer pixels everywhere, density = contentScaleFactor (px per
 * point), exactly as on Android (px per dp). */
#import <CoreLocation/CoreLocation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <PhotosUI/PhotosUI.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "frame.hpp"

#include "voidmaiz/location.hpp"
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/textinput.hpp"
#include "voidmaiz/update.hpp" // safe_url

#include "imgui.h"
#include "imgui_impl_metal.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

extern "C" const char* const maiz_app_name;
extern "C" const char* const maiz_app_version;

@class MaizKeyInput;
@class MaizLocationDelegate;

// ── the view controller's interface (the Device calls into it) ────────────────

@interface MaizViewController : UIViewController <MTKViewDelegate, UIDocumentPickerDelegate,
                                                  PHPickerViewControllerDelegate>
- (void)lifecycle:(maiz::AppEvent)e;
- (void)openedURL:(NSURL*)url;
- (BOOL)pickDocument:(int)request mime:(const std::string&)mime;
- (BOOL)saveDocument:(int)request path:(const std::string&)path;
- (BOOL)shareItems:(NSArray*)items;
@end

static __weak MaizViewController* g_vc = nil;

// ── the keyboard: UIKeyInput under the textinput.hpp pivot ────────────────────

@interface MaizKeyInput : UIView <UIKeyInput>
@property(nonatomic) UIKeyboardType keyboardType;
@property(nonatomic) UIReturnKeyType returnKeyType;
@property(nonatomic) UITextAutocorrectionType autocorrectionType;
@property(nonatomic) UITextAutocapitalizationType autocapitalizationType;
@property(nonatomic) UITextSpellCheckingType spellCheckingType;
@property(nonatomic, getter=isSecureTextEntry) BOOL secureTextEntry;
@property(nonatomic) void* owner; // IOSTextInput*
@end

namespace {

/* The keyboard edits a MIRROR of the field (the pivot's EditingState), and
 * after every edit the whole state goes to the glue, which turns it into the
 * keystrokes ImGui's field accepts: the same split Android's MaizActivity
 * makes. Everything runs on the main thread (UIKit's and the frame's). */
struct IOSTextInput : maiz::TextInputPlatform {
    MaizKeyInput* view = nil;
    std::mutex mu;
    std::vector<maiz::TextInputEvent> events;
    maiz::EditingState mirror;
    bool multiline = false;
    float covered = 0.0f;

    void push_state() {
        maiz::TextInputEvent e;
        e.type = maiz::TextInputEvent::Type::State;
        e.state = mirror;
        std::lock_guard<std::mutex> lock(mu);
        events.push_back(e);
    }
    void push(maiz::TextInputEvent::Type t, maiz::TextKey k = maiz::TextKey::Enter) {
        maiz::TextInputEvent e;
        e.type = t;
        e.key = k;
        std::lock_guard<std::mutex> lock(mu);
        events.push_back(e);
    }

    void show(maiz::InputKind kind, maiz::InputAction action, const maiz::EditingState& st) override;
    void update(const maiz::EditingState& st) override { mirror = st; }
    void hide() override;
    void poll(std::vector<maiz::TextInputEvent>& out) override {
        std::lock_guard<std::mutex> lock(mu);
        out.insert(out.end(), events.begin(), events.end());
        events.clear();
    }
    float covered_px() const override { return covered; }

    void insert(const std::string& t) {
        if (t == "\n" && !multiline) {
            push(maiz::TextInputEvent::Type::Action);
            return;
        }
        const int n = (int)mirror.text.size();
        const int a = std::clamp(std::min(mirror.sel_start, mirror.sel_end), 0, n);
        const int b = std::clamp(std::max(mirror.sel_start, mirror.sel_end), 0, n);
        mirror.text.replace((size_t)a, (size_t)(b - a), t);
        mirror.sel_start = mirror.sel_end = a + (int)t.size();
        mirror.compose_start = mirror.compose_end = -1;
        push_state();
    }
    void backspace() {
        const int n = (int)mirror.text.size();
        int a = std::clamp(std::min(mirror.sel_start, mirror.sel_end), 0, n);
        const int b = std::clamp(std::max(mirror.sel_start, mirror.sel_end), 0, n);
        if (a != b) {
            mirror.text.erase((size_t)a, (size_t)(b - a));
        } else if (a > 0) {
            const int p = maiz::utf8_floor(mirror.text, a - 1);
            mirror.text.erase((size_t)p, (size_t)(a - p));
            a = p;
        } else {
            push(maiz::TextInputEvent::Type::Key, maiz::TextKey::Backspace); // nothing here; the field decides
            return;
        }
        mirror.sel_start = mirror.sel_end = a;
        push_state();
    }
};

} // namespace

@implementation MaizKeyInput
- (BOOL)canBecomeFirstResponder {
    return YES;
}
- (BOOL)hasText {
    return YES; // so a backspace in an empty mirror still reaches us, and the field
}
- (void)insertText:(NSString*)text {
    if (self.owner) static_cast<IOSTextInput*>(self.owner)->insert(text.UTF8String ? text.UTF8String : "");
}
- (void)deleteBackward {
    if (self.owner) static_cast<IOSTextInput*>(self.owner)->backspace();
}
@end

namespace {

void IOSTextInput::show(maiz::InputKind kind, maiz::InputAction action, const maiz::EditingState& st) {
    using K = maiz::InputKind;
    mirror = st;
    multiline = kind == K::Multiline || action == maiz::InputAction::Newline;
    UIKeyboardType kb = UIKeyboardTypeDefault;
    UITextAutocorrectionType ac = UITextAutocorrectionTypeDefault;
    UITextAutocapitalizationType cap = UITextAutocapitalizationTypeSentences;
    switch (kind) {
    case K::Name: ac = UITextAutocorrectionTypeNo; cap = UITextAutocapitalizationTypeWords; break;
    case K::Number: kb = UIKeyboardTypeDecimalPad; break;
    case K::Phone: kb = UIKeyboardTypePhonePad; break;
    case K::Email: kb = UIKeyboardTypeEmailAddress; ac = UITextAutocorrectionTypeNo; cap = UITextAutocapitalizationTypeNone; break;
    case K::Url: kb = UIKeyboardTypeURL; ac = UITextAutocorrectionTypeNo; cap = UITextAutocapitalizationTypeNone; break;
    case K::Search: kb = UIKeyboardTypeWebSearch; break;
    case K::Code: kb = UIKeyboardTypeASCIICapable; ac = UITextAutocorrectionTypeNo; cap = UITextAutocapitalizationTypeNone; break;
    default: break;
    }
    UIReturnKeyType ret = UIReturnKeyDone;
    switch (action) {
    case maiz::InputAction::Next: ret = UIReturnKeyNext; break;
    case maiz::InputAction::Go: ret = UIReturnKeyGo; break;
    case maiz::InputAction::Search: ret = UIReturnKeySearch; break;
    case maiz::InputAction::Send: ret = UIReturnKeySend; break;
    case maiz::InputAction::Newline: ret = UIReturnKeyDefault; break;
    default: break;
    }
    view.keyboardType = kb;
    view.autocorrectionType = ac;
    view.autocapitalizationType = cap;
    view.spellCheckingType = ac == UITextAutocorrectionTypeNo ? UITextSpellCheckingTypeNo : UITextSpellCheckingTypeDefault;
    view.returnKeyType = ret;
    if (view.isFirstResponder) [view reloadInputViews];
    else [view becomeFirstResponder];
}

void IOSTextInput::hide() {
    if (view.isFirstResponder) [view resignFirstResponder];
}

} // namespace

// ── location: CoreLocation under location.hpp ─────────────────────────────────

@interface MaizLocationDelegate : NSObject <CLLocationManagerDelegate>
@property(nonatomic) void* owner; // IOSLocation*
@end

namespace {

struct IOSLocation : maiz::LocationPlatform {
    CLLocationManager* mgr = nil;
    MaizLocationDelegate* del = nil;
    bool on = false;
    bool has = false;
    CLLocation* last = nil;

    IOSLocation();
    maiz::LocationAccess access() override {
        switch (mgr.authorizationStatus) {
        case kCLAuthorizationStatusNotDetermined: return on ? maiz::LocationAccess::Asking : maiz::LocationAccess::Unasked;
        case kCLAuthorizationStatusRestricted:
        case kCLAuthorizationStatusDenied: return maiz::LocationAccess::Denied;
        default: break;
        }
        return mgr.accuracyAuthorization == CLAccuracyAuthorizationFullAccuracy ? maiz::LocationAccess::Precise
                                                                                : maiz::LocationAccess::Approximate;
    }
    /* From a person's tap only (location.hpp): the one call that may prompt. */
    bool request(bool precise) override {
        mgr.desiredAccuracy = precise ? kCLLocationAccuracyBest : kCLLocationAccuracyKilometer;
        on = true;
        if (mgr.authorizationStatus == kCLAuthorizationStatusNotDetermined) [mgr requestWhenInUseAuthorization];
        [mgr startUpdatingLocation];
        return true;
    }
    void stop() override {
        on = false;
        [mgr stopUpdatingLocation];
    }
    bool latest(maiz::LocationFix& out) override {
        if (!on || !last) return false;
        out.lat = last.coordinate.latitude;
        out.lon = last.coordinate.longitude;
        out.accuracy_m = (float)std::max(0.0, last.horizontalAccuracy);
        out.age_s = -[last.timestamp timeIntervalSinceNow];
        out.provider = "core-location";
        return true;
    }
    bool following() const override { return on; }
};

} // namespace

@implementation MaizLocationDelegate
- (void)locationManager:(CLLocationManager*)manager didUpdateLocations:(NSArray<CLLocation*>*)locations {
    if (self.owner && locations.count) static_cast<IOSLocation*>(self.owner)->last = locations.lastObject;
}
- (void)locationManagerDidChangeAuthorization:(CLLocationManager*)manager {
    // granted while following: start (the prompt answered after request())
    if (self.owner && static_cast<IOSLocation*>(self.owner)->on) [manager startUpdatingLocation];
}
- (void)locationManager:(CLLocationManager*)manager didFailWithError:(NSError*)error {
}
@end

namespace {

IOSLocation::IOSLocation() {
    mgr = [[CLLocationManager alloc] init];
    del = [[MaizLocationDelegate alloc] init];
    del.owner = this;
    mgr.delegate = del;
}

// ── the Device ────────────────────────────────────────────────────────────────

bool safe_scheme(const std::string& url) {
    for (const char* s : {"https://", "http://", "tel:", "mailto:", "sms:", "maps:"})
        if (url.rfind(s, 0) == 0) return url.find_first_of("\"'`\r\n") == std::string::npos;
    return false;
}

fs::path search_path(NSSearchPathDirectory which) {
    NSString* p = NSSearchPathForDirectoriesInDomains(which, NSUserDomainMask, YES).firstObject;
    fs::path out = p ? fs::path(p.UTF8String) : fs::temp_directory_path();
    std::error_code ec;
    fs::create_directories(out, ec);
    return out;
}

struct IOSDevice : maiz::Device {
    float dens = 3.0f;
    maiz::SafeArea safe;
    std::unique_ptr<IOSTextInput> keyboard;
    id<MTLDevice> gpu = nil;

    maiz::Platform platform() const override { return maiz::Platform::iOS; }
    std::string model() const override {
        UIDevice* d = UIDevice.currentDevice;
        return std::string(d.model.UTF8String) + " (iOS " + d.systemVersion.UTF8String + ")";
    }
    float density() const override { return dens; }
    maiz::SafeArea safe_area() const override { return safe; }
    /* Application Support: private, kept, backed up, never shown in Files. */
    fs::path data_dir() const override { return search_path(NSApplicationSupportDirectory); }
    fs::path cache_dir() const override { return search_path(NSCachesDirectory); }
    bool read_asset(const std::string& name, std::string& bytes) const override {
        NSString* file = [@(name.c_str()) lastPathComponent];
        NSString* path = [NSBundle.mainBundle pathForResource:file ofType:nil];
        NSData* data = path ? [NSData dataWithContentsOfFile:path] : nil;
        if (!data) return false;
        bytes.assign((const char*)data.bytes, data.length);
        return true;
    }
    maiz::TextInputPlatform* text_input() override { return keyboard.get(); }
    maiz::Http http(const std::string& ua) override {
        return maiz::ios_http(ua.empty() ? std::string(maiz_app_name) + "/" + maiz_app_version + " (iOS)" : ua);
    }
    bool open_url(const std::string& url) override {
        if (!safe_scheme(url)) return false;
        NSURL* u = [NSURL URLWithString:@(url.c_str())];
        if (!u) return false;
        [UIApplication.sharedApplication openURL:u options:@{} completionHandler:nil];
        return true;
    }
    bool share(const maiz::ShareItem& it) override {
        NSMutableArray* items = [NSMutableArray array];
        if (!it.text.empty()) [items addObject:@(it.text.c_str())];
        if (!it.url.empty())
            if (NSURL* u = [NSURL URLWithString:@(it.url.c_str())]) [items addObject:u];
        if (!it.file.empty()) [items addObject:[NSURL fileURLWithPath:@(it.file.c_str())]];
        return items.count && [g_vc shareItems:items];
    }
    void haptic(maiz::Haptic h) override {
        switch (h) {
        case maiz::Haptic::Tap: [[[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight] impactOccurred]; break;
        case maiz::Haptic::Select: [[[UISelectionFeedbackGenerator alloc] init] selectionChanged]; break;
        case maiz::Haptic::Success:
            [[[UINotificationFeedbackGenerator alloc] init] notificationOccurred:UINotificationFeedbackTypeSuccess];
            break;
        case maiz::Haptic::Warning:
            [[[UINotificationFeedbackGenerator alloc] init] notificationOccurred:UINotificationFeedbackTypeWarning];
            break;
        case maiz::Haptic::Error:
            [[[UINotificationFeedbackGenerator alloc] init] notificationOccurred:UINotificationFeedbackTypeError];
            break;
        }
    }
    bool pick_document(int request, const std::string& mime) override { return [g_vc pickDocument:request mime:mime]; }
    bool save_document(int request, const std::string& src, const std::string&, const std::string&) override {
        return [g_vc saveDocument:request path:src];
    }
    maiz::Texture create_texture(const unsigned char* rgba, int w, int h) override {
        MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                     width:(NSUInteger)w
                                                                                    height:(NSUInteger)h
                                                                                 mipmapped:NO];
        d.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> t = [gpu newTextureWithDescriptor:d];
        if (!t) return {};
        [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:rgba
             bytesPerRow:(NSUInteger)w * 4];
        // ImGui's Metal backend reads an ImTextureID as a bridged id<MTLTexture>;
        // the retain is ours until destroy_texture
        return {(std::uint64_t)(uintptr_t)(__bridge_retained void*)t, w, h};
    }
    void destroy_texture(maiz::Texture& t) override {
        if (t.id) {
            id<MTLTexture> gone = (__bridge_transfer id<MTLTexture>)(void*)(uintptr_t)t.id;
            gone = nil;
        }
        t = {};
    }
    // quit(): nothing. can_quit() is false on iOS; Apple reads a self-exit as a crash.
};

/* A file the system handed us (a pick, an "Open in"), copied into our own
 * folder under its own name made unique, so native code only ever sees an
 * ordinary file it owns (documents.hpp's rule, Android's too). */
maiz::DocumentResult copy_in(int request, NSURL* url, const fs::path& dir, NSString* name_hint = nil) {
    maiz::DocumentResult r;
    r.request = request;
    std::error_code ec;
    fs::create_directories(dir, ec);
    const BOOL scoped = [url startAccessingSecurityScopedResource];
    NSString* name = name_hint.length ? name_hint : url.lastPathComponent;
    std::string base = name.length ? name.UTF8String : "picked";
    for (char& c : base)
        if (c == '/' || c == '\\' || c == ':') c = '_';
    while (!base.empty() && base[0] == '.') base.erase(0, 1);
    if (base.empty()) base = "picked";
    fs::path dest = dir / base;
    for (int i = 2; fs::exists(dest, ec); ++i)
        dest = dir / (fs::path(base).stem().string() + "-" + std::to_string(i) + fs::path(base).extension().string());
    NSError* err = nil;
    const BOOL ok = [NSFileManager.defaultManager copyItemAtURL:url
                                                         toURL:[NSURL fileURLWithPath:@(dest.string().c_str())]
                                                         error:&err];
    if (scoped) [url stopAccessingSecurityScopedResource];
    if (ok) {
        r.status = "ok";
        r.path = dest.string();
        r.name = base;
    } else {
        r.status = "error";
        r.name = err ? err.localizedDescription.UTF8String : "the file could not be copied";
    }
    return r;
}

} // namespace

// ── http: NSURLSession under http.hpp ─────────────────────────────────────────

maiz::Http maiz::ios_http(const std::string& user_agent) {
    return [user_agent](const std::string& url, const fs::path& to) {
        HttpResult r;
        if (!maiz::update::safe_url(url)) {
            r.error = "refusing \"" + url + "\": not a plain https URL";
            return r;
        }
        NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@(url.c_str())]];
        req.timeoutInterval = 60;
        if (!user_agent.empty()) [req setValue:@(user_agent.c_str()) forHTTPHeaderField:@"User-Agent"];
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        __block HttpResult* out = &r;
        const std::string dest = to.string();
        NSURLSessionDownloadTask* task = [NSURLSession.sharedSession
            downloadTaskWithRequest:req
                  completionHandler:^(NSURL* location, NSURLResponse* response, NSError* error) {
                    if ([response isKindOfClass:NSHTTPURLResponse.class])
                        out->status = (int)((NSHTTPURLResponse*)response).statusCode;
                    if (error || !location) {
                        out->error = error ? error.localizedDescription.UTF8String : "no file arrived";
                    } else if (out->status < 200 || out->status > 299) {
                        out->error = "HTTP " + std::to_string(out->status);
                    } else {
                        NSURL* target = [NSURL fileURLWithPath:@(dest.c_str())];
                        [NSFileManager.defaultManager removeItemAtURL:target error:nil];
                        NSError* moveErr = nil;
                        out->ok = [NSFileManager.defaultManager moveItemAtURL:location toURL:target error:&moveErr];
                        if (!out->ok) out->error = moveErr.localizedDescription.UTF8String;
                    }
                    dispatch_semaphore_signal(done);
                  }];
        [task resume];
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER); // a worker thread's wait (http.hpp)
        return r;
    };
}

// ── the view controller ───────────────────────────────────────────────────────

@implementation MaizViewController {
    id<MTLCommandQueue> _queue;
    IOSDevice _dev;
    std::unique_ptr<maiz::shell::Runner> _runner;
    std::vector<maiz::DocumentResult> _early; // arrived before the app started
    MaizKeyInput* _keyInput;
    std::unordered_map<const void*, int> _touchIds;
    double _lastTime;
    int _pickRequest;
    int _saveRequest;
    __weak UIDocumentPickerViewController* _exportPicker;
    BOOL _lightContent;
}

- (void)loadView {
    id<MTLDevice> gpu = MTLCreateSystemDefaultDevice();
    MTKView* v = [[MTKView alloc] initWithFrame:CGRectZero device:gpu];
    v.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    v.multipleTouchEnabled = YES;
    v.preferredFramesPerSecond = 60;
    v.delegate = self;
    self.view = v;
    _queue = [gpu newCommandQueue];
    _dev.gpu = gpu;
    _lastTime = 0;
    _lightContent = YES;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // a phone's layout is its own, not imgui.ini's
    io.BackendPlatformName = "voidmaiz_ios";
    ImGui_ImplMetal_Init(gpu);
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
        static std::string text;
        NSString* s = UIPasteboard.generalPasteboard.string;
        text = s ? s.UTF8String : "";
        return text.c_str();
    };
    pio.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
        UIPasteboard.generalPasteboard.string = @(text ? text : "");
    };

    // the keyboard's anchor: tiny, transparent, never in the way of a touch
    _keyInput = [[MaizKeyInput alloc] initWithFrame:CGRectMake(0, 0, 1, 1)];
    _keyInput.alpha = 0.0;
    _keyInput.userInteractionEnabled = NO;
    [v addSubview:_keyInput];
    _dev.keyboard = std::make_unique<IOSTextInput>();
    _dev.keyboard->view = _keyInput;
    _keyInput.owner = _dev.keyboard.get();

    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(keyboardFrame:)
                                               name:UIKeyboardWillChangeFrameNotification
                                             object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(keyboardHidden:)
                                               name:UIKeyboardWillHideNotification
                                             object:nil];
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
    return _lightContent ? UIStatusBarStyleLightContent : UIStatusBarStyleDarkContent;
}

- (void)keyboardFrame:(NSNotification*)note {
    CGRect end = [note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGRect local = [self.view convertRect:end fromCoordinateSpace:self.view.window.screen.coordinateSpace];
    const CGFloat covered = std::max<CGFloat>(0, CGRectGetMaxY(self.view.bounds) - CGRectGetMinY(local));
    if (_dev.keyboard) _dev.keyboard->covered = (float)(covered * self.view.contentScaleFactor);
}

- (void)keyboardHidden:(NSNotification*)note {
    if (_dev.keyboard) _dev.keyboard->covered = 0.0f;
}

/* Once the view is in a window: its scale is the screen's, its size is real. */
- (void)startIfReady:(MTKView*)view {
    if (_runner) return;
    _dev.dens = (float)view.contentScaleFactor;
    maiz::install_location(std::make_unique<IOSLocation>());
    _runner = std::make_unique<maiz::shell::Runner>(_dev);
    _runner->create();
    _runner->start();
    for (auto& r : _early) _runner->document(r);
    _early.clear();
}

- (void)drawInMTKView:(MTKView*)view {
    [self startIfReady:view];
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)view.drawableSize.width, (float)view.drawableSize.height);
    io.DisplayFramebufferScale = ImVec2(1, 1);
    const double now = CACurrentMediaTime();
    io.DeltaTime = _lastTime > 0 ? (float)std::max(1e-4, now - _lastTime) : 1.0f / 60.0f;
    _lastTime = now;
    const UIEdgeInsets in = view.safeAreaInsets;
    const CGFloat k = view.contentScaleFactor;
    _dev.dens = (float)k;
    _dev.safe = {(float)(in.left * k), (float)(in.top * k), (float)(in.right * k), (float)(in.bottom * k)};

    id<MTLCommandBuffer> cb = [_queue commandBuffer];
    MTLRenderPassDescriptor* rpd = view.currentRenderPassDescriptor;
    if (!rpd) {
        [cb commit];
        return;
    }
    const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    rpd.colorAttachments[0].clearColor = MTLClearColorMake(bg.x, bg.y, bg.z, 1.0);
    rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
    // the status bar's ink follows the app's background, not the system theme
    const BOOL light = (0.299 * bg.x + 0.587 * bg.y + 0.114 * bg.z) < 0.5;
    if (light != _lightContent) {
        _lightContent = light;
        [self setNeedsStatusBarAppearanceUpdate];
    }

    ImGui_ImplMetal_NewFrame(rpd);
    ImGui::NewFrame();
    _runner->frame(); // an unclaimed Back does nothing on iOS: an app never exits
    ImGui::Render();
    id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rpd];
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), cb, enc);
    [enc endEncoding];
    [cb presentDrawable:view.currentDrawable];
    [cb commit];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
}

// ── fingers, through the touch gate ──

- (int)idFor:(UITouch*)t create:(BOOL)create {
    const void* key = (__bridge const void*)t;
    auto it = _touchIds.find(key);
    if (it != _touchIds.end()) return it->second;
    if (!create) return -1;
    int pointer = 0; // the smallest free number, as Android numbers its pointers
    for (;; ++pointer) {
        bool taken = false;
        for (const auto& kv : _touchIds)
            if (kv.second == pointer) taken = true;
        if (!taken) break;
    }
    _touchIds[key] = pointer;
    return pointer;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    const CGFloat k = self.view.contentScaleFactor;
    for (UITouch* t in touches) {
        const CGPoint p = [t locationInView:self.view];
        maiz::touch_gate_down(maiz::default_touch_gate(), [self idFor:t create:YES], (float)(p.x * k), (float)(p.y * k),
                              t.timestamp);
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    const CGFloat k = self.view.contentScaleFactor;
    for (UITouch* t in touches) {
        const int pointer = [self idFor:t create:NO];
        if (pointer < 0) continue;
        // every sample UIKit coalesced into this event, each with its own time
        NSArray<UITouch*>* samples = [event coalescedTouchesForTouch:t];
        if (!samples.count) samples = @[ t ];
        for (UITouch* c in samples) {
            const CGPoint p = [c locationInView:self.view];
            maiz::touch_gate_move(maiz::default_touch_gate(), pointer, (float)(p.x * k), (float)(p.y * k), c.timestamp);
        }
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    const CGFloat k = self.view.contentScaleFactor;
    for (UITouch* t in touches) {
        const int pointer = [self idFor:t create:NO];
        if (pointer < 0) continue;
        const CGPoint p = [t locationInView:self.view];
        maiz::touch_gate_up(maiz::default_touch_gate(), pointer, (float)(p.x * k), (float)(p.y * k), t.timestamp);
        _touchIds.erase((__bridge const void*)t);
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    maiz::touch_gate_cancel(maiz::default_touch_gate());
    _touchIds.clear();
}

// ── life ──

- (void)lifecycle:(maiz::AppEvent)e {
    MTKView* v = (MTKView*)self.view;
    if (e == maiz::AppEvent::Background) v.paused = YES; // no GPU work in the background, ever
    if (e == maiz::AppEvent::Foreground) {
        v.paused = NO;
        _lastTime = 0;
    }
    if (_runner) _runner->event(e);
}

- (void)openedURL:(NSURL*)url {
    maiz::DocumentResult r = copy_in(-1, url, _dev.data_dir() / "incoming");
    if (_runner) _runner->document(r);
    else _early.push_back(r);
}

// ── the system's pickers and share sheet ──

- (BOOL)pickDocument:(int)request mime:(const std::string&)mime {
    _pickRequest = request;
    if (mime.rfind("image/", 0) == 0) {
        // the photo picker: no permission; the person picks, and the pick is the consent
        PHPickerConfiguration* cfg = [[PHPickerConfiguration alloc] init];
        cfg.filter = [PHPickerFilter imagesFilter];
        cfg.selectionLimit = 1;
        PHPickerViewController* p = [[PHPickerViewController alloc] initWithConfiguration:cfg];
        p.delegate = self;
        [self presentViewController:p animated:YES completion:nil];
        return YES;
    }
    UTType* type = mime == "*/*" ? nil : [UTType typeWithMIMEType:@(mime.c_str())];
    UIDocumentPickerViewController* p =
        [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ type ? type : UTTypeItem ] asCopy:YES];
    p.delegate = self;
    [self presentViewController:p animated:YES completion:nil];
    return YES;
}

- (BOOL)saveDocument:(int)request path:(const std::string&)path {
    _saveRequest = request;
    NSURL* src = [NSURL fileURLWithPath:@(path.c_str())];
    UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[ src ] asCopy:YES];
    p.delegate = self;
    _exportPicker = p;
    [self presentViewController:p animated:YES completion:nil];
    return YES;
}

- (void)deliver:(maiz::DocumentResult)r {
    if (_runner) _runner->document(r);
    else _early.push_back(r);
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    if (controller == _exportPicker) {
        maiz::DocumentResult r;
        r.request = _saveRequest;
        r.status = "ok";
        r.path = urls.firstObject ? urls.firstObject.absoluteString.UTF8String : "";
        [self deliver:r];
        return;
    }
    if (!urls.firstObject) return;
    [self deliver:copy_in(_pickRequest, urls.firstObject, _dev.data_dir() / "incoming")];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    maiz::DocumentResult r;
    r.request = controller == _exportPicker ? _saveRequest : _pickRequest;
    r.status = "cancelled";
    [self deliver:r];
}

- (void)picker:(PHPickerViewController*)picker didFinishPicking:(NSArray<PHPickerResult*>*)results {
    [picker dismissViewControllerAnimated:YES completion:nil];
    const int request = _pickRequest;
    if (!results.count) {
        maiz::DocumentResult r;
        r.request = request;
        r.status = "cancelled";
        [self deliver:r];
        return;
    }
    NSItemProvider* provider = results.firstObject.itemProvider;
    NSString* suggested = provider.suggestedName;
    const fs::path dir = _dev.data_dir() / "incoming";
    __weak MaizViewController* weakSelf = self;
    [provider loadFileRepresentationForTypeIdentifier:UTTypeImage.identifier
                                    completionHandler:^(NSURL* url, NSError* error) {
                                      // the file exists only inside this block: copy it now
                                      maiz::DocumentResult r;
                                      if (url) {
                                          NSString* name = suggested.length
                                                               ? [suggested stringByAppendingPathExtension:url.pathExtension]
                                                               : url.lastPathComponent;
                                          r = copy_in(request, url, dir, name);
                                      } else {
                                          r.request = request;
                                          r.status = "error";
                                          r.name = error ? error.localizedDescription.UTF8String : "no picture";
                                      }
                                      dispatch_async(dispatch_get_main_queue(), ^{
                                        [weakSelf deliver:r];
                                      });
                                    }];
}

- (BOOL)shareItems:(NSArray*)items {
    UIActivityViewController* avc = [[UIActivityViewController alloc] initWithActivityItems:items
                                                                      applicationActivities:nil];
    // an iPad presents the sheet as a popover, and a popover with no anchor is a crash
    avc.popoverPresentationController.sourceView = self.view;
    avc.popoverPresentationController.sourceRect =
        CGRectMake(CGRectGetMidX(self.view.bounds), CGRectGetMaxY(self.view.bounds) - 1, 1, 1);
    [self presentViewController:avc animated:YES completion:nil];
    return YES;
}

@end

// ── the scene and the application ─────────────────────────────────────────────

@interface MaizSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

@implementation MaizSceneDelegate
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    if (![scene isKindOfClass:UIWindowScene.class]) return;
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene*)scene];
    MaizViewController* vc = [[MaizViewController alloc] init];
    g_vc = vc;
    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];
    for (UIOpenURLContext* c in options.URLContexts) [vc openedURL:c.URL]; // launched by "Open in"
}
- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)contexts {
    for (UIOpenURLContext* c in contexts) [g_vc openedURL:c.URL];
}
- (void)sceneWillEnterForeground:(UIScene*)scene {
    [g_vc lifecycle:maiz::AppEvent::Foreground];
}
- (void)sceneDidEnterBackground:(UIScene*)scene {
    [g_vc lifecycle:maiz::AppEvent::Background]; // save now: iOS may never say anything again
}
@end

@interface MaizAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation MaizAppDelegate
- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:(UISceneSession*)session
                                   options:(UISceneConnectionOptions*)options {
    UISceneConfiguration* c = [[UISceneConfiguration alloc] initWithName:@"Default" sessionRole:session.role];
    c.delegateClass = MaizSceneDelegate.class;
    return c;
}
- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
    [g_vc lifecycle:maiz::AppEvent::LowMemory];
}
- (void)applicationWillTerminate:(UIApplication*)application {
    [g_vc lifecycle:maiz::AppEvent::Terminating]; // rarely sent: iOS suspends, then kills
}
@end

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(MaizAppDelegate.class));
    }
}
