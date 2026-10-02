// The same caller loops, constant 8-byte length, and native optimizations
// are available to every baseline. XXH_INLINE_ALL avoids a shared-library
// call penalty for xxHash and allows automatic vectorization.
#include <iskra64/iskra64.hpp>
#define XXH_INLINE_ALL
#include "xxhash.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using u64 = std::uint64_t;
using clock_type = std::chrono::steady_clock;
#if defined(__VERSION__)
static constexpr const char* compiler_version = __VERSION__;
#else
static constexpr const char* compiler_version = "unknown compiler";
#endif
static volatile u64 sink = 0;
#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
static inline void memory_barrier() { asm volatile("" ::: "memory"); }
#else
#define NOINLINE
#include <atomic>
static inline void memory_barrier() { std::atomic_signal_fence(std::memory_order_seq_cst); }
#endif

struct iskra {
    u64 operator()(u64 x, u64 seed) const { return iskra64::hash_u64(x,seed); }
};
struct xxh3 {
    u64 operator()(u64 x, u64 seed) const { return XXH3_64bits_withSeed(&x,8,seed); }
};
struct xxh64 {
    u64 operator()(u64 x, u64 seed) const { return XXH64(&x,8,seed); }
};
// This baseline is intentional: Iskra64 uses this KNOWN mixer, not a new
// primitive. A sufficiently optimized compiler can reach the same speed.
struct splitmix_reference {
    u64 operator()(u64 x, u64 seed) const {
        u64 z = x + seed + UINT64_C(0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
        return z ^ (z >> 31);
    }
};
struct identity {
    u64 operator()(u64 x, u64 /* seed */) const { return std::hash<u64>{}(x); }
};
using batch_fn = void (*)(const u64*, u64*, std::size_t, u64);
using chain_fn = u64 (*)(u64, std::size_t, u64);
template<class F> NOINLINE void batch_loop(const u64* in,u64* out,std::size_t n,u64 seed) {
    for (std::size_t i = 0; i < n; ++i) out[i] = F{}(in[i],seed);
}
template<class F> NOINLINE u64 chain(u64 x,std::size_t n,u64 seed) {
    for (std::size_t i = 0; i < n; ++i) x = F{}(x,seed);
    return x;
}
template<iskra64::backend B> NOINLINE void iskra_batch(const u64* in,u64* out,std::size_t n,u64 seed) {
    if (!iskra64::hash_many(in,out,n,seed,B)) std::abort();
}
struct item { const char* name; batch_fn batch; chain_fn latency; };

int main(int argc, char** argv) {
    try {
        const double target_ms = argc>1 ? std::stod(argv[1]) : 15.0;
        const int repetitions = argc>2 ? std::stoi(argv[2]) : 9;
        const u64 seed = argc>3 ? std::stoull(argv[3]) : 42;
        if (target_ms < 1 || target_ms > 500 || repetitions < 3 || repetitions > 99)
            throw std::invalid_argument("usage: iskra64_bench [milliseconds=15, 1..500] [repetitions=9, 3..99] [seed=42]");
        std::cerr << "compiler=" << compiler_version << "; xxhash=" << XXH_VERSION_MAJOR << '.' << XXH_VERSION_MINOR << '.' << XXH_VERSION_RELEASE
                  << "; auto=" << iskra64::backend_name(iskra64::selected_backend()) << "; seed=" << seed
                  << "; target_ms=" << target_ms << "; repetitions=" << repetitions << '\n';
        std::vector<item> items{
            {"iskra64-inline",batch_loop<iskra>,chain<iskra>},
            {"splitmix-reference",batch_loop<splitmix_reference>,chain<splitmix_reference>},
            {"XXH3-64-inline",batch_loop<xxh3>,chain<xxh3>},
            {"XXH64-inline",batch_loop<xxh64>,chain<xxh64>},
            {"std-hash-u64",batch_loop<identity>,nullptr},
            {"iskra64-auto",iskra_batch<iskra64::backend::automatic>,nullptr},
            {"iskra64-portable",iskra_batch<iskra64::backend::portable>,nullptr}};
        if (iskra64::supported(iskra64::backend::avx2)) items.push_back({"iskra64-avx2",iskra_batch<iskra64::backend::avx2>,nullptr});
        if (iskra64::supported(iskra64::backend::avx512)) items.push_back({"iskra64-avx512",iskra_batch<iskra64::backend::avx512>,nullptr});
        std::mt19937_64 rng(20261002);
        std::cout << "mode,algorithm,keys_per_batch,repeat,iterations,ns_per_key,input_GB_per_s,seed\n";
        std::cout << std::setprecision(9);
        const auto elapsed = [](auto start){ return std::chrono::duration<double,std::nano>(clock_type::now()-start).count(); };
        // Dependency chain: measures latency, not independent-key throughput.
        std::vector<item> latency;
        for (auto a: items) if(a.latency) latency.push_back(a);
        for (int r = -1; r < repetitions; ++r) {
            std::shuffle(latency.begin(),latency.end(),rng);
            for (const auto& a: latency) {
                std::size_t iterations = 1<<16;
                auto start = clock_type::now();
                sink = a.latency(rng(),iterations,seed);
                const double estimate = std::max(elapsed(start),1.0);
                iterations = std::clamp<std::size_t>(static_cast<std::size_t>(iterations * target_ms * 1e6 / estimate),1024,100000000);
                start = clock_type::now();
                const auto h = a.latency(rng(),iterations,seed);
                const double ns = elapsed(start)/iterations;
                sink = h;
                if (r >= 0) std::cout << "latency," << a.name << ",1," << r << ',' << iterations << ',' << ns << ',' << 8/ns << ',' << seed << '\n';
            }
        }
        // Each case hashes n independent keys to n output values. Inputs and
        // outputs stay the same size, memory barriers prevent hoisting or DCE.
        // 1024 keys: 16 KiB working set. 1M keys: 16 MiB working set. NOT a
        // claim about a stream from DRAM or hashing one giant message.
        for (const std::size_t n: {std::size_t(16),std::size_t(1024),std::size_t(65536),std::size_t(1048576)}) {
            std::vector<u64> in(n),out(n);
            for (auto& x: in) x = rng();
            for (int r = -1; r < repetitions; ++r) {
                std::shuffle(items.begin(),items.end(),rng);
                for (const auto& a: items) {
                    a.batch(in.data(),out.data(),n,seed); memory_barrier();
                    std::size_t iterations = std::max<std::size_t>(1,262144/n);
                    auto run = [&](std::size_t count) {
                        const auto start = clock_type::now();
                        for (std::size_t k=0;k<count;++k) {
                            a.batch(in.data(),out.data(),n,seed);
                            memory_barrier();
                        }
                        const double result = elapsed(start);
                        sink = out[rng()%n];
                        return result;
                    };
                    const double estimate = std::max(run(iterations),1.0);
                    iterations = std::clamp<std::size_t>(static_cast<std::size_t>(iterations * target_ms * 1e6 / estimate),1,100000000);
                    const double ns = run(iterations)/(double(iterations)*n);
                    if (r >= 0) std::cout << "batch," << a.name << ',' << n << ',' << r << ',' << iterations << ',' << ns << ',' << 8/ns << ',' << seed << '\n';
                }
            }
        }
        std::cerr << "sink=" << sink << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
