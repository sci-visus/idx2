#pragma once

// TODO: add logging levels
// TODO: multithreading?
// TODO: per-file buffering mode
// TODO: do we need more than one logger?

#include "mg_common.h"

#define mg_MaxSlots 16

namespace idx2 {

enum class buffer_mode { Full, Line, None };

struct logger {
  stack_array<FILE*, mg_MaxSlots> FHandles    = {};
  stack_array<cstr , mg_MaxSlots> FNames      = {};
  stack_array<u32  , mg_MaxSlots> FNameHashes = {};
  buffer_mode Mode = buffer_mode::Full;
};


inline logger GlobalLogger;

void SetBufferMode(logger* Logger, buffer_mode Mode);
void SetBufferMode(buffer_mode Mode); // set buffer mode for the global logger

} // namespace idx2

/*
Use the following macro for logging as follows
mg_Log("log.txt", "Message %d", A)
mg_Log(stderr, "Message %f", B) */
#define mg_Log(FileName, Format, ...)

namespace idx2 {

constexpr inline bool
IsStdErr(cstr Input) {
  return Input[0] == 's' && Input[1] == 't' && Input[2] == 'd' &&
         Input[3] == 'e' && Input[4] == 'r' && Input[5] == 'r';
}
constexpr inline bool
IsStdOut(cstr Input) {
  return Input[0] == 's' && Input[1] == 't' && Input[2] == 'd' &&
         Input[3] == 'o' && Input[4] == 'u' && Input[5] == 't';
}

mg_T(t) constexpr cstr
CastCStr(t Input) {
  if constexpr (is_same_type<t, cstr>::Value)
    return Input;
  return nullptr;
}

} // namespace idx2

#undef mg_Log
#if defined(mg_Verbose)
#define mg_Log(FileName, Format, ...) {\
  FILE* Fp = nullptr;\
  if constexpr (IsStdErr(#FileName))\
    Fp = stderr;\
  else if constexpr (IsStdOut(#FileName))\
    Fp = stdout;\
  else\
    Fp = idx2::GetFileHandle(&idx2::GlobalLogger, CastCStr(FileName));\
  idx2::printer Pr(Fp);\
  mg_Print(&Pr, Format, __VA_ARGS__);\
}
#else
#define mg_Log(FileName, Format, ...)
#endif

#undef mg_MaxSlots
