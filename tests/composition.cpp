// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <compare>

namespace {

class Composite
{
public:
  Composite();
  Composite(Composite const&);
  Composite(Composite&&);
  ~Composite();

  Composite& operator=(Composite const&);
  Composite& operator=(Composite&&);

private:
  friend bool operator==(Composite const&, Composite const&);
  friend auto operator<=>(Composite const&, Composite const&) -> std::strong_ordering;

  struct Implementation;
  xyz::copy_on_write<Implementation> impl_;
};

struct Composite::Implementation
{
  friend bool operator==(Implementation const&, Implementation const&) = default;
  friend auto operator<=>(Implementation const&, Implementation const&) = default;
};

Composite::Composite() = default;
Composite::Composite(Composite const&) = default;
Composite::Composite(Composite&&) = default;
Composite::~Composite() = default;

Composite& Composite::operator=(Composite const&) = default;
Composite& Composite::operator=(Composite&&) = default;

auto operator==(Composite const&, Composite const&) -> bool = default;
auto operator<=>(Composite const&, Composite const&) -> std::strong_ordering = default;

} // namespace

TEST(Composite, Construction)
{
  Composite c;                // default construction
  Composite d(c);             // copy construction
  Composite e = std::move(c); // move construction
  d = e;                      // copy assignment
  e = std::move(d);           // move assignment
  EXPECT_EQ(c, d);
  EXPECT_TRUE(std::is_eq(c <=> d));
}
