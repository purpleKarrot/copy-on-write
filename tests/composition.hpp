// SPDX-License-Identifier: BSL-1.0

#ifndef XYZ_COPY_ON_WRITE_COMPOSITION_HPP
#define XYZ_COPY_ON_WRITE_COMPOSITION_HPP

#include <copy_on_write.hpp>

class Composite
{
public:
  Composite();
  Composite(Composite const&);
  Composite(Composite&&);
  Composite& operator=(Composite const&);
  Composite& operator=(Composite&&);
  ~Composite();

private:
  friend bool operator==(Composite const&, Composite const&);
  friend auto operator<=>(Composite const&, Composite const&) -> std::strong_ordering;

  struct Implementation;
  xyz::copy_on_write<Implementation> impl_;
};

#endif // XYZ_COPY_ON_WRITE_COMPOSITION_HPP
