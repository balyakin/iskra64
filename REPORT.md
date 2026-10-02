# Iskra64 0.1.0-experimental: benchmark report

Measurements: October 2, 2026. One local experiment, not a league table for every hash library.

## Result in context

For batches of 1,024 or more independent 64-bit keys on the tested machine, Iskra64's specialized batch API beat inline XXH3. At 16 keys, it lost. An auto-vectorized implementation of the same well-known SplitMix64 mixer came close to Iskra64's throughput; neither a new general-purpose algorithm nor a universal speed lead is claimed.

## Machine and method

The runs used a virtualized AMD EPYC 9V74 (x86-64, KVM, Linux), one thread pinned to allowed CPU 0, GCC 14.2.0 and Clang 17.0.0. The unmodified xxHash 0.8.3 header came from Debian `libxxhash-dev` 0.8.3-2; its checksum is in `third_party/manifest.json`.

CMake Release used `-O3 -DNDEBUG`. The benchmark client additionally used `-march=native`, including for xxHash and the SplitMix reference. Iskra64 itself was built for the baseline target, with separately targeted SIMD functions; automatic dispatch selected AVX-512. Different dispatch strategies are part of the comparison, but the baselines were not deliberately compiled without available CPU optimizations.

Both XXH3 and XXH64 used `XXH_INLINE_ALL`, and the compiler knew each input was exactly eight bytes. Each contender received the same runtime seed. Function pointers sat at the batch or dependency-chain level, not around every key. Auto-vectorization was allowed. Cases were shuffled between repetitions, warmed up, calibrated for duration, and protected against elimination of work. Timing used `steady_clock`. Each published result is the median of nine recorded repetitions targeting 15 ms apiece, not the best run.

CPU frequency, turbo behavior, and other activity on the physical host were uncontrolled. Pinning to a vCPU does not reserve a physical core. No confidence intervals were calculated, so small differences should not be treated as durable wins.

## Dependent-chain latency

Each next key is the previous hash, leaving no parallelism between keys. Lower is better; values are nanoseconds per hash.

| Algorithm | GCC, seed 42 | Clang, seed 42 |
| --- | ---: | ---: |
| iskra64-inline | 3.715 | 3.667 |
| splitmix-reference | 3.693 | 3.685 |
| XXH3-64-inline | 4.578 | 4.649 |
| XXH64-inline | 7.184 | 7.150 |

`std::hash` is excluded from this chain: on the tested platform its integer hash behaves like the identity function, allowing the compiler to eliminate the dependency. It remains in the batch test as a copy-cost baseline.

## Independent-key throughput

These figures include writing one output hash for each input key. They do not measure one hash over a long message.

| Compiler | Keys | Iskra64 automatic, ns/key | XXH3, ns/key | XXH3 / Iskra64 |
| --- | ---: | ---: | ---: | ---: |
| GCC | 16 | 0.516 | 0.406 | 0.79× |
| GCC | 1,024 | 0.198 | 0.358 | 1.80× |
| GCC | 65,536 | 0.196 | 0.335 | 1.71× |
| GCC | 1,048,576 | 0.205 | 0.339 | 1.65× |
| Clang | 16 | 0.484 | 0.283 | 0.58× |
| Clang | 1,024 | 0.189 | 0.289 | 1.53× |
| Clang | 65,536 | 0.212 | 0.302 | 1.42× |
| Clang | 1,048,576 | 0.215 | 0.306 | 1.42× |

A ratio below 1 means Iskra64 lost. That happened at 16 keys with both compilers. An inline loop deserves separate measurement for tiny batches because GCC and Clang make different vectorization decisions.

### All contenders at 1,024 keys and seed 42

| Contender | GCC, ns/key | Clang, ns/key |
| --- | ---: | ---: |
| iskra64-auto | 0.198 | 0.189 |
| iskra64-inline | 0.199 | 0.182 |
| iskra64-portable | 1.082 | 1.495 |
| iskra64-avx2 | 0.516 | 0.522 |
| iskra64-avx512 | 0.188 | 0.189 |
| splitmix-reference | 0.201 | 0.185 |
| XXH3-64-inline | 0.358 | 0.289 |
| XXH64-inline | 1.941 | 1.978 |
| std-hash-u64 | 0.103 | 0.092 |

The fast `std::hash` result essentially copies the tested numeric keys. It is a useful lower bound for overhead, not evidence of good low-bit distribution for structured input. A hash table with a different bucket rule may behave differently.

### Additional GCC run with seed 0

| Contender | 1,024 keys, ns/key |
| --- | ---: |
| iskra64-auto | 0.185 |
| splitmix-reference | 0.209 |
| XXH3-64-inline | 0.360 |
| XXH64-inline | 2.055 |

XXH3 used its runtime `withSeed` path for both seed 0 and seed 42; a literal compile-time zero-seed path was not measured separately.

## Memory and the meaning of GB/s

The arrays were reused. A batch of 1,024 keys has 8 KiB of input and 8 KiB of output; a batch of 1,048,576 has 8 MiB of each. Some or all of that working set may be served from cache. The `input_GB_per_s` field counts only the eight input bytes per key, excluding another eight bytes written. It is not a measured DRAM bandwidth record or file-hashing throughput.

## Correctness and diagnostics

The archived release builds with GCC and Clang, a portable-only build, and a GCC Debug build with AddressSanitizer and UndefinedBehaviorSanitizer passed CTest. There are also 54 vectors from an independent Python implementation. The correctness test performs roughly 51 million comparisons, including repeated cross-backend checks; these are not 51 million statistically independent random keys.

Coverage includes 600,000 random invertibility checks, batch sizes 0 through 512, start offsets 0 through 7 elements, six seeds, in-place operation, partial overlap, invalid pointer/count combinations, Linux `PROT_NONE` guard pages, and agreement between `hash8` and numeric hashing.

The avalanche smoke test flips each of 64 input bits on 32,768 random values, observing all 4,096 input/output bit pairs at seed 0. Mean flipped bits: **31.997530 of 64**. Largest deviation of an output-bit flip frequency from 50%: **1.0559 percentage points**. This is a narrow diagnostic, not a cryptographic security assessment.

For 1,048,576 distinct structured keys `i << 20` at seed 42, the full 64-bit collision count was zero. Across 65,536 buckets selected by the low 16 bits, reduced χ² was **1.003521** and maximum load was **34**, against a mean of 16.

Zero collisions at full width also follow mathematically from the reversibility of the mixer for a fixed seed. It says nothing about collisions after truncation or bucket reduction, and it is not a unique advantage over all other hash functions.

## What these results do not establish

There is no measured lead over rapidhash, other integer mixers, or existing specialized SIMD libraries. Full SMHasher3, an independent audit, real hash-table workloads, cold data sets beyond cache, multiple physical machines, ARM and other microarchitectures, energy tests, and multithreaded measurements remain undone. Iskra64 is not a cryptographic hash or a new universal primitive.

## Reproducibility files

`results/bench-gcc-seed42.csv`, `bench-clang-seed42.csv`, and `bench-gcc-seed0.csv` hold the raw final repetitions. `results/summary.csv` contains medians, minima, maxima, and median absolute deviations. `results/environment.txt` records the original environment. Reproduction commands are in [README.md](README.md).

`results/bench-gcc-initial.csv` is retained for transparency: it predates the change of the automatic dispatch threshold from 32 to eight keys. It was excluded from the final tables and should not be mixed with those results. Algorithm and license provenance appears in [NOTICE.md](NOTICE.md).
