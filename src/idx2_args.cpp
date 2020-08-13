#include <string.h>
#include "idx2_args.h"
#include "idx2_string.h"

namespace idx2 {

bool
OptVal(int NArgs, cstr* Args, cstr Opt, cstr* Val) {
  for (int I = 0; I + 1 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0) {
      *Val = Args[I + 1];
      return true;
    }
  }
  return false;
}

bool
OptVal(int NArgs, cstr* Args, cstr Opt, int* Val) {
  for (int I = 0; I + 1 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0)
      return ToInt(Args[I + 1], Val);
  }
  return false;
}

bool
OptVal(int NArgs, cstr* Args, cstr Opt, u8* Val) {
  int IntVal;
  for (int I = 0; I + 1 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0) {
      bool Success = ToInt(Args[I + 1], &IntVal);
      *Val = (u8)IntVal;
      return Success;
    }
  }
  return false;
}

bool
OptVal(int NArgs, cstr* Args, cstr Opt, t2<char, int>* Val) {
  for (int I = 0; I + 1 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0) {
      Val->First = Args[I + 1][0];
      return ToInt(Args[I + 2], &Val->Second);
    }
  }
  return false;
}

bool
OptVal(int NArgs, cstr* Args, cstr Opt, v3i* Val) {
  for (int I = 0; I + 3 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0) {
      return ToInt(Args[I + 1], &Val->X) &&
             ToInt(Args[I + 2], &Val->Y) &&
             ToInt(Args[I + 3], &Val->Z);
    }
  }
  return false;
}

bool
OptVal(int NArgs, cstr* Args, cstr Opt, array<int>* Vals) {
  Clear(Vals);
  for (int I = 0; I < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0) {
      int J = I;
      while (true) {
        ++J;
        int X;
        if (J < NArgs && ToInt(Args[J], &X)) { PushBack(Vals, X); }
        else { break; }
      }
      return J > I + 1;
    }
  }
  return false;
}

bool
OptVal(int NArgs, cstr* Args, cstr Opt, v2i* Val) {
  for (int I = 0; I + 2 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0) {
      return ToInt(Args[I + 1], &Val->X) &&
             ToInt(Args[I + 2], &Val->Y);
    }
  }
  return false;
}


bool
OptVal(int NArgs, cstr* Args, cstr Opt, v3<t2<char, int>>* Val) {
  for (int I = 0; I + 1 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0) {
      bool Success = true;
      (*Val)[0].First = Args[I + 1][0];
      Success = Success && ToInt(Args[I + 2], &(*Val)[0].Second);
      (*Val)[1].First = Args[I + 2][0];
      Success = Success && ToInt(Args[I + 3], &(*Val)[1].Second);
      (*Val)[2].First = Args[I + 4][0];
      Success = Success && ToInt(Args[I + 5], &(*Val)[2].Second);
      return Success;
    }
  }
  return false;
}

bool
OptVal(int NArgs, cstr* Args, cstr Opt, f64* Val) {
  for (int I = 0; I + 1 < NArgs; ++I) {
    if (strncmp(Args[I], Opt, 32) == 0)
      return ToDouble(Args[I + 1], Val);
  }
  return false;
}

bool
OptExists(int NArgs, cstr* Args, cstr Opt) {
  for (int I = 0; I < NArgs; ++I) {
    if (strcmp(Args[I], Opt) == 0)
      return true;
  }
  return false;
}

} // namespace idx2
