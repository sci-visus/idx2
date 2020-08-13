#pragma once

#include "idx2_common.h"
#include "idx2_error.h"
#include "idx2_macros.h"

namespace idx2 {

/* Store either a value or an error */
idx2_TT(t, u = err_code)
struct expected {
  union {
    t Val;
    error<u> Err;
  };
  bool Ok = false;
  expected();
  expected(const t& ValIn);
  expected(const error<u>& ErrIn);

  /* Get the value through the pointer syntax */
  t& operator*();
  /* Mimic pointer semantics */
  t* operator->();
  /* Bool operator */
  explicit operator bool() const;
}; // struct expected

idx2_TT(t, u) t& Value(expected<t, u>& E);
idx2_TT(t, u) error<u>& Error(expected<t, u>& E);

} // namespace idx2

#include "idx2_assert.h"

namespace idx2 {

idx2_TTi(t, u) expected<t, u>::
expected() : Ok(false) {}

idx2_TTi(t, u) expected<t, u>::
expected(const t& ValIn) : Val(ValIn), Ok(true) {}

idx2_TTi(t, u) expected<t, u>::
expected(const error<u>& ErrIn) : Err(ErrIn), Ok(false) {}

idx2_TTi(t, u) t& expected<t, u>::
operator*() { return Val; }

/* (Safely) get the value with the added check */
idx2_TTi(t, u) t&
Value(expected<t, u>& E) { idx2_Assert(E.Ok); return E.Val; }

idx2_TTi(t, u) t* expected<t, u>::
operator->() { return &**this; }

/* Get the error */
idx2_TTi(t, u) error<u>&
Error(expected<t, u>& E) { idx2_Assert(!E.Ok); return E.Err; }

/* Bool operator */
idx2_TTi(t, u) expected<t, u>::
operator bool() const { return Ok; }

} // namespace idx2


