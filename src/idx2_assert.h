/* Assert macros that carry file and line information, as well as a custom message */

#pragma once

// TODO: add a idx2_AssertIf
#define idx2_Assert(Cond, ...)
#define idx2_AbortIf(Cond, ...)
#define idx2_Abort(...)

namespace idx2 {

using handler = void (int);
void AbortHandler(int Signum);
void SetHandleAbortSignals(handler& Handler = AbortHandler);

} // namespace idx2

#include <stdio.h>
#include <stdlib.h>
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-inline-cpp-without-extern"
#endif
#include "idx2_debugbreak.h"
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif
#include "idx2_io.h"
#include "idx2_macros.h"
#include "idx2_stacktrace.h"

#define idx2_AssertHelper(Debug, Cond, ...)\
  do {\
    if (!(Cond)) {\
      fprintf(stderr, "Condition \"%s\" failed, ", #Cond);\
      fprintf(stderr, "in file %s, line %d\n", __FILE__, __LINE__);\
      if constexpr(idx2_NumArgs(__VA_ARGS__) > 0) {\
        idx2_FPrintHelper(stderr, "" __VA_ARGS__);\
        fprintf(stderr, "\n");\
      }\
      printer Pr(stderr);\
      PrintStacktrace(&Pr);\
      if constexpr (Debug)\
        debug_break();\
      else\
        exit(EXIT_FAILURE);\
    }\
  } while (0);

#undef idx2_Assert
#if defined(idx2_Slow)
  #define idx2_Assert(Cond, ...) idx2_AssertHelper(true, (Cond), __VA_ARGS__)
#else
  #define idx2_Assert(Cond, ...) do {} while (0)
#endif

#undef idx2_AbortIf
#define idx2_AbortIf(Cond, ...)\
  idx2_AssertHelper(false, !(Cond) && "Fatal error!", __VA_ARGS__)
#undef idx2_Abort
#define idx2_Abort(...) idx2_AbortIf(true, __VA_ARGS__)
