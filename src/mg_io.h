#pragma once

#include "mg_common.h"
#include "mg_error.h"
#include "mg_memory.h"

#define mg_FSeek
#define mg_FTell

namespace mg {

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

#define mg_Print(PrinterPtr, Format, ...)
#define mg_PrintScratch(Format, ...)
#define mg_PrintScratchN(N, Format, ...) // Print at most N characters

/*
Read a text file from disk into a buffer. The buffer can be nullptr or it can be
initialized in advance, in which case the existing memory will be reused if the
file can fit in it. The caller is responsible to deallocate the memory. */
error<> ReadFile(cstr FileName, buffer* Buf);
error<> WriteBuffer(cstr FileName, const buffer& Buf);

/* Dump a range of stuffs into a text file */
mg_T(i) error<> DumpText(cstr FileName, i Begin, i End, cstr Format);

} // namespace mg

#include <assert.h>
#include <stdio.h>
#include "mg_scopeguard.h"

#undef mg_FSeek
#undef mg_FTell
/* Enable support for reading large files */
#if defined(_WIN32)
  #define mg_FSeek _fseeki64
  #define mg_FTell _ftelli64
#elif defined(__linux__) || defined(__APPLE__)
  #define _FILE_OFFSET_BITS 64
  #define mg_FSeek fseeko
  #define mg_FTell ftello
#endif

namespace mg {

#undef mg_Print
#define mg_Print(PrinterPtr, Format, ...) {\
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

#undef mg_PrintScratch
#define mg_PrintScratch(Format, ...) (\
  snprintf(ScratchBuf, sizeof(ScratchBuf), Format, ##__VA_ARGS__),\
  ScratchBuf\
)

#undef mg_PrintScratchN
#define mg_PrintScratchN(N, Format, ...) (\
  snprintf(ScratchBuf, N + 1, Format, ##__VA_ARGS__),\
  ScratchBuf\
)

mg_T(i) error<>
DumpText(cstr FileName, i Begin, i End, cstr Format) {
  FILE* Fp = fopen(FileName, "w");
  mg_CleanUp(0, if (Fp) fclose(Fp));
  if (!Fp)
    return mg_Error(err_code::FileCreateFailed, "%s", FileName);
  for (i It = Begin; It != End; ++It) {
    if (fprintf(Fp, Format, *It) < 0)
      return mg_Error(err_code::FileWriteFailed);
  }
  return mg_Error(err_code::NoError);
}

#define mg_OpenExistingFile(Fp, FileName, Mode) FILE* Fp = fopen(FileName, Mode)
#define mg_OpenMaybeExistingFile(Fp, FileName, Mode)\
  mg_RAII(FILE*, Fp = fopen(FileName, Mode), , if (Fp) fclose(Fp));\
  if (!Fp) {\
    CreateFullDir(GetDirName(FileName));\
    Fp = fopen(FileName, Mode);\
    mg_Assert(Fp);\
  }
mg_Ti(t) void WritePOD(FILE* Fp, const t Var) { fwrite(&Var, sizeof(Var), 1, Fp); }
mg_Inline void WriteBuffer(FILE* Fp, const buffer& Buf) { fwrite(Buf.Data, Size(Buf), 1, Fp); }
mg_Inline void WriteBuffer(FILE* Fp, const buffer& Buf, i64 Sz) { fwrite(Buf.Data, Sz, 1, Fp); }
mg_Inline void ReadBuffer(FILE* Fp, buffer* Buf) { fread(Buf->Data, Size(*Buf), 1, Fp); }
mg_Inline void ReadBuffer(FILE* Fp, buffer* Buf, i64 Sz) { fread(Buf->Data, Sz, 1, Fp); }
mg_Ti(t) void ReadBuffer(FILE* Fp, buffer_t<t>* Buf) { fread(Buf->Data, Bytes(*Buf), 1, Fp); }
mg_Ti(t) void ReadPOD(FILE* Fp, t* Val) { fread(Val, sizeof(t), 1, Fp); }
mg_Ti(t) void
ReadBackwardPOD(FILE* Fp, t* Val) {
  auto Where = mg_FTell(Fp);
  mg_FSeek(Fp, Where -= sizeof(t), SEEK_SET);
  fread(Val, sizeof(t), 1, Fp);
  mg_FSeek(Fp, Where, SEEK_SET);
}
mg_Inline void
ReadBackwardBuffer(FILE* Fp, buffer* Buf) {
  auto Where = mg_FTell(Fp);
  mg_FSeek(Fp, Where -= Size(*Buf), SEEK_SET);
  fread(Buf->Data, Size(*Buf), 1, Fp);
  mg_FSeek(Fp, Where, SEEK_SET);
}
mg_Inline void
ReadBackwardBuffer(FILE* Fp, buffer* Buf, i64 Sz) {
  assert(Sz <= Size(*Buf));
  auto Where = mg_FTell(Fp);
  mg_FSeek(Fp, Where -= Sz, SEEK_SET);
  fread(Buf->Data, Sz, 1, Fp);
  mg_FSeek(Fp, Where, SEEK_SET);
}

} // namespace mg
