#include "idx2_assert.h"
#include "idx2_io.h"
#include "idx2_memory.h"
#include "idx2_scopeguard.h"

namespace idx2 {

printer::
printer() = default;

printer::
printer(char* BufIn, int SizeIn) : Buf(BufIn), Size(SizeIn), File(nullptr) {}

printer::
printer(FILE* FileIn) : Buf(nullptr), Size(0), File(FileIn) {}

void
Reset(printer* Pr, char* Buf, int Size) {
  Pr->Buf = Buf;
  Pr->Size = Size;
  Pr->File = nullptr;
}

void
Reset(printer* Pr, FILE* File) {
  Pr->Buf = nullptr;
  Pr->Size = 0;
  Pr->File = File;
}

error<>
ReadFile(cstr FileName, buffer* Buf) {
  idx2_Assert((Buf->Data && Buf->Bytes) || (!Buf->Data && !Buf->Bytes));

  FILE* Fp = fopen(FileName, "rb");
  idx2_CleanUp(if (Fp) fclose(Fp));
  if (!Fp)
    return idx2_Error(err_code::FileOpenFailed, "%s", FileName);

  /* Determine the file size */
  if (idx2_FSeek(Fp, 0, SEEK_END))
    return idx2_Error(err_code::FileSeekFailed, "%s", FileName);
  i64 Size = 0;
  if ((Size = idx2_FTell(Fp)) == -1)
    return idx2_Error(err_code::FileTellFailed, "%s", FileName);
  if (idx2_FSeek(Fp, 0, SEEK_SET))
    return idx2_Error(err_code::FileSeekFailed, "%s", FileName);
  if (Buf->Bytes < Size)
    AllocBuf(Buf, Size);

  /* Read file contents */
  idx2_CleanUp(1, DeallocBuf(Buf));
  if (fread(Buf->Data, size_t(Size), 1, Fp) != 1)
    return idx2_Error(err_code::FileReadFailed, "%s", FileName);

  idx2_DismissCleanUp(1);
  return idx2_Error(err_code::NoError);
}

error<>
WriteBuffer(cstr FileName, const buffer& Buf) {
  idx2_Assert(Buf.Data && Buf.Bytes);

  FILE* Fp = fopen(FileName, "wb");
  idx2_CleanUp(if (Fp) fclose(Fp));
  if (!Fp)
    return idx2_Error(err_code::FileCreateFailed, "%s", FileName);

  /* Read file contents */
  if (fwrite(Buf.Data, size_t(Buf.Bytes), 1, Fp) != 1)
    return idx2_Error(err_code::FileWriteFailed, "%s", FileName);
  return idx2_Error(err_code::NoError);
}

} // namespace idx2
