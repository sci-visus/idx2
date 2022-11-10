/* Functions that implement file/chunk/brick lookup logic */
#include "idx2Lookup.h"
#include "Format.h"
#include "idx2Common.h"


namespace idx2
{



u64
GetLinearBrick(const idx2_file& Idx2, int Iter, v3i Brick3)
{
  u64 LinearBrick = 0;
  int Size = Idx2.BricksOrderStr[Iter].Len;
  for (int I = Size - 1; I >= 0; --I)
  {
    int D = Idx2.BricksOrderStr[Iter][I] - 'X';
    LinearBrick |= (Brick3[D] & u64(1)) << (Size - I - 1);
    Brick3[D] >>= 1;
  }
  return LinearBrick;
}


file_id
ConstructFilePath(const idx2_file& Idx2, u64 BrickAddress)
{
  u64 Brick;
  i16 BitPlane;
  i8 Level;
  i8 Subband;
  UnpackAddress(Idx2, BrickAddress, &Brick, &Level, &Subband, &BitPlane);
  return ConstructFilePath(Idx2, Brick, Level, Subband, BitPlane);
}


file_id
ConstructFilePath(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BitPlane)
{
#define idx2_PrintLevel idx2_Print(&Pr, "/L%02x", Level);
#define idx2_PrintBrick                                                                            \
  for (int Depth = 0; Depth + 1 < Idx2.FilesDirsDepth[Level].Len; ++Depth)                          \
  {                                                                                                \
    int BitLen =                                                                                   \
      idx2_BitSizeOf(u64) - Idx2.BricksOrderStr[Level].Len + Idx2.FilesDirsDepth[Level][Depth];     \
    idx2_Print(&Pr, "/B%" PRIx64, TakeFirstBits(Brick, BitLen));                                   \
    Brick <<= Idx2.FilesDirsDepth[Level][Depth];                                                    \
    Shift += Idx2.FilesDirsDepth[Level][Depth];                                                     \
  }
#define idx2_PrintSubband idx2_Print(&Pr, "/S%02x", Subband);
#define idx2_PrintBitPlane idx2_Print(&Pr, "/P%04hx", BitPlane);
#define idx2_PrintExtension idx2_Print(&Pr, ".bin");
  u64 BrickBackup = Brick;
  int Shift = 0;
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%.*s/%s/%s/", Idx2.Dir.Size, Idx2.Dir.ConstPtr, Idx2.Name, Idx2.Field);
  if (!Idx2.GroupBitPlanes)
    idx2_PrintBitPlane;
  if (!Idx2.GroupLevels)
    idx2_PrintLevel;
  if (!Idx2.GroupSubbands)
    idx2_PrintSubband;
  idx2_PrintBrick;
  idx2_PrintExtension;
  u64 FileId = GetFileAddress(Idx2, BrickBackup, Level, Subband, BitPlane);
  return file_id{ stref{ FilePath, Pr.Size }, FileId };
#undef idx2_PrintLevel
#undef idx2_PrintBrick
#undef idx2_PrintSubband
#undef idx2_PrintBitPlane
#undef idx2_PrintExtension
}


} // namespace idx2
