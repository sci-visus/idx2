#include "idx2Common.h"


#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#endif
#endif
#define SEXPR_IMPLEMENTATION
#include "sexpr.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif
#include "zstd/zstd.c"
#include "zstd/zstd.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif


namespace idx2
{


free_list_allocator BrickAlloc_;


void
Dealloc(params* P)
{
  Dealloc(&P->RdoLevels);
}


void
SetName(idx2_file* Idx2, cstr Name)
{
  snprintf(Idx2->Name, sizeof(Idx2->Name), "%s", Name);
}


void
SetField(idx2_file* Idx2, cstr Field)
{
  snprintf(Idx2->Field, sizeof(Idx2->Field), "%s", Field);
}


void
SetVersion(idx2_file* Idx2, const v2i& Ver)
{
  Idx2->Version = Ver;
}


void
SetDimensions(idx2_file* Idx2, const v3i& Dims3)
{
  Idx2->Dims3 = Dims3;
}


void
SetDataType(idx2_file* Idx2, dtype DType)
{
  Idx2->DType = DType;
}


void
SetBrickSize(idx2_file* Idx2, const v3i& BrickDims3)
{
  Idx2->BrickDims3 = BrickDims3;
}


void
SetNumIterations(idx2_file* Idx2, i8 NLevels)
{
  Idx2->NLevels = NLevels;
}


void
SetAccuracy(idx2_file* Idx2, f64 Accuracy)
{
  Idx2->Accuracy = Accuracy;
}


void
SetChunksPerFile(idx2_file* Idx2, int ChunksPerFile)
{
  Idx2->ChunksPerFileIn = ChunksPerFile;
}


void
SetBricksPerChunk(idx2_file* Idx2, int BricksPerChunk)
{
  Idx2->BricksPerChunkIn = BricksPerChunk;
}


void
SetFilesPerDirectory(idx2_file* Idx2, int FilesPerDir)
{
  Idx2->FilesPerDir = FilesPerDir;
}


void
SetDir(idx2_file* Idx2, cstr Dir)
{
  Idx2->Dir = Dir;
}


void
SetGroupLevels(idx2_file* Idx2, bool GroupLevels)
{
  Idx2->GroupLevels = GroupLevels;
}


void
SetGroupSubLevels(idx2_file* Idx2, bool GroupSubLevels)
{
  Idx2->GroupSubLevels = GroupSubLevels;
}


void
SetGroupBitPlanes(idx2_file* Idx2, bool GroupBitPlanes)
{
  Idx2->GroupBitPlanes = GroupBitPlanes;
}


void
SetQualityLevels(idx2_file* Idx2, const array<int>& QualityLevels)
{
  Clear(&Idx2->QualityLevelsIn);
  idx2_ForEach (It, QualityLevels)
  {
    PushBack(&Idx2->QualityLevelsIn, (int)*It);
  };
}


void
Dealloc(idx2_file* Idx2)
{
  Dealloc(&Idx2->BrickOrderStrs);
  Dealloc(&Idx2->ChunkOrderStrs);
  Dealloc(&Idx2->FileOrderStrs);
  Dealloc(&Idx2->Subbands);
  Dealloc(&Idx2->SubbandsNonExt);
  Dealloc(&Idx2->QualityLevelsIn);
  Dealloc(&Idx2->RdoLevels);
}


grid
GetGrid(const extent& Ext, int Iter, u8 Mask, const array<subband>& Subbands)
{
  v3i Strd3(1); // start with stride (1, 1, 1)
  idx2_For (int, D, 0, 3)
    Strd3[D] <<= Iter; // TODO: only work with 1 transform pass per level
  v3i Div(0);

  idx2_For (u8, Sb, 0, 8)
  {
    if (!BitSet(Mask, Sb))
      continue;
    v3i Lh3 = Subbands[Sb].LowHigh3;
    idx2_For (int, D, 0, 3)
      Div[D] = Max(Div[D], Lh3[D]);
  }

  idx2_For (int, D, 0, 3)
    if (Div[D] == 0)
      Strd3[D] <<= 1;

  v3i First3 = From(Ext), Last3 = Last(Ext);
  First3 = ((First3 + Strd3 - 1) / Strd3) * Strd3;
  Last3 = (Last3 / Strd3) * Strd3;
  v3i Dims3 = (Last3 - First3) / Strd3 + 1;

  return grid(First3, Dims3, Strd3);
}


} // namespace idx2
