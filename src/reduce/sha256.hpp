/*
 * src/reduce/sha256.hpp — SHA-256 (FIPS 180-4), arbitrary truncation.
 * INTERNAL to the reduce module; deliberately not in include/voidmaiz/.
 *
 * WHY A HASH LIVES IN A NODE-GRAPH LIBRARY. The reduce contract makes the
 * identity of rule-created agents normative (VoidCore conformance/reduce
 * README §2): a fresh agent MUST be named from the redex — which rule fired on
 * which two agents, plus an ordinal — never from a running counter, a clock or
 * an RNG. Confluence alone promises two reducers the same normal form only *up
 * to renaming*, and a name becomes a rune's `spirit.name`, which `layout.edges`
 * references and tag expressions match. Two peers merging by reduction need the
 * same BYTES, not the same shape.
 *
 * WHY SHA-256 AND NOT THE BLAKE2b THIS REPLACES. We asked Void Core to make the
 * digest normative (2026-08-18) and argued for it in these words: *"'not
 * normative' only buys an implementation that agrees with itself, and the
 * property worth having is two implementations agreeing."* They agreed, and
 * then chose a different digest than the one we had matched byte for byte —
 * SHA-256, shipped normative in 0.2.10.
 *
 * The reason is worth keeping, because it is invisible from C++ and it is why
 * our own preference was wrong: **Void Unity built the second executor in C#**,
 * and .NET/Mono have no BLAKE2b. Vendoring one would have meant carrying a
 * cryptographic primitive whose test vectors are RFC 7693's rather than Void
 * Core's — inside the one project whose charter forbids exactly that. Blessing
 * BLAKE2b would have made "be conformant" and "follow your own charter"
 * mutually exclusive for them. We asked from a language where both digests are
 * equally reachable, so the question looked like it had one free answer; it
 * took a host with only one of the two to show the choice was doing work.
 *
 * Between two digests equally fine at naming, the one every standard library
 * already has is strictly better. BLAKE2b's only advantage was being first.
 *
 * Written from the FIPS publication rather than vendored: ~70 lines against
 * published vectors is a smaller liability than a third-party dependency, and
 * there is no license to carry. Correctness is pinned in reduce_conformance.cpp
 * against FIPS 180-4's own "abc" vector, against the nine key->id vectors of
 * conformance case 16, and against case 15's ids through a real rewrite.
 *
 * Not a security primitive here and not used as one: this is a naming function.
 * 48 bits is a naming digest; collision resistance is not the property bought.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace maiz::reduce::detail {

namespace sha256_impl {

inline constexpr std::uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

inline void compress(std::uint32_t h[8], const unsigned char* block) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (std::uint32_t(block[4 * i]) << 24) |
               (std::uint32_t(block[4 * i + 1]) << 16) |
               (std::uint32_t(block[4 * i + 2]) << 8) |
               std::uint32_t(block[4 * i + 3]);
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

} // namespace sha256_impl

/* The full 32-byte digest of `in`. */
inline std::vector<unsigned char> sha256(std::string_view in) {
    std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                          0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

    /* Padding: the message, 0x80, zeros, then the bit length as 64-bit
     * big-endian. Built into one buffer — a naming key is short, so the
     * streaming form would be complexity with no reader. */
    std::vector<unsigned char> msg(in.begin(), in.end());
    const std::uint64_t bits = static_cast<std::uint64_t>(msg.size()) * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 7; i >= 0; --i)
        msg.push_back(static_cast<unsigned char>((bits >> (i * 8)) & 0xff));

    for (std::size_t off = 0; off < msg.size(); off += 64)
        sha256_impl::compress(h, msg.data() + off);

    std::vector<unsigned char> out(32);
    for (int i = 0; i < 8; ++i) {
        out[4 * i]     = static_cast<unsigned char>((h[i] >> 24) & 0xff);
        out[4 * i + 1] = static_cast<unsigned char>((h[i] >> 16) & 0xff);
        out[4 * i + 2] = static_cast<unsigned char>((h[i] >> 8) & 0xff);
        out[4 * i + 3] = static_cast<unsigned char>(h[i] & 0xff);
    }
    return out;
}

/* The first `bytes` of the digest, as lowercase hex — "the digest's first N
 * bytes written in order", which is what README §2 means by big-endian here. */
inline std::string sha256_hex(std::string_view in, std::size_t bytes) {
    const std::vector<unsigned char> d = sha256(in);
    if (bytes > d.size()) bytes = d.size();
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        out += hex[(d[i] >> 4) & 0xf];
        out += hex[d[i] & 0xf];
    }
    return out;
}

} // namespace maiz::reduce::detail
