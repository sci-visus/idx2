#include <signal.h>
#include "idx2_assert.h"
#include "idx2_common.h"

namespace idx2 {

void AbortHandler(int Signum) {
  cstr Name = nullptr;
  switch (Signum) {
    case SIGABRT: Name = "SIGABRT"; break;
    case SIGSEGV: Name = "SIGSEGV"; break;
#if !defined(_WIN32)
    case SIGBUS : Name = "SIGBUS" ; break;
#endif
    case SIGILL : Name = "SIGILL" ; break;
    case SIGFPE : Name = "SIGFPE" ; break;
  };
  if (Name)
    fprintf(stderr, "Caught signal %d (%s)\n", Signum, Name);
  else
    fprintf(stderr, "Caught signal %d\n", Signum);

  printer Pr(stderr);
  PrintStacktrace(&Pr);
  exit(Signum);
}

void SetHandleAbortSignals(handler& Handler) {
  signal(SIGABRT, Handler);
  signal(SIGSEGV, Handler);
#if !defined(_WIN32)
  signal(SIGBUS,  Handler);
#endif
  signal(SIGILL,  Handler);
  signal(SIGFPE,  Handler);
}

} // namespace idx2
