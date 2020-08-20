#pragma once

#include "idx2_common.h"
#include "idx2_error_codes.h"
#include "idx2_macros.h"

namespace idx2 {

/* There should be only one error in-flight on each thread */
template <typename t = err_code>
struct error {
  cstr Msg = "";
  t Code = {};
  i8 StackIdx = 0;
  bool StrGened = false;
  error();
  error(t CodeIn, bool StrGenedIn = false, cstr MsgIn = "");
  idx2_T(u) error(const error<u>& Err);
  inline thread_local static cstr Files[64]; // Store file names up the stack
  inline thread_local static i16 Lines[64]; // Store line numbers up the stack
  operator bool() const;
}; // struct err_template

idx2_T(t) cstr ToString(const error<t>& Err, bool Force = false);
struct printer;
idx2_T(t) void PrintStacktrace(printer* Pr, const error<t>& Err);
idx2_T(t) bool ErrorExists(const error<t>& Err);

} // namespace idx2

/* Use this to quickly return an error with line number and file name */
#define idx2_Error(ErrCode, ...)

/* Record file and line information in Error when propagating it up the stack */
#define idx2_PropagateError(Error)
/* Return the error if there is error */
#define idx2_ReturnIfError(Expr)
/* Exit the program if there is error */
#define idx2_ExitIfError(Expr)

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "idx2_memory.h"

namespace idx2 {

idx2_T(t) error<t>::
error() {}

idx2_T(t) error<t>::
error(t CodeIn, bool StrGenedIn, cstr MsgIn) :
  Msg(MsgIn), Code(CodeIn), StackIdx(0), StrGened(StrGenedIn) {}

idx2_T(t) idx2_T(u) error<t>::
error(const error<u>& Err) :
  Msg(Err.Msg), Code((t)Err.Code), StackIdx(Err.StackIdx), StrGened(Err.StrGened) {
//  static_assert(sizeof(t) == sizeof(u));
}

idx2_T(t) error<t>::
operator bool() const {
  return Code == t::NoError;
}

idx2_T(t) cstr
ToString(const error<t>& Err, bool Force) {
  if (Force || !Err.StrGened) {
    auto ErrStr = ToString(Err.Code);
    snprintf(ScratchBuf, sizeof(ScratchBuf), "%.*s (file: %s, line %d): %s",
             ErrStr.Size, ErrStr.Ptr, Err.Files[0], Err.Lines[0], Err.Msg);
  }
  return ScratchBuf;
}

#define idx2_Print(PrinterPtr, Format, ...)
idx2_T(t) void
PrintStacktrace(printer* Pr, const error<t>& Err) {
  (void)Pr;
  idx2_Print(Pr, "Stack trace:\n");
  for (i8 I = 0; I < Err.StackIdx; ++I)
    idx2_Print(Pr, "File %s, line %d\n", Err.Files[I], Err.Lines[I]);
}
#undef idx2_Print

idx2_T(t) bool
ErrorExists(const error<t>& Err) { return Err.Code != t::NoError; }

} // namespace idx2

#undef idx2_Error
#define idx2_Error(ErrCode, ...)\
  [&]() {\
    if constexpr(idx2_NumArgs(__VA_ARGS__) > 0) {\
      idx2::error Err(ErrCode, true, "" idx2_ExtractFirst(__VA_ARGS__));\
      Err.Files[0] = __FILE__;\
      Err.Lines[0] = __LINE__;\
      auto ErrStr = ToString(Err.Code);\
      int L = snprintf(idx2::ScratchBuf, sizeof(idx2::ScratchBuf), "%.*s (file %s, line %d): ",\
                       ErrStr.Size, ErrStr.Ptr, __FILE__, __LINE__);\
      idx2_SPrintHelper(idx2::ScratchBuf, L, "" __VA_ARGS__);\
      return Err;\
    }\
    idx2::error Err(ErrCode);\
    Err.Files[0] = __FILE__;\
    Err.Lines[0] = __LINE__;\
    return Err;\
  }();

#undef idx2_PropagateError
#define idx2_PropagateError(Err)\
  [&Err]() {\
    if (Err.StackIdx >= 64)\
      assert(false && "stack too deep");\
    ++Err.StackIdx;\
    Err.Lines[Err.StackIdx] = __LINE__;\
    Err.Files[Err.StackIdx] = __FILE__;\
    return Err;\
  }();

#undef idx2_ReturnIfError
#define idx2_ReturnIfError(Expr)\
  { auto Result = Expr; if (ErrorExists(Result)) return Result; }

#undef idx2_PropagateIfError
#define idx2_PropagateIfError(Expr)\
  { auto Result = Expr; if (!Result) return idx2_PropagateError(Result); }

#undef idx2_ExitIfError
#define idx2_ExitIfError(Expr)\
  { auto Result = Expr; if (ErrorExists(Result)) { fprintf(stderr, "%s\n", ToString(Result)); exit(1); } }

#define idx2_PropagateIfExpectedError(Expr)\
  { auto Result = Expr; if (!Result) return idx2_PropagateError(Error(Result)); }
