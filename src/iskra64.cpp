// SPDX-License-Identifier: MIT
#include "iskra64/iskra64.hpp"

#if (defined(__x86_64__) || defined(__i386__)) && \
    (defined(__GNUC__) || defined(__clang__)) && !defined(ISKRA64_DISABLE_SIMD)
#define ISKRA64_X86_TARGETS 1
#include <immintrin.h>
#else
#define ISKRA64_X86_TARGETS 0
#endif

namespace iskra64 {
namespace {
using kernel = void (*)(const std::uint64_t*, std::uint64_t*, std::size_t,
                        std::uint64_t) noexcept;
void portable(const std::uint64_t* in, std::uint64_t* out, std::size_t n,
              std::uint64_t seed) noexcept {
    for (std::size_t i = 0; i < n; ++i) out[i] = hash_u64(in[i], seed);
}

#if ISKRA64_X86_TARGETS
#define TARGET2 __attribute__((target("avx2")))
#define TARGET512 __attribute__((target("avx512f,avx512dq")))
// AVX2 does not have packed 64x64->64 multiplication. Reconstruct the low
// product with three packed 32x32->64 products. The high*high term vanishes.
TARGET2 inline __m256i mullo64(__m256i x, __m256i c) noexcept {
    const __m256i lo = _mm256_mul_epu32(x, c);
    const __m256i cross = _mm256_add_epi64(
        _mm256_mul_epu32(_mm256_srli_epi64(x, 32), c),
        _mm256_mul_epu32(x, _mm256_srli_epi64(c, 32)));
    return _mm256_add_epi64(lo, _mm256_slli_epi64(cross, 32));
}
TARGET2 inline __m256i mix2(__m256i x, __m256i offset, __m256i c1,
                           __m256i c2) noexcept {
    x = _mm256_add_epi64(x, offset);
    x = mullo64(_mm256_xor_si256(x, _mm256_srli_epi64(x, 30)), c1);
    x = mullo64(_mm256_xor_si256(x, _mm256_srli_epi64(x, 27)), c2);
    return _mm256_xor_si256(x, _mm256_srli_epi64(x, 31));
}
TARGET2 void vector2(const std::uint64_t* in, std::uint64_t* out,
                     std::size_t n, std::uint64_t seed) noexcept {
    const auto off = _mm256_set1_epi64x(static_cast<long long>(seed + detail::increment));
    const auto c1 = _mm256_set1_epi64x(static_cast<long long>(detail::mul1));
    const auto c2 = _mm256_set1_epi64x(static_cast<long long>(detail::mul2));
    std::size_t i = 0;
    for (; n - i >= 16; i += 16) {
        auto a = mix2(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(in+i)), off,c1,c2);
        auto b = mix2(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(in+i+4)), off,c1,c2);
        auto c = mix2(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(in+i+8)), off,c1,c2);
        auto d = mix2(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(in+i+12)), off,c1,c2);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out+i), a);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out+i+4), b);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out+i+8), c);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out+i+12), d);
    }
    for (; n - i >= 4; i += 4) {
        auto x = mix2(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(in+i)), off,c1,c2);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out+i), x);
    }
    for (; i < n; ++i) out[i] = hash_u64(in[i], seed);
}
TARGET512 inline __m512i mix512(__m512i x, __m512i offset, __m512i c1,
                               __m512i c2) noexcept {
    x = _mm512_add_epi64(x, offset);
    x = _mm512_mullo_epi64(_mm512_xor_si512(x, _mm512_srli_epi64(x, 30)), c1);
    x = _mm512_mullo_epi64(_mm512_xor_si512(x, _mm512_srli_epi64(x, 27)), c2);
    return _mm512_xor_si512(x, _mm512_srli_epi64(x, 31));
}
TARGET512 void vector512(const std::uint64_t* in, std::uint64_t* out,
                         std::size_t n, std::uint64_t seed) noexcept {
    const auto off = _mm512_set1_epi64(static_cast<long long>(seed + detail::increment));
    const auto c1 = _mm512_set1_epi64(static_cast<long long>(detail::mul1));
    const auto c2 = _mm512_set1_epi64(static_cast<long long>(detail::mul2));
    std::size_t i = 0;
    for (; n - i >= 32; i += 32) {
        auto a = mix512(_mm512_loadu_si512(in+i), off,c1,c2);
        auto b = mix512(_mm512_loadu_si512(in+i+8), off,c1,c2);
        auto c = mix512(_mm512_loadu_si512(in+i+16), off,c1,c2);
        auto d = mix512(_mm512_loadu_si512(in+i+24), off,c1,c2);
        _mm512_storeu_si512(out+i, a);
        _mm512_storeu_si512(out+i+8, b);
        _mm512_storeu_si512(out+i+16, c);
        _mm512_storeu_si512(out+i+24, d);
    }
    for (; n - i >= 8; i += 8) {
        auto x = mix512(_mm512_loadu_si512(in+i), off,c1,c2);
        _mm512_storeu_si512(out+i, x);
    }
    for (; i < n; ++i) out[i] = hash_u64(in[i], seed);
}
#undef TARGET2
#undef TARGET512
#endif

struct dispatch {
    bool avx2 = false, avx512 = false;
    backend chosen = backend::portable;
    kernel fn = portable;
    dispatch() noexcept {
#if ISKRA64_X86_TARGETS
        __builtin_cpu_init();
        avx2 = __builtin_cpu_supports("avx2");
        avx512 = __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512dq");
        if (avx512) { chosen = backend::avx512; fn = vector512; }
        else if (avx2) { chosen = backend::avx2; fn = vector2; }
#endif
    }
};
const dispatch& get_dispatch() noexcept { static const dispatch d; return d; }
} // namespace

bool supported(backend b) noexcept {
    const auto& d = get_dispatch();
    switch (b) {
        case backend::automatic: case backend::portable: return true;
        case backend::avx2: return d.avx2;
        case backend::avx512: return d.avx512;
        default: return false;
    }
}
backend selected_backend() noexcept { return get_dispatch().chosen; }
const char* backend_name(backend b) noexcept {
    switch (b) {
        case backend::automatic: return "automatic";
        case backend::portable: return "portable";
        case backend::avx2: return "avx2";
        case backend::avx512: return "avx512";
        default: return "invalid";
    }
}
bool hash_many(const std::uint64_t* in, std::uint64_t* out, std::size_t n,
               std::uint64_t seed, backend b) noexcept {
    if (!supported(b)) return false;
    if (n == 0) return true;
    if (!in || !out || n > std::numeric_limits<std::size_t>::max()/sizeof(*in)) return false;
    if (in != out) {
        const auto a = reinterpret_cast<std::uintptr_t>(in);
        const auto c = reinterpret_cast<std::uintptr_t>(out);
        const auto distance = a > c ? a - c : c - a;
        if (distance < n * sizeof(*in)) return false;
    }
    const auto& d = get_dispatch();
    switch (b) {
        case backend::automatic:
            // Avoid vector setup for tiny batches. The capability policy is
            // deterministic, not a claim of optimal dispatch on every CPU.
            if (n < 8) portable(in, out, n, seed);
            else d.fn(in, out, n, seed);
            return true;
        case backend::portable: portable(in, out, n, seed); return true;
#if ISKRA64_X86_TARGETS
        case backend::avx2: vector2(in, out, n, seed); return true;
        case backend::avx512: vector512(in, out, n, seed); return true;
#endif
        default: return false;
    }
}
} // namespace iskra64
