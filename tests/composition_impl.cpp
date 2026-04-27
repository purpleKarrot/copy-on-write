// SPDX-License-Identifier: BSL-1.0

#include "composition.hpp"

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
