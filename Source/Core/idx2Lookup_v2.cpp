/* Functions that implement file/chunk/brick lookup logic */
#include "idx2Lookup.h"
#include "Format.h"
#include "idx2Common.h"


namespace idx2
{


file_id
ConstructFilePath_v2(const idx2_file& Idx2, u64 BrickAddress)
{
  u64 Brick;
  i16 BitPlane;
  i8 Level;
  i8 Subband;
  UnpackFileAddress(Idx2, BrickAddress, &Brick, &Level, &Subband, &BitPlane);
  return ConstructFilePath(Idx2, Brick, Level, Subband, BitPlane);
}


file_id
ConstructFilePath_v2(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BpKey)
{
  //i16 BpFileKey = (BpKey * Idx2.BitPlanesPerChunk) / Idx2.BitPlanesPerFile;
#define idx2_PrintLevel idx2_Print(&Pr, "/L%02x", Level);
#define idx2_PrintBrick                                                                            \
  for (int Depth = 0; Depth + 1 < Idx2.FilesDirsDepth[Level].Len; ++Depth)                         \
  {                                                                                                \
    int BitLen =                                                                                   \
      idx2_BitSizeOf(u64) - Idx2.BricksOrderStr[Level].Len + Idx2.FilesDirsDepth[Level][Depth];    \
    idx2_Print(&Pr, "/B%" PRIx64, TakeFirstBits(Brick, BitLen));                                   \
    Brick <<= Idx2.FilesDirsDepth[Level][Depth];                                                   \
    Shift += Idx2.FilesDirsDepth[Level][Depth];                                                    \
  }
#define idx2_PrintBitPlane idx2_Print(&Pr, "/P%04hx", BpKey);
#define idx2_PrintExtension idx2_Print(&Pr, ".bin");
  u64 BrickBackup = Brick;
  int Shift = 0;
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%.*s/%s/%s/", Idx2.Dir.Size, Idx2.Dir.ConstPtr, Idx2.Name, Idx2.Field);
  idx2_PrintBitPlane;
  idx2_PrintLevel;
  idx2_PrintBrick;
  idx2_PrintExtension;
  u64 FileId = GetFileAddress(Idx2, BrickBackup, Level, Subband, BpKey);
  return file_id{ stref{ FilePath, Pr.Size }, FileId };
#undef idx2_PrintLevel
#undef idx2_PrintBrick
#undef idx2_PrintSubband
#undef idx2_PrintBitPlane
#undef idx2_PrintExtension
}


} // namespace idx2
