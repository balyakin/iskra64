# Algorithm and third-party notices

Iskra64's own code is licensed under MIT. The project name is provisional; it does not imply a registered trademark or a uniqueness check.

## Integer mixer

Iskra64 does **not** invent a new scalar hash algorithm. `hash_u64(x, seed)` applies the established SplitMix64 / Stafford Mix13 finalizer to `x + seed + 0x9e3779b97f4a7c15` modulo 2^64. The constants and shift sequence come from Sebastiano Vigna's published implementation:

https://prng.di.unimi.it/splitmix64.c

This prototype contributes an API for independent keys, a portable path, AVX2/AVX-512 batch kernels, runtime dispatch, contract checks, tests, and a reproducible benchmark. The auto-vectorized reference is deliberately present in the benchmark so that an existing mixer is not mistaken for a new invention.

The source permission accompanying the SplitMix64 implementation reads:

> Written in 2015 by Sebastiano Vigna (vigna@acm.org)
>
> To the extent possible under law, the author has dedicated all copyright
> and related and neighboring rights to this software to the public domain
> worldwide.
>
> Permission to use, copy, modify, and/or distribute this software for any
> purpose with or without fee is hereby granted.
> THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
> WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
> MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
> ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
> WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
> ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
> IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

## xxHash

`third_party/xxhash.h` is the unmodified xxHash 0.8.3 header from Debian package `libxxhash-dev` version `0.8.3-2`. Its complete BSD 2-Clause license and Yann Collet's copyright notice remain in the header. The header is used only by the benchmark; Iskra64's library does not depend on xxHash.

https://github.com/Cyan4973/xxHash

The SHA-256 checksum is recorded in `third_party/manifest.json`. This is the version actually used for the comparison, not a claim about the latest upstream release.

## Context, not benchmark entrants

Rapidhash and SMHasher were also considered while scoping the experiment. Rapidhash was **not** measured in this archive, and no performance claim against it is made. Full SMHasher/SMHasher3 was not run either.

https://github.com/Nicoshev/rapidhash
https://github.com/rurban/smhasher
https://github.com/fwojcik/smhasher3
