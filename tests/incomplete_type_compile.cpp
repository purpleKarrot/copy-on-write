// SPDX-License-Identifier: BSL-1.0
#include "incomplete_type.hpp"
#include <utility>

#if !defined(COW_INLINE_MOVE_ASSIGN)
void move_construct(incomplete_document& source)
{
  incomplete_document target(std::move(source));
}
#endif
#if !defined(COW_INLINE_MOVE_CTOR)
void move_assign(incomplete_document& target, incomplete_document& source)
{
  target = std::move(source);
}
#endif
