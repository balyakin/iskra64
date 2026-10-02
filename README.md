# Iskra64

[![CI](https://github.com/balyakin/iskra64/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/balyakin/iskra64/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![MIT license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**Fast batches of 64-bit integer hashes. A deliberately narrow job.** Iskra64 is an experimental C++17 library for non-cryptographic hashing of fixed-width keys. Its scalar function works in a header; its allocation-free batch API adds portable, AVX2, and AVX-512 kernels with runtime CPU dispatch.

The mixer is the established SplitMix64 / Stafford Mix13 finalizer, applied after adding the seed and SplitMix increment. Iskra64 does **not** claim to invent a new hash function. The work here is the API, SIMD implementation, dispatch, contract checks, tests, and a benchmark you can reproduce or challenge.

`hash_many` computes **one hash per key**. It does not hash an entire array into one digest. The input is a `uint64_t` or exactly eight readable bytes; arbitrary strings, files, streaming input, and 128-bit output are outside this release.

> [!WARNING]
> **Not cryptographic.** With a known seed, the full-width mapping is reversible. A seed is not a secret key. Do not use Iskra64 for passwords, signatures, authentication, adversarial integrity checks, or hash-flooding defense.

## Build and try it

You need a C++17 compiler and CMake 3.16 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
(cd build && ctest --output-on-failure)
./build/iskra64_example
```

For a build without SIMD kernels:

```sh
cmake -S . -B build-portable -DCMAKE_BUILD_TYPE=Release \
  -DISKRA64_DISABLE_SIMD=ON -DISKRA64_BUILD_BENCH=OFF
cmake --build build-portable --parallel
(cd build-portable && ctest --output-on-failure)
```

The library itself is built for the baseline target by default; AVX2/AVX-512 instructions live in separately targeted functions and are called only after CPU/OS capability checks. The **benchmark executable** uses `-march=native` by default so its xxHash and scalar baselines are not artificially handicapped. To make that executable portable across older CPUs, configure with `-DISKRA64_BENCH_NATIVE=OFF`.

The archived measurements were run on Linux/x86-64 with GCC 14.2.0 and Clang 17.0.0. CI builds and tests GCC, Clang, a portable-only build, and a sanitizer build on Linux. Other architectures and operating systems have not been validated to the same degree.

## Use it

```cpp
#include <iskra64/iskra64.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <unordered_map>

int main() {
    constexpr std::uint64_t seed = 42;
    const std::uint64_t single = iskra64::hash_u64(123, seed);

    std::array<std::uint64_t, 4> keys{10, 20, 30, 40};
    std::array<std::uint64_t, 4> hashes{};
    if (!iskra64::hash_many(keys.data(), hashes.data(), keys.size(), seed)) {
        return 1;
    }

    std::unordered_map<std::uint64_t, int, iskra64::u64_hasher>
        table(0, iskra64::u64_hasher{seed});
    table.emplace(123, 7);
    std::cout << "hash_u64(123): " << single << '\n';
    return table.at(123) == 7 ? 0 : 1;
}
```

The scalar `hash_u64` is header-only. Link against `iskra64` for `hash_many` and backend selection:

```cmake
add_subdirectory(path/to/iskra64)
target_link_libraries(your_target PRIVATE iskra64::iskra64)
```

Embedded builds add only the library by default. Set `ISKRA64_BUILD_TESTS` or `ISKRA64_BUILD_BENCH` before `add_subdirectory` if you want those targets too.

You can also install the CMake package with `cmake --install build --prefix /your/prefix` and consume it with `find_package(iskra64 CONFIG REQUIRED)`. The vendored `xxhash.h` is needed only to build the benchmark, never the library.

### API contract

- `hash_u64(value, seed)` hashes the numeric value. For any fixed seed, the full 64-bit result is a permutation of all 64-bit inputs.
- `hash8(pointer, seed)` reads exactly eight bytes without alignment requirements and interprets them as a little-endian integer. The pointer must be non-null and readable for eight bytes.
- `hash_many(input, output, count, seed, backend)` writes `count` independent hashes. Both arrays require natural `uint64_t` alignment. Exact in-place operation is supported; partial overlap is rejected before writing. Null pointers are accepted only when `count == 0`.
- The optional backend is `automatic`, `portable`, `avx2`, or `avx512`. An unavailable explicit backend, an invalid backend, or invalid array arguments make `hash_many` return `false` without writing. For automatic mode, batches smaller than eight keys use the portable path. Every backend produces identical bits.

The caller remains responsible for supplying genuinely valid memory ranges; a raw-pointer API cannot inspect allocation boundaries. Truncating a result, including through `u64_hasher` on a 32-bit platform, also loses the full-width permutation guarantee. Bucket collisions remain possible.

## What the benchmark says

On **one virtualized AMD EPYC 9V74**, for 1,024 independent keys, seed 42, and the median of nine repetitions:

| Compiler | Iskra64 automatic | Inline XXH3 | XXH3 / Iskra64 |
| --- | ---: | ---: | ---: |
| GCC 14.2.0 | 0.198 ns/key | 0.358 ns/key | 1.80× |
| Clang 17.0.0 | 0.189 ns/key | 0.289 ns/key | 1.53× |

These are per-key throughput figures for reused input/output arrays, **not** single-call latency, file-hashing speed, or DRAM bandwidth. At 16 keys the batch API loses to XXH3 in this setup. A compiler-vectorized SplitMix reference nearly matches Iskra64 at larger sizes; there is no algorithmic breakthrough hiding in the table. Rapidhash was not benchmarked, so no comparison with it is claimed.

Read the [full methodology and negative results](REPORT.md), [raw repetitions](results/), and [machine details](results/environment.txt) before quoting a speedup.

To run the benchmark on Linux, choose a CPU allowed by your environment. The archived run used CPU 0:

```sh
taskset -c 0 ./build/iskra64_bench 15 9 42 > own-results.csv
python3 tools/summarize.py own-results.csv --output own-summary.csv
```

Arguments are target milliseconds per measurement, repetitions, and seed. Without `taskset`, run the benchmark directly and record that difference. Avoid concurrent benchmark runs when comparing results.

## Evidence and status

The repository includes known-answer vectors from an independent Python implementation, differential checks across available kernels, in-place and overlap cases, boundary-page tests on Linux, ASan/UBSan runs, and limited distribution diagnostics. The raw results and their constraints are in [REPORT.md](REPORT.md). Full SMHasher3, an independent audit, hash-table workloads, and measurements across multiple physical machines have not been done. Treat this as a working prototype, not a blanket recommendation to replace a production hash library.

Iskra64's own code is [MIT licensed](LICENSE), copyright © 2026 Evgeny Balyakin. The known mixer and vendored xxHash header have their own provenance and license details in [NOTICE.md](NOTICE.md).
