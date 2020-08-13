/* Assert macros that carry file and line information, as well as a custom message */

#pragma once

// TODO: add a mg_AssertIf
#define mg_Assert(Cond, ...)
#define mg_AbortIf(Cond, ...)
#define mg_Abort(...)

namespace mg {

using handler = void (int);
void AbortHandler(int Signum);
void SetHandleAbortSignals(handler& Handler = AbortHandler);

} // namespace mg

#include <stdio.h>
#include <stdlib.h>
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wgnu-inline-cpp-without-extern"
#endif
#include "mg_debugbreak.h"
#pragma GCC diagnostic pop
#include "mg_io.h"
#include "mg_macros.h"
#include "mg_stacktrace.h"

#define mg_AssertHelper(Debug, Cond, ...)\
  do {\
    if (!(Cond)) {\
      fprintf(stderr, "Condition \"%s\" failed, ", #Cond);\
      fprintf(stderr, "in file %s, line %d\n", __FILE__, __LINE__);\
      if (mg_NumArgs(__VA_ARGS__) > 0) {\
        mg_FPrintHelper(stderr, __VA_ARGS__);\
        fprintf(stderr, "\n");\
      }\
      printer Pr(stderr);\
      PrintStacktrace(&Pr);\
      if (Debug)\
        debug_break();\
      else\
        exit(EXIT_FAILURE);\
    }\
  } while (0);

#undef mg_Assert
#if defined(mg_Slow)
  //#define mg_Assert(Cond, ...) mg_AssertHelper(true, (Cond), __VA_ARGS__)
  #define mg_Assert(Cond, ...) do {} while (0)
#else
  #define mg_Assert(Cond, ...) do {} while (0)
#endif

#undef mg_AbortIf
#define mg_AbortIf(Cond, ...)\
  mg_AssertHelper(false, !(Cond) && "Fatal error!", __VA_ARGS__)
#undef mg_Abort
#define mg_Abort(...) mg_AbortIf(true, __VA_ARGS__)
