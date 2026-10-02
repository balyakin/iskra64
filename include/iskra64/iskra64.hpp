// SPDX-License-Identifier: MIT
// Iskra64 0.1.0-experimental: non-cryptographic, fixed-width integer hashing.
// Mixer: the established SplitMix64 / Stafford Mix13 finalizer. See NOTICE.md.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace iskra64 {
inline constexpr const char* version = "0.1.0-experimental";
namespace detail {
inline constexpr std::uint64_t increment = UINT64_C(0x9e3779b97f4a7c15);
inline constexpr std::uint64_t mul1 = UINT64_C(0xbf58476d1ce4e5b9);
inline constexpr std::uint64_t mul2 = UINT64_C(0x94d049bb133111eb);
}

// A permutation of all 2^64 values for each FIXED seed. Not cryptographic.
// Integer arithmetic is unsigned and intentionally wraps modulo 2^64.
[[nodiscard]] constexpr std::uint64_t hash_u64(std::uint64_t x,
                                              std::uint64_t seed = 0) noexcept {
    x += seed + detail::increment;
    x = (x ^ (x >> 30)) * detail::mul1;
    x = (x ^ (x >> 27)) * detail::mul2;
    return x ^ (x >> 31);
}

// Exactly 8 readable bytes, little-endian interpretation, no alignment
// requirement. p must not be null. This is NOT a variable-length string hash.
[[nodiscard]] inline std::uint64_t hash8(const void* p,
                                       std::uint64_t seed = 0) noexcept {
    std::uint64_t x;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    std::memcpy(&x, p, sizeof x);
#elif defined(_WIN32)
    std::memcpy(&x, p, sizeof x);
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ && \
      (defined(__GNUC__) || defined(__clang__))
    std::memcpy(&x, p, sizeof x);
    x = __builtin_bswap64(x);
#else
    const auto* b = static_cast<const unsigned char*>(p);
    x = 0;
    for (unsigned i = 0; i < 8; ++i) x |= std::uint64_t(b[i]) << (8 * i);
#endif
    return hash_u64(x, seed);
}

enum class backend { automatic, portable, avx2, avx512 };
[[nodiscard]] bool supported(backend b) noexcept;
[[nodiscard]] backend selected_backend() noexcept;
[[nodiscard]] const char* backend_name(backend b) noexcept;

// Writes count independent hashes, NOT one digest of the whole array.
// Each result is bit-identical to hash_u64(in[i], seed), on every backend.
// Input and output must each refer to count valid uint64_t objects.
// in == out is supported. Partially overlapping ranges are rejected.
// For count == 0, null pointers are accepted and no memory is accessed.
// Returns false without writing for null/nonzero, overlap, size overflow,
// an unsupported backend, or an invalid enum. No allocations or exceptions.
[[nodiscard]] bool hash_many(const std::uint64_t* in, std::uint64_t* out,
                             std::size_t count, std::uint64_t seed = 0,
                             backend b = backend::automatic) noexcept;

struct u64_hasher {
    std::uint64_t seed = 0;
    [[nodiscard]] std::size_t operator()(std::uint64_t value) const noexcept {
        // On 32-bit platforms truncation loses the no-full-hash-collision property.
        return static_cast<std::size_t>(hash_u64(value, seed));
    }
};
} // namespace iskra64
