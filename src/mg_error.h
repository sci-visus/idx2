#pragma once

#include "mg_common.h"
#include "mg_error_codes.h"
#include "mg_macros.h"

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
  mg_T(u) error(const error<u>& Err);
  inline thread_local static cstr Files[64]; // Store file names up the stack
  inline thread_local static i16 Lines[64]; // Store line numbers up the stack
  operator bool() const;
}; // struct err_template

mg_T(t) cstr ToString(const error<t>& Err, bool Force = false);
struct printer;
mg_T(t) void PrintStacktrace(printer* Pr, const error<t>& Err);
mg_T(t) bool ErrorExists(const error<t>& Err);

} // namespace idx2

/* Use this to quickly return an error with line number and file name */
#define mg_Error(ErrCode, ...)

/* Record file and line information in Error when propagating it up the stack */
#define mg_PropagateError(Error)
/* Return the error if there is error */
#define mg_ReturnIfError(Expr)
/* Exit the program if there is error */
#define mg_ExitIfError(Expr)

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mg_memory.h"

namespace idx2 {

mg_T(t) error<t>::
error() {}

mg_T(t) error<t>::
error(t CodeIn, bool StrGenedIn, cstr MsgIn) :
  Msg(MsgIn), Code(CodeIn), StackIdx(0), StrGened(StrGenedIn) {}

mg_T(t) mg_T(u) error<t>::
error(const error<u>& Err) :
  Msg(Err.Msg), Code((t)Err.Code), StackIdx(Err.StackIdx), StrGened(Err.StrGened) {
//  static_assert(sizeof(t) == sizeof(u));
}

mg_T(t) error<t>::
operator bool() const {
  return Code == t::NoError;
}

mg_T(t) cstr
ToString(const error<t>& Err, bool Force) {
  if (Force || !Err.StrGened) {
    auto ErrStr = ToString(Err.Code);
    snprintf(ScratchBuf, sizeof(ScratchBuf), "%.*s (file: %s, line %d): %s",
             ErrStr.Size, ErrStr.Ptr, Err.Files[0], Err.Lines[0], Err.Msg);
  }
  return ScratchBuf;
}

#define mg_Print(PrinterPtr, Format, ...)
mg_T(t) void
PrintStacktrace(printer* Pr, const error<t>& Err) {
  (void)Pr;
  mg_Print(Pr, "Stack trace:\n");
  for (i8 I = 0; I < Err.StackIdx; ++I)
    mg_Print(Pr, "File %s, line %d\n", Err.Files[I], Err.Lines[I]);
}
#undef mg_Print

mg_T(t) bool
ErrorExists(const error<t>& Err) { return Err.Code != t::NoError; }

} // namespace idx2

#undef mg_Error
#define mg_Error(ErrCode, ...)\
  [&]() {\
    idx2::error Err(ErrCode);\
    Err.Files[0] = __FILE__;\
    Err.Lines[0] = __LINE__;\
    return Err;\
  }();

#undef mg_PropagateError
#define mg_PropagateError(Err)\
  [&Err]() {\
    if (Err.StackIdx >= 64)\
      assert(false && "stack too deep");\
    ++Err.StackIdx;\
    Err.Lines[Err.StackIdx] = __LINE__;\
    Err.Files[Err.StackIdx] = __FILE__;\
    return Err;\
  }();

#undef mg_ReturnIfError
#define mg_ReturnIfError(Expr)\
  { auto Result = Expr; if (ErrorExists(Result)) return Result; }

#undef mg_PropagateIfError
#define mg_PropagateIfError(Expr)\
  { auto Result = Expr; if (!Result) return mg_PropagateError(Result); }

#undef mg_ExitIfError
#define mg_ExitIfError(Expr)\
  { auto Result = Expr; if (ErrorExists(Result)) { fprintf(stderr, "%s\n", ToString(Result)); exit(1); } }

#define mg_PropagateIfExpectedError(Expr)\
  { auto Result = Expr; if (!Result) return mg_PropagateError(Error(Result)); }
