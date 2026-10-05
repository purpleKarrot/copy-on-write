// SPDX-License-Identifier: BSL-1.0
#include <copy_on_write.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct payload
{
  std::array<std::uint64_t, 64> values{};
  explicit payload(std::uint64_t seed)
  {
    for (std::size_t i = 0; i < values.size(); ++i) {
      values[i] = seed + i;
    }
  }
  payload(std::uint64_t seed, std::size_t writes)
    : payload(seed)
  {
    for (std::size_t i = 0; i < writes; ++i) {
      values[i % values.size()] = seed + i * 3;
    }
  }
};

void update(payload& p, std::size_t i, std::uint64_t seed)
{
  // Setup overwrites: later writes to the same element make earlier writes dead.
  p.values[i % p.values.size()] = seed + i * 3;
}

// Escape the completed payload, outside the mutation loop. No barrier is
// inserted between modify calls: that would obscure the optimization question.
void consume(payload const& p)
{
#if defined(__clang__) || defined(__GNUC__)
  asm volatile("" : : "g"(&p) : "memory");
#else
  static std::uint64_t volatile sink;
  sink = p.values[0];
#endif
}

using cow = xyz::copy_on_write<payload>;
enum class method
{
  repeated,
  grouped,
  direct,
  direct_in_place,
  shared_repeated,
  shared_grouped,
  shared_transform
};

std::uint64_t run(method m, std::size_t iterations, std::size_t writes)
{
  std::uint64_t checksum = 0;
  for (std::size_t seed = 0; seed < iterations; ++seed) {
    if (m == method::direct_in_place) {
      cow value(std::in_place, seed, writes);
      consume(*value);
      checksum += value->values[0];
    } else if (m == method::direct) {
      payload p(seed);
      for (std::size_t i = 0; i < writes; ++i) {
        update(p, i, seed);
      }
      cow value(std::move(p));
      consume(*value);
      checksum += value->values[0];
    } else if (m == method::repeated || m == method::grouped) {
      cow value(std::in_place, seed);
      if (m == method::repeated) {
        for (std::size_t i = 0; i < writes; ++i) {
          value.modify([&](payload& p) { update(p, i, seed); });
        }
      } else {
        value.modify([&](payload& p) {
          for (std::size_t i = 0; i < writes; ++i) {
            update(p, i, seed);
          }
        });
      }
      consume(*value);
      checksum += value->values[0];
    } else {
      cow original(std::in_place, seed);
      auto changed = original;
      if (m == method::shared_repeated) {
        for (std::size_t i = 0; i < writes; ++i) {
          changed.modify([&](payload& p) { update(p, i, seed); });
        }
      } else if (m == method::shared_grouped) {
        changed.modify([&](payload& p) {
          for (std::size_t i = 0; i < writes; ++i) {
            update(p, i, seed);
          }
        });
      } else {
        changed.modify(
          [&](payload& p) {
            for (std::size_t i = 0; i < writes; ++i) {
              update(p, i, seed);
            }
          },
          [&](payload const& p) {
            auto result = p;
            for (std::size_t i = 0; i < writes; ++i) {
              update(result, i, seed);
            }
            return result;
          });
      }
      consume(*original);
      consume(*changed);
      if (original->values[0] != seed) {
        throw std::runtime_error("Shared source was modified");
      }
      checksum += changed->values[0];
    }
  }
  return checksum;
}

// A diagnostic probe, not an alternative implementation of copy_on_write.
// Both versions have identical allocation and payload layout except for the
// counter type. The non-atomic version is only used in this single-thread run.
template <bool Atomic>
std::uint64_t counter_probe(std::size_t iterations, std::size_t writes)
{
  struct model
  {
    std::conditional_t<Atomic, std::atomic<long>, long> count{1};
    payload value;
    explicit model(std::uint64_t seed)
      : value(seed)
    {
    }
  };
  std::uint64_t checksum = 0;
  for (std::size_t seed = 0; seed < iterations; ++seed) {
    auto p = std::make_unique<model>(seed);
    for (std::size_t i = 0; i < writes; ++i) {
      long count;
      if constexpr (Atomic) {
        count = p->count.load(std::memory_order_acquire);
      } else {
        count = p->count;
      }
      if (count != 1) {
        throw std::runtime_error("Probe must stay unique");
      }
      update(p->value, i, seed);
    }
    consume(p->value);
    checksum += p->value.values[0];
  }
  return checksum;
}

