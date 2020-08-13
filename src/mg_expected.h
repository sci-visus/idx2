#pragma once

#include "mg_common.h"
#include "mg_error.h"
#include "mg_macros.h"

namespace idx2 {

/* Store either a value or an error */
mg_TT(t, u = err_code)
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

mg_TT(t, u) t& Value(expected<t, u>& E);
mg_TT(t, u) error<u>& Error(expected<t, u>& E);

} // namespace idx2

#include "mg_assert.h"

namespace idx2 {

mg_TTi(t, u) expected<t, u>::
expected() : Ok(false) {}

mg_TTi(t, u) expected<t, u>::
expected(const t& ValIn) : Val(ValIn), Ok(true) {}

mg_TTi(t, u) expected<t, u>::
expected(const error<u>& ErrIn) : Err(ErrIn), Ok(false) {}

mg_TTi(t, u) t& expected<t, u>::
operator*() { return Val; }

/* (Safely) get the value with the added check */
mg_TTi(t, u) t&
Value(expected<t, u>& E) { mg_Assert(E.Ok); return E.Val; }

mg_TTi(t, u) t* expected<t, u>::
operator->() { return &**this; }

/* Get the error */
mg_TTi(t, u) error<u>&
Error(expected<t, u>& E) { mg_Assert(!E.Ok); return E.Err; }

/* Bool operator */
mg_TTi(t, u) expected<t, u>::
operator bool() const { return Ok; }

} // namespace idx2


