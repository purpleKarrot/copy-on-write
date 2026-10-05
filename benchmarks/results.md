# Recorded mutation results

Measurements taken on 2026-10-05 on a macOS 27.0.1 arm64 system. Both builds
used CMake Release (`-O3 -DNDEBUG`), C++20, and the platform libc++. Compilers:
Apple Clang 21.0.0 (clang-2100.3.34.2) and Homebrew LLVM Clang 23.1.2.
Each row is the median of seven samples of 500,000 completed values. All
preflight payload checks and timed checksums passed.

The two compilers ran sequentially on the same machine. Case order is fixed;
CPU scheduling, allocator state, frequency changes, and background activity
can affect results. Differences of a few nanoseconds should not be treated as
portable findings. These are end-to-end setup workloads, including allocation
and destruction, with a 512-byte array payload. See [README.md](README.md) for
case definitions and reproduction commands.

| Case | Writes | Apple Clang 21, ns/value | Clang 23, ns/value |
| --- | ---: | ---: | ---: |
| `unique_repeated` | 8 | 22.02 | 22.26 |
| `unique_grouped` | 8 | 28.55 | 25.99 |
| `prepare_then_wrap` | 8 | 28.18 | 25.89 |
| `direct_in_place` | 8 | 28.63 | 28.45 |
| `shared_repeated` | 8 | 48.98 | 46.06 |
| `shared_grouped` | 8 | 47.28 | 42.55 |
| `shared_transform` | 8 | 51.78 | 54.75 |
| `atomic_counter_probe` | 8 | 22.48 | 22.09 |
| `plain_counter_probe` | 8 | 29.09 | 25.85 |
| `unique_repeated` | 64 | 39.88 | 40.28 |
| `unique_grouped` | 64 | 25.14 | 26.18 |
| `prepare_then_wrap` | 64 | 31.14 | 30.49 |
| `direct_in_place` | 64 | 27.12 | 25.10 |
| `shared_repeated` | 64 | 71.53 | 73.37 |
| `shared_grouped` | 64 | 46.95 | 45.93 |
| `shared_transform` | 64 | 55.18 | 57.35 |
| `atomic_counter_probe` | 64 | 37.40 | 37.88 |
| `plain_counter_probe` | 64 | 24.86 | 23.40 |
| `unique_repeated` | 256 | 105.46 | 106.08 |
| `unique_grouped` | 256 | 85.90 | 86.93 |
| `prepare_then_wrap` | 256 | 90.09 | 90.00 |
| `direct_in_place` | 256 | 86.94 | 85.62 |
| `shared_repeated` | 256 | 136.99 | 177.29 |
| `shared_grouped` | 256 | 109.22 | 106.04 |
| `shared_transform` | 256 | 115.97 | 117.52 |
| `atomic_counter_probe` | 256 | 92.11 | 92.72 |
| `plain_counter_probe` | 256 | 84.59 | 85.15 |

At 64 writes, grouping gave 1.59× throughput for unique values and 1.52× for shared values with Apple Clang 21.

At 64 writes, grouping gave 1.54× throughput for unique values and 1.60× for shared values with Clang 23.

Both compilers show improvements from grouping at 64 and 256 writes. At eight
writes, unique grouped mutation is slower than separate calls. Direct in-place
construction is competitive at the larger counts; preparing then wrapping
adds an array move and is not the fastest case. The copy-based transformation
does not beat grouped mutation in these recorded larger batches.

The atomic/plain diagnostic probes show a difference at 64 writes but reverse
ordering at eight writes. Removing the atomic operation changes optimization
choices as well as instruction cost. Neither probe models reference-count
increments, decrements, detachment, or cross-thread safety. These results cannot
be used to attribute every wrapper overhead to atomic loads or to recommend a
plain counter as a drop-in replacement.

## Decision

Keep atomic reference counting and do not add a counter customization parameter
on the basis of these measurements. Grouping and direct construction address
part of the measured setup cost within the existing interface. Counter
customization remains a possible follow-up if representative application
benchmarks establish a substantial residual cost after those techniques.

Before proposing that interface change, measure workloads with expensive
payload operations, long-lived owners, allocator variation, and concurrent
copy/destruction on additional architectures. These results investigate setup
and compiler optimization; they do not evaluate contention or the overall
benefit of copy-on-write versus deep copying.

Full minimum/median/maximum measurements, including the one-write cases, are
in [apple-clang21.csv](apple-clang21.csv) and [llvm-clang23.csv](llvm-clang23.csv).