constexpr std::size_t samples = 7;
template <typename F>
void measure(char const* name, std::size_t iterations, std::size_t writes, F operation)
{
  auto const last_write = ((writes - 1) / 64) * 64;
  auto const expected =
    std::uint64_t(iterations) * (iterations - 1) / 2 + std::uint64_t(iterations) * last_write * 3;
  // validate() checks payload contents before timing. The timed checksum
  // additionally detects incorrect final values.
  operation(100, writes); // warm-up
  std::array<double, samples> timings;
  for (auto& ns : timings) {
    auto start = std::chrono::steady_clock::now();
    auto checksum = operation(iterations, writes);
    auto stop = std::chrono::steady_clock::now();
    if (checksum != expected) {
      throw std::runtime_error("Incorrect final value");
    }
    ns = std::chrono::duration<double, std::nano>(stop - start).count() / iterations;
  }
  std::sort(timings.begin(), timings.end());
  std::cout << name << ',' << writes << ',' << iterations << ',' << timings.front() << ','
            << timings[samples / 2] << ',' << timings.back() << '\n';
}

void validate(method m, std::size_t writes)
{
  // Independently check the update algorithm, including untouched elements.
  payload expected(5);
  for (std::size_t i = 0; i < writes; ++i) {
    update(expected, i, 5);
  }
  cow value(std::in_place, 5);
  std::optional<cow> original;
  if (m == method::shared_repeated || m == method::shared_grouped ||
      m == method::shared_transform) {
    original.emplace(value);
  }
  auto change = [&](payload& p) {
    for (std::size_t i = 0; i < writes; ++i) {
      update(p, i, 5);
    }
  };
  if (m == method::repeated || m == method::shared_repeated) {
    for (std::size_t i = 0; i < writes; ++i) {
      value.modify([&](payload& p) { update(p, i, 5); });
    }
  } else if (m == method::shared_transform) {
    value.modify(change, [&](payload const& p) {
      auto result = p;
      change(result);
      return result;
    });
  } else if (m == method::direct_in_place) {
    value = cow(std::in_place, 5, writes);
  } else if (m == method::direct) {
    value = cow(expected);
  } else {
    value.modify(change);
  }
  if (value->values != expected.values || (original && (*original)->values != payload(5).values)) {
    throw std::runtime_error("Payload validation failed");
  }
}
} // namespace

int main(int argc, char** argv)
{
  try {
    std::size_t iterations = argc > 1 ? std::stoull(argv[1]) : 100000;
    if (iterations == 0 || iterations > 10000000 || argc > 2) {
      throw std::runtime_error("Usage: cow_mutation_benchmark [iterations: 1..10000000]");
    }
    std::cout << "# compiler: " <<
#ifdef __clang__
      __clang_version__
#elif defined(__GNUC__)
      __VERSION__
#else
      "unknown"
#endif
              << "\n# 64 uint64_t payload elements; 7 samples; nanoseconds per completed value\n";
    std::cout << "case,writes,iterations,min_ns,median_ns,max_ns\n"
              << std::fixed << std::setprecision(2);
    for (std::size_t writes : {1, 8, 64, 256}) {
      for (auto [name, m] : {std::pair{"unique_repeated", method::repeated},
                             {"unique_grouped", method::grouped},
                             {"prepare_then_wrap", method::direct},
                             {"direct_in_place", method::direct_in_place},
                             {"shared_repeated", method::shared_repeated},
                             {"shared_grouped", method::shared_grouped},
                             {"shared_transform", method::shared_transform}}) {
        validate(m, writes);
        measure(name, iterations, writes, [m](auto n, auto w) { return run(m, n, w); });
      }
      measure("atomic_counter_probe", iterations, writes, counter_probe<true>);
      measure("plain_counter_probe", iterations, writes, counter_probe<false>);
    }
  } catch (std::exception const& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
