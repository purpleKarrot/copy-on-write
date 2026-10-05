# Mutation benchmarks

Build independently of the tests and GTest:

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DCOW_BUILD_BENCHMARKS=ON
cmake --build build-bench
./build-bench/benchmarks/cow_mutation_benchmark 500000
```

The output is CSV with minimum, median, and maximum nanoseconds per completed
value over seven samples. Each sample completes the requested number of values;
use an optimized build. The harness uses GCC/Clang's compiler barrier to keep
all completed payload contents observable, with no barriers between mutations.
Other compilers use a fallback sink and can produce different optimization
behavior. Warm-up and payload validation occur outside the timed region.

The payload is an array of 64 `uint64_t` elements (512 bytes). Each value starts
with `seed + index`, followed by 1, 8, 64, or 256 setup writes, some of which
overwrite earlier writes. This deliberately models the feedback's concern about
redundant uniqueness checks and dead stores during setup. It does not model
large application objects, I/O, or concurrent access.

Cases:

- `unique_repeated`: construct a wrapper, then one `modify` per write.
- `unique_grouped`: the same writes inside one `modify`.
- `prepare_then_wrap`: prepare the payload locally, then move it into a wrapper.
- `direct_in_place`: pass the complete setup to the payload's constructor and
  construct it inside the wrapper, avoiding a payload move.
- `shared_repeated` and `shared_grouped`: retain a second owner of the original
  value; the first mutation detaches. Both include the same copy and allocation.
- `shared_transform`: use the two-callback overload, copying the input into a
  result and then updating it. This is a baseline transform, not an optimized
  payload-specific reconstruction algorithm.
- `atomic_counter_probe` and `plain_counter_probe`: identical single-owner
  allocation/update loops with atomic and plain counters. These diagnostic
  probes omit sharing, detachment, and atomic release, so they cannot be compared
  directly with the full wrapper as substitute implementations. The plain
  counter is used only in this single-threaded benchmark.

Wrapper cases include construction, allocation, payload work, and destruction.
Reported times are per whole value, not per `modify` or per atomic instruction.
The timed checksum validates the final first element; separate preflight checks
validate every element and preservation of a shared source. A compiler barrier
makes the whole payload observable. Case dispatch and validation of the shared
source are included in the measured wrapper loops.

## Guidance

Group related changes into one `modify` when intermediate observations and
exception behavior do not require separate calls. This permits one uniqueness
check for the batch and lets the compiler optimize the payload work together.
The mutation contract still applies throughout that callback.

For initial setup, construct the final payload directly with `std::in_place`
when its constructor supports that operation. Alternatively, prepare it locally
and move it into the wrapper. The latter can be attractive for cheaply movable
payloads such as vectors, but moving this benchmark's array copies its elements.
Neither approach is guaranteed faster for every workload.

For shared values, batching avoids repeated checks after the first detach; it
does not eliminate the detach. A payload-specific transform may avoid a costly
copy, but merely copying the payload inside the transform is not inherently an
improvement.

Recorded measurements and the decision on counter customization are in
[results.md](results.md). Raw CSV files are alongside that report.
