#include <iskra64/iskra64.hpp>
#include "vectors.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>
#if defined(__unix__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#define REQUIRE(x) do { if (!(x)) { std::cerr << "FAILED: " #x << " at " << __LINE__ << '\n'; std::exit(1); } } while (0)
using u64 = std::uint64_t;

// Independently reverse every operation, including multiplication modulo 2^64.
static u64 undo_shift(u64 y, unsigned k) {
    u64 x = y;
    for (unsigned s = k; s < 64; s += k) x ^= y >> s;
    return x;
}
static u64 inverse_odd(u64 a) {
    u64 inverse = 1;
    for (int i = 0; i < 6; ++i) inverse *= 2 - a * inverse;
    return inverse;
}
static u64 reverse_hash(u64 y, u64 seed) {
    y = undo_shift(y, 31);
    y *= inverse_odd(iskra64::detail::mul2);
    y = undo_shift(y, 27);
    y *= inverse_odd(iskra64::detail::mul1);
    y = undo_shift(y, 30);
    return y - seed - iskra64::detail::increment;
}
static u64 reference(u64 value, u64 seed) {
    // Arithmetic matches the independently implemented Python test vectors.
    value += seed;
    value += UINT64_C(0x9e3779b97f4a7c15);
    value ^= value / (UINT64_C(1) << 30);
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value / (UINT64_C(1) << 27);
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value / (UINT64_C(1) << 31));
}
int main() {
    constexpr std::array<iskra64::backend,4> modes{iskra64::backend::automatic,
        iskra64::backend::portable, iskra64::backend::avx2, iskra64::backend::avx512};
    const std::array<u64,6> seeds{0, 1, 42, UINT64_MAX, UINT64_C(1)<<63, UINT64_C(0x123456789abcdef0)};
    static_assert(iskra64::hash_u64(0) == UINT64_C(0xe220a8397b1dcdaf));
    static_assert(iskra64::hash_u64(1) == UINT64_C(0x910a2dec89025cc1));
    static_assert(iskra64::hash_u64(UINT64_MAX) == UINT64_C(0xe4d971771b652c20));
    std::mt19937_64 rng(20261002);
    std::size_t tested = 0;
    for (const auto& v: known_vectors) {
        REQUIRE(iskra64::hash_u64(v.input,v.seed) == v.output);
        ++tested;
    }
    for (auto seed: seeds) {
        for (std::size_t j = 0; j < 100000; ++j) {
            const u64 x = rng(), h = iskra64::hash_u64(x, seed);
            REQUIRE(h == reference(x, seed));
            REQUIRE(reverse_hash(h, seed) == x);
            ++tested;
        }
        for (std::size_t n = 0; n <= 512; ++n) {
            for (std::size_t offset = 0; offset < 8; ++offset) {
                std::vector<u64> in(n+16), out(n+16, UINT64_C(0xbadbadbadbadbad)), expected(n);
                for (auto& x: in) x = rng();
                for (std::size_t i = 0; i < n; ++i) expected[i] = reference(in[i+offset], seed);
                for (auto mode: modes) {
                    if (!iskra64::supported(mode)) continue;
                    REQUIRE(iskra64::hash_many(in.data()+offset, out.data()+offset, n, seed, mode));
                    for (std::size_t i = 0; i < n; ++i) REQUIRE(out[i+offset] == expected[i]);
                    if (offset) REQUIRE(out[offset-1] == UINT64_C(0xbadbadbadbadbad));
                    REQUIRE(out[offset+n] == UINT64_C(0xbadbadbadbadbad));
                    auto inplace = in;
                    REQUIRE(iskra64::hash_many(inplace.data()+offset, inplace.data()+offset, n, seed, mode));
                    for (std::size_t i = 0; i < n; ++i) REQUIRE(inplace[i+offset] == expected[i]);
                    tested += 2*n;
                }
            }
        }
    }
    // Fixed eight-byte hashing is stable for unaligned byte buffers.
    std::array<unsigned char,80> bytes{};
    for (unsigned offset = 0; offset < 64; ++offset) {
        const u64 x = rng();
        for (unsigned i = 0; i < 8; ++i) bytes[offset+i] = static_cast<unsigned char>(x >> (8*i));
        REQUIRE(iskra64::hash8(bytes.data()+offset, 42) == iskra64::hash_u64(x,42));
    }
    std::array<u64,8> data{1,2,3,4,5,6,7,8}, copy = data;
    REQUIRE(iskra64::hash_many(nullptr,nullptr,0));
    REQUIRE(!iskra64::hash_many(nullptr,data.data(),1));
    REQUIRE(!iskra64::hash_many(data.data(),nullptr,1));
    REQUIRE(!iskra64::hash_many(data.data(),data.data()+1,4));
    REQUIRE(data == copy);
    REQUIRE(!iskra64::hash_many(data.data()+1,data.data(),4));
    REQUIRE(data == copy);
    REQUIRE(!iskra64::hash_many(data.data(),data.data(),std::numeric_limits<std::size_t>::max()));
    REQUIRE(!iskra64::hash_many(data.data(),data.data(),8,0,static_cast<iskra64::backend>(99)));
    REQUIRE(data == copy);
    for (auto mode: modes) if (!iskra64::supported(mode)) {
        REQUIRE(!iskra64::hash_many(data.data(),data.data(),8,0,mode));
        REQUIRE(data == copy);
    }
#if defined(__unix__)
    // Any vector overread/overwrite crosses into a PROT_NONE page.
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* a = mmap(nullptr,page*3,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    void* b = mmap(nullptr,page*3,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    REQUIRE(a != MAP_FAILED && b != MAP_FAILED);
    REQUIRE(mprotect(static_cast<char*>(a)+page,page,PROT_READ|PROT_WRITE) == 0);
    REQUIRE(mprotect(static_cast<char*>(b)+page,page,PROT_READ|PROT_WRITE) == 0);
    for (std::size_t n = 1; n <= 128; ++n) {
        auto* in = reinterpret_cast<u64*>(static_cast<char*>(a)+page*2)-n;
        auto* out = reinterpret_cast<u64*>(static_cast<char*>(b)+page*2)-n;
        for (std::size_t i = 0; i < n; ++i) in[i] = rng();
        for (auto mode: modes) if (iskra64::supported(mode)) {
            REQUIRE(iskra64::hash_many(in,out,n,42,mode));
            for (std::size_t i = 0; i < n; ++i) REQUIRE(out[i] == reference(in[i],42));
        }
        REQUIRE(iskra64::hash8(in,42) == reference(in[0],42));
    }
    REQUIRE(munmap(a,page*3) == 0 && munmap(b,page*3) == 0);
#endif
    std::cout << "PASS correctness; comparisons=" << tested << "; selected="
              << iskra64::backend_name(iskra64::selected_backend()) << '\n';
}
