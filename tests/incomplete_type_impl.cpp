// SPDX-License-Identifier: BSL-1.0
#include "incomplete_type.hpp"

namespace {
int live = 0;
}

struct incomplete_document::Implementation
{
  int value;
  explicit Implementation(int n)
    : value(n)
  {
    ++live;
  }
  Implementation(Implementation const& other)
    : Implementation(other.value)
  {
  }
  ~Implementation() { --live; }
};

incomplete_document::incomplete_document(int value)
  : impl_(std::in_place, value)
{
}
incomplete_document::incomplete_document(incomplete_document const&) = default;
incomplete_document& incomplete_document::operator=(incomplete_document const&) = default;
incomplete_document::incomplete_document(incomplete_document&&) noexcept = default;
incomplete_document& incomplete_document::operator=(incomplete_document&&) noexcept = default;
incomplete_document::~incomplete_document() = default;

int incomplete_document::value() const
{
  return impl_->value;
}
void incomplete_document::set_value(int value)
{
  impl_.modify([value](Implementation& impl) { impl.value = value; });
}
bool incomplete_document::empty() const noexcept
{
  return impl_.valueless_after_move();
}
int incomplete_document::live_payloads() noexcept
{
  return live;
}
