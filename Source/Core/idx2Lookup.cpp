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
  i16 BitPlane = i16((i32(BrickAddress & 0xFFF) << 20) >> 20); // this convoluted double shifts is to keep the sign of BitPlane
  i8 Level = (BrickAddress >> 12) & 0x3F;
  i8 Iter = (BrickAddress >> 60) & 0xF;
  u64 Brick = ((BrickAddress >> 18) & 0x3FFFFFFFFFFull) << Log2Ceil(Idx2.BricksPerFile[Iter]);
  return ConstructFilePath(Idx2, Brick, Iter, Level, BitPlane);
}


static file_id
ConstructFilePathExponents(const idx2_file& Idx2, u64 Brick, i8 Level, i8 SubLevel);


//static idx2_Inline file_id
//ConstructFilePathExponents(const idx2_file& Idx2, u64 BrickAddress)
//{
//  i8 Level = (BrickAddress >> 12) & 0x3F;
//  i8 Iter = (BrickAddress >> 60) & 0xF;
//  u64 Brick = ((BrickAddress >> 18) & 0x3FFFFFFFFFFull) << Log2Ceil(Idx2.BricksPerFile[Iter]);
//  return ConstructFilePathExponents(Idx2, Brick, Iter, Level);
//}


file_id
ConstructFilePathRdos(const idx2_file& Idx2, u64 Brick, i8 Level)
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
#define idx2_PrintExtension idx2_Print(&Pr, ".rdo");
  u64 BrickBackup = Brick;
  int Shift = 0;
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%s/%s/%s/TruncationPoints/", Idx2.Dir, Idx2.Name, Idx2.Field);
  idx2_PrintLevel;
  idx2_PrintBrick;
  idx2_PrintExtension;
  u64 FileId = GetFileAddressRdo(Idx2.BricksPerFile[Level], BrickBackup, Level);
  return file_id{ stref{ FilePath, Pr.Size }, FileId };
#undef idx2_PrintLevel
#undef idx2_PrintBrick
#undef idx2_PrintExtension
}

// TODO: write a struct to help with bit packing / unpacking so we don't have to manually edit these
// TODO: make the following inline

file_id
ConstructFilePath(const idx2_file& Idx2, u64 Brick, i8 Level, i8 SubLevel, i16 BitPlane)
{
  if (BitPlaneIsExponent(BitPlane)) //
  {
    return ConstructFilePathExponents(Idx2, Brick, Level, SubLevel);
  }

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
#define idx2_PrintSubLevel idx2_Print(&Pr, "/S%02x", SubLevel);
#define idx2_PrintBitPlane idx2_Print(&Pr, "/P%04hx", BitPlane);
#define idx2_PrintExtension idx2_Print(&Pr, ".bin");
  u64 BrickBackup = Brick;
  int Shift = 0;
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%s/%s/%s/BrickData/", Idx2.Dir, Idx2.Name, Idx2.Field);
  if (!Idx2.GroupBitPlanes)
    idx2_PrintBitPlane;
  if (!Idx2.GroupLevels)
    idx2_PrintLevel;
  if (!Idx2.GroupSubLevels)
    idx2_PrintSubLevel;
  idx2_PrintBrick;
  idx2_PrintExtension;
  u64 FileId = GetFileAddress(Idx2, BrickBackup, Level, SubLevel, BitPlane);
  return file_id{ stref{ FilePath, Pr.Size }, FileId };
#undef idx2_PrintLevel
#undef idx2_PrintBrick
#undef idx2_PrintSubLevel
#undef idx2_PrintBitPlane
#undef idx2_PrintExtension
}


static file_id
ConstructFilePathExponents(const idx2_file& Idx2, u64 Brick, i8 Level, i8 SubLevel)
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
#define idx2_PrintSubLevel idx2_Print(&Pr, "/S%02x", SubLevel);
#define idx2_PrintExtension idx2_Print(&Pr, ".bex");
  u64 BrickBackup = Brick;
  int Shift = 0;
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%s/%s/%s/BrickExponents/", Idx2.Dir, Idx2.Name, Idx2.Field);
  idx2_PrintLevel;
  idx2_PrintSubLevel;
  idx2_PrintBrick;
  idx2_PrintExtension;
  u64 FileId = GetFileAddress(Idx2, BrickBackup, Level, SubLevel, ExponentBitPlane_);
  return file_id{ stref{ FilePath, Pr.Size }, FileId };
#undef idx2_PrintLevel
#undef idx2_PrintSubLevel
#undef idx2_PrintBrick
#undef idx2_PrintExtension
}


// file_id
// ConstructFilePathRdos(const idx2_file& Idx2, u64 Brick, i8 Level) {
//   #define idx2_PrintLevel idx2_Print(&Pr, "/L%02x", Level);
//   #d  ef    ine idx2_PrintBrick\
//    for (int Depth = 0; Depth + 1 < Idx2.FileDirDepths[Level].Len; ++Depth) {\
//      int BitLen = idx2_BitSizeOf(u64) - Idx2.BrickOrderStrs[Level].Len +
//       Idx2.FileDirDepths[Level][Depth];\
//      idx2_Print(&Pr, "/B%" PRIx64, TakeFirstBits(Brick, BitLen));\
//      Brick <<= Idx2.FileDirDepths[Level][Depth];\
//      Shift += Idx2.FileDirDepths[Level][Depth];\
//    }
//   #define idx2_PrintExtension idx2_Print(&Pr, ".rdo");
//   u64 BrickBackup = Brick;
//   int Shift = 0;
//   thread_local static char FilePath[256];
//   printer Pr(FilePath, sizeof(FilePath));
//   idx2_Print(&Pr, "%s/%s/%s/TruncationPoints/", Idx2.Dir, Idx2.Name, Idx2.Field);
//   idx2_PrintLevel;
//   idx2_PrintBrick;
//   idx2_PrintExtension;
//   u64 FileId = GetFileAddressRdo(Idx2.BricksPerFiles[Level], BrickBackup, Level);
//   return file_id{stref{FilePath, Pr.Size}, FileId};
//   #undef idx2_PrintLevel
//   #undef idx2_PrintBrick
//   #undef idx2_PrintExtension
// }


} // namespace idx2
