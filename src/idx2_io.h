#pragma once

#include "idx2_common.h"
#include "idx2_error.h"
#include "idx2_memory.h"

#define idx2_FSeek
#define idx2_FTell

namespace idx2 {

/* Print formatted strings into a buffer */
struct printer {
  char* Buf = nullptr;
  int Size = 0;
  FILE* File = nullptr; // either File == nullptr or Buf == nullptr
  printer();
  printer(char* BufIn, int SizeIn);
  printer(FILE* FileIn);
};

void Reset(printer* Pr, char* Buf, int Size);
void Reset(printer* Pr, FILE* File);

#define idx2_Print(PrinterPtr, Format, ...)
#define idx2_PrintScratch(Format, ...)
#define idx2_PrintScratchN(N, Format, ...) // Print at most N characters

/*
Read a text file from disk into a buffer. The buffer can be nullptr or it can be
initialized in advance, in which case the existing memory will be reused if the
file can fit in it. The caller is responsible to deallocate the memory. */
error<> ReadFile(cstr FileName, buffer* Buf);
error<> WriteBuffer(cstr FileName, const buffer& Buf);

/* Dump a range of stuffs into a text file */
idx2_T(i) error<> DumpText(cstr FileName, i Begin, i End, cstr Format);

} // namespace idx2

#include <assert.h>
#include <stdio.h>
#include "idx2_scopeguard.h"

#undef idx2_FSeek
#undef idx2_FTell
/* Enable support for reading large files */
#if defined(_WIN32)
  #define idx2_FSeek _fseeki64
  #define idx2_FTell _ftelli64
#elif defined(__linux__) || defined(__APPLE__)
  #define _FILE_OFFSET_BITS 64
  #define idx2_FSeek fseeko
  #define idx2_FTell ftello
#endif

namespace idx2 {

#undef idx2_Print
#define idx2_Print(PrinterPtr, Format, ...) {\
  if ((PrinterPtr)->Buf && !(PrinterPtr)->File) {\
    if ((PrinterPtr)->Size <= 1)\
      assert(false && "buffer too small"); /* TODO: always abort */ \
    int Written = snprintf((PrinterPtr)->Buf, size_t((PrinterPtr)->Size),\
                           Format, ##__VA_ARGS__);\
    (PrinterPtr)->Buf += Written;\
    if (Written < (PrinterPtr)->Size)\
      (PrinterPtr)->Size -= Written;\
    else\
      assert(false && "buffer overflow?");\
  } else if (!(PrinterPtr)->Buf && (PrinterPtr)->File) {\
    fprintf((PrinterPtr)->File, Format, ##__VA_ARGS__);\
  } else {\
    assert(false && "unavailable or ambiguous printer destination");\
  }\
}

#undef idx2_PrintScratch
#define idx2_PrintScratch(Format, ...) (\
  snprintf(ScratchBuf, sizeof(ScratchBuf), Format, ##__VA_ARGS__),\
  ScratchBuf\
)

#undef idx2_PrintScratchN
#define idx2_PrintScratchN(N, Format, ...) (\
  snprintf(ScratchBuf, N + 1, Format, ##__VA_ARGS__),\
  ScratchBuf\
)

idx2_T(i) error<>
DumpText(cstr FileName, i Begin, i End, cstr Format) {
  FILE* Fp = fopen(FileName, "w");
  idx2_CleanUp(0, if (Fp) fclose(Fp));
  if (!Fp)
    return idx2_Error(err_code::FileCreateFailed, "%s", FileName);
  for (i It = Begin; It != End; ++It) {
    if (fprintf(Fp, Format, *It) < 0)
      return idx2_Error(err_code::FileWriteFailed);
  }
  return idx2_Error(err_code::NoError);
}

#define idx2_OpenExistingFile(Fp, FileName, Mode) FILE* Fp = fopen(FileName, Mode)
#define idx2_OpenMaybeExistingFile(Fp, FileName, Mode)\
  idx2_RAII(FILE*, Fp = fopen(FileName, Mode), , if (Fp) fclose(Fp));\
  if (!Fp) {\
    CreateFullDir(GetDirName(FileName));\
    Fp = fopen(FileName, Mode);\
    idx2_Assert(Fp);\
  }
idx2_Ti(t) void WritePOD(FILE* Fp, const t Var) { fwrite(&Var, sizeof(Var), 1, Fp); }
idx2_Inline void WriteBuffer(FILE* Fp, const buffer& Buf) { fwrite(Buf.Data, Size(Buf), 1, Fp); }
idx2_Inline void WriteBuffer(FILE* Fp, const buffer& Buf, i64 Sz) { fwrite(Buf.Data, Sz, 1, Fp); }
idx2_Inline void ReadBuffer(FILE* Fp, buffer* Buf) { fread(Buf->Data, Size(*Buf), 1, Fp); }
idx2_Inline void ReadBuffer(FILE* Fp, buffer* Buf, i64 Sz) { fread(Buf->Data, Sz, 1, Fp); }
idx2_Ti(t) void ReadBuffer(FILE* Fp, buffer_t<t>* Buf) { fread(Buf->Data, Bytes(*Buf), 1, Fp); }
idx2_Ti(t) void ReadPOD(FILE* Fp, t* Val) { fread(Val, sizeof(t), 1, Fp); }
idx2_Ti(t) void
ReadBackwardPOD(FILE* Fp, t* Val) {
  auto Where = idx2_FTell(Fp);
  idx2_FSeek(Fp, Where -= sizeof(t), SEEK_SET);
  fread(Val, sizeof(t), 1, Fp);
  idx2_FSeek(Fp, Where, SEEK_SET);
}
idx2_Inline void
ReadBackwardBuffer(FILE* Fp, buffer* Buf) {
  auto Where = idx2_FTell(Fp);
  idx2_FSeek(Fp, Where -= Size(*Buf), SEEK_SET);
  fread(Buf->Data, Size(*Buf), 1, Fp);
  idx2_FSeek(Fp, Where, SEEK_SET);
}
idx2_Inline void
ReadBackwardBuffer(FILE* Fp, buffer* Buf, i64 Sz) {
  assert(Sz <= Size(*Buf));
  auto Where = idx2_FTell(Fp);
  idx2_FSeek(Fp, Where -= Sz, SEEK_SET);
  fread(Buf->Data, Sz, 1, Fp);
  idx2_FSeek(Fp, Where, SEEK_SET);
}

} // namespace idx2
