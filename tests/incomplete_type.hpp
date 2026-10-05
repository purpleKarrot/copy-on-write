// SPDX-License-Identifier: BSL-1.0
#ifndef XYZ_INCOMPLETE_TYPE_HPP
#define XYZ_INCOMPLETE_TYPE_HPP

#include <copy_on_write.hpp>

// The client translation unit never sees the definition of Implementation.
class incomplete_document
{
public:
  explicit incomplete_document(int value);
  incomplete_document(incomplete_document const&);
  incomplete_document& operator=(incomplete_document const&);
  ~incomplete_document();

#ifdef COW_INLINE_MOVE_CTOR
  incomplete_document(incomplete_document&&) noexcept = default;
#else
  incomplete_document(incomplete_document&&) noexcept;
#endif
#ifdef COW_INLINE_MOVE_ASSIGN
  incomplete_document& operator=(incomplete_document&&) noexcept = default;
#else
  incomplete_document& operator=(incomplete_document&&) noexcept;
#endif

  int value() const;
  void set_value(int value);
  bool empty() const noexcept;
  static int live_payloads() noexcept;

private:
  struct Implementation;
  xyz::copy_on_write<Implementation> impl_;
};

#endif
