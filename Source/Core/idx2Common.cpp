#include "idx2Common.h"
#include "Math.h"


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
SetDownsamplingFactor(idx2_file* Idx2, const v3i& DownsamplingFactor3)
{
  Idx2->DownsamplingFactor3 = DownsamplingFactor3;
}


error<idx2_err_code>
Finalize(idx2_file* Idx2)
{
  if (!(IsPow2(Idx2->BrickDims3.X) && IsPow2(Idx2->BrickDims3.Y) && IsPow2(Idx2->BrickDims3.Z)))
    return idx2_Error(
      idx2_err_code::BrickSizeNotPowerOfTwo, idx2_PrStrV3i "\n", idx2_PrV3i(Idx2->BrickDims3));
  if (!(Idx2->Dims3 >= Idx2->BrickDims3))
    return idx2_Error(idx2_err_code::BrickSizeTooBig,
                      " total dims: " idx2_PrStrV3i ", brick dims: " idx2_PrStrV3i "\n",
                      idx2_PrV3i(Idx2->Dims3),
                      idx2_PrV3i(Idx2->BrickDims3));
  if (!(Idx2->NLevels <= idx2_file::MaxLevels))
    return idx2_Error(idx2_err_code::TooManyLevels, "Max # of levels = %d\n", Idx2->MaxLevels);

  char TformOrder[8] = {};
  { /* compute the transform order (try to repeat XYZ++) */
    int J = 0;
    idx2_For (int, D, 0, 3)
    {
      if (Idx2->BrickDims3[D] > 1)
        TformOrder[J++] = char('X' + D);
    }
    TformOrder[J++] = '+';
    TformOrder[J++] = '+';
    Idx2->TformOrder = EncodeTransformOrder(TformOrder);
    Idx2->TformOrderFull.Len =
      DecodeTransformOrder(Idx2->TformOrder, Idx2->NTformPasses, Idx2->TformOrderFull.Data);
  }

  { /* build the subbands */
    Idx2->BrickDimsExt3 = idx2_ExtDims(Idx2->BrickDims3);
    BuildSubbands(Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TformOrder, &Idx2->Subbands);
    BuildSubbands(Idx2->BrickDims3, Idx2->NTformPasses, Idx2->TformOrder, &Idx2->SubbandsNonExt);

    // Compute the decode subband mask based on DownsamplingFactor3
    v3i Df3 = Idx2->DownsamplingFactor3;
    idx2_For (int, I, 0, Idx2->NLevels)
    {
      if (Df3.X > 0 && Df3.Y > 0 && Df3.Z > 0)
      {
        --Df3.X;
        --Df3.Y;
        --Df3.Z;
        if (Df3.X > 0 && Df3.Y > 0 && Df3.Z > 0)
          Idx2->DecodeSubbandMasks[I] = 0;
        else
          Idx2->DecodeSubbandMasks[I] = 1; // decode only subband (0, 0, 0)
        continue;
      }
      u8 Mask = 0xFF;
      idx2_For (int, Sb, 0, Size(Idx2->Subbands))
      {
        const v3i& Lh3 = Idx2->Subbands[Sb].LowHigh3;
        if (Df3.X >= Lh3.X && Df3.Y >= Lh3.Y && Df3.Z >= Lh3.Z)
          Mask = UnsetBit(Mask, Sb);
        if (Lh3 == v3i(0)) // always decode subband 0
          Mask = SetBit(Mask, Sb);
      }
      Idx2->DecodeSubbandMasks[I] = Mask;
      if (Df3.X > 0) --Df3.X;
      if (Df3.Y > 0) --Df3.Y;
      if (Df3.Z > 0) --Df3.Z;
    }
    // TODO: maybe decode the first (0, 0, 0) subband?
  }

  { /* compute number of bricks per level */
    Idx2->GroupBrick3 = Idx2->BrickDims3 / Dims(Idx2->SubbandsNonExt[0].Grid);
    v3i NBricks3 = (Idx2->Dims3 + Idx2->BrickDims3 - 1) / Idx2->BrickDims3;
    v3i NBricksIter3 = NBricks3;
    idx2_For (int, I, 0, Idx2->NLevels)
    {
      Idx2->NBricks3s[I] = NBricksIter3;
      NBricksIter3 = (NBricksIter3 + Idx2->GroupBrick3 - 1) / Idx2->GroupBrick3;
    }
  }

  { /* compute the brick order, by repeating the (per brick) transform order */
    Resize(&Idx2->BrickOrderStrs, Idx2->NLevels);
    idx2_For (int, I, 0, Idx2->NLevels)
    {
      v3i N3 = Idx2->NBricks3s[I];
      v3i LogN3 = v3i(Log2Ceil(N3.X), Log2Ceil(N3.Y), Log2Ceil(N3.Z));
      int MinLogN3 = Min(LogN3.X, LogN3.Y, LogN3.Z);
      v3i LeftOver3 =
        LogN3 -
        v3i(Idx2->BrickDims3.X > 1, Idx2->BrickDims3.Y > 1, Idx2->BrickDims3.Z > 1) * MinLogN3;
      char BrickOrder[128];
      int J = 0;
      idx2_For (int, D, 0, 3)
      {
        if (Idx2->BrickDims3[D] == 1)
        {
          while (LeftOver3[D]-- > 0)
            BrickOrder[J++] = char('X' + D);
        }
      }
      while (!(LeftOver3 <= 0))
      {
        idx2_For (int, D, 0, 3)
        {
          if (LeftOver3[D]-- > 0)
            BrickOrder[J++] = char('X' + D);
        }
      }
      if (J > 0)
        BrickOrder[J++] = '+';
      idx2_For (size_t, K, 0, sizeof(TformOrder))
        BrickOrder[J++] = TformOrder[K];
      Idx2->BrickOrders[I] = EncodeTransformOrder(BrickOrder);
      Idx2->BrickOrderStrs[I].Len =
        DecodeTransformOrder(Idx2->BrickOrders[I], N3, Idx2->BrickOrderStrs[I].Data);
      // idx2_Assert(Idx2->BrickOrderStrs[I].Len == Idx2->BrickOrderStrs[0].Len - I *
      // Idx2->TformOrderFull.Len); // disabled since this is not always true
      if (Idx2->BrickOrderStrs[I].Len < Idx2->TformOrderFull.Len)
      {
        return idx2_Error(idx2_err_code::TooManyLevels);
      }
    }

    /* disabled since this check is not always true
    idx2_For(int, I, 1, Idx2->NLevels) {
      i8 Len = Idx2->BrickOrderStrs[I].Len - Idx2->TformOrderFull.Len;
      auto S1 = stref((Idx2->BrickOrderStrs[I].Data + Len), Idx2->TformOrderFull.Len);
      auto S2 = stref(Idx2->TformOrderFull.Data, Idx2->TformOrderFull.Len);
      if (!(S1 == S2))
        return idx2_Error(idx2_err_code::TooManyLevelsOrTransformPasses);
    }*/
  }

  { /* compute BricksPerChunk3 and BrickOrderChunks */
    Idx2->ChunksPerFiles[0] = Idx2->ChunksPerFileIn;
    Idx2->BricksPerChunks[0] = Idx2->BricksPerChunkIn;
    if (!(Idx2->BricksPerChunks[0] <= idx2_file::MaxBricksPerChunk))
      return idx2_Error(idx2_err_code::TooManyBricksPerChunk);
    if (!IsPow2(Idx2->BricksPerChunks[0]))
      return idx2_Error(idx2_err_code::BricksPerChunkNotPowerOf2);
    if (!(Idx2->ChunksPerFiles[0] <= idx2_file::MaxChunksPerFile))
      return idx2_Error(idx2_err_code::TooManyChunksPerFile);
    if (!IsPow2(Idx2->ChunksPerFiles[0]))
      return idx2_Error(idx2_err_code::ChunksPerFileNotPowerOf2);
    idx2_For (int, I, 0, Idx2->NLevels)
    {
      Idx2->BricksPerChunks[I] =
        1 << Min((u8)Log2Ceil(Idx2->BricksPerChunks[0]), Idx2->BrickOrderStrs[I].Len);
      stack_string<64> BrickOrderChunk;
      BrickOrderChunk.Len = Log2Ceil(Idx2->BricksPerChunks[I]);
      Idx2->BricksPerChunk3s[I] = v3i(1);
      idx2_For (int, J, 0, BrickOrderChunk.Len)
      {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - J - 1];
        Idx2->BricksPerChunk3s[I][C - 'X'] *= 2;
        BrickOrderChunk[BrickOrderChunk.Len - J - 1] = C;
      }
      Idx2->BrickOrderChunks[I] =
        EncodeTransformOrder(stref(BrickOrderChunk.Data, BrickOrderChunk.Len));
      idx2_Assert(Idx2->BricksPerChunks[I] = Prod(Idx2->BricksPerChunk3s[I]));
      Idx2->NChunks3s[I] =
        (Idx2->NBricks3s[I] + Idx2->BricksPerChunk3s[I] - 1) / Idx2->BricksPerChunk3s[I];
      /* compute ChunksPerFile3 and ChunkOrderFiles */
      Idx2->ChunksPerFiles[I] = 1 << Min((u8)Log2Ceil(Idx2->ChunksPerFiles[0]),
                                         (u8)(Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len));
      idx2_Assert(Idx2->BrickOrderStrs[I].Len >= BrickOrderChunk.Len);
      stack_string<64> ChunkOrderFile;
      ChunkOrderFile.Len = Log2Ceil(Idx2->ChunksPerFiles[I]);
      Idx2->ChunksPerFile3s[I] = v3i(1);
      idx2_For (int, J, 0, ChunkOrderFile.Len)
      {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len - J - 1];
        Idx2->ChunksPerFile3s[I][C - 'X'] *= 2;
        ChunkOrderFile[ChunkOrderFile.Len - J - 1] = C;
      }
      Idx2->ChunkOrderFiles[I] =
        EncodeTransformOrder(stref(ChunkOrderFile.Data, ChunkOrderFile.Len));
      idx2_Assert(Idx2->ChunksPerFiles[I] == Prod(Idx2->ChunksPerFile3s[I]));
      Idx2->NFiles3s[I] =
        (Idx2->NChunks3s[I] + Idx2->ChunksPerFile3s[I] - 1) / Idx2->ChunksPerFile3s[I];
      /* compute ChunkOrders */
      stack_string<64> ChunkOrder;
      Idx2->ChunksPerVol[I] = 1 << (Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len);
      idx2_Assert(Idx2->BrickOrderStrs[I].Len >= BrickOrderChunk.Len);
      ChunkOrder.Len = Log2Ceil(Idx2->ChunksPerVol[I]);
      idx2_For (int, J, 0, ChunkOrder.Len)
      {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len - J - 1];
        ChunkOrder[ChunkOrder.Len - J - 1] = C;
      }
      Idx2->ChunkOrders[I] = EncodeTransformOrder(stref(ChunkOrder.Data, ChunkOrder.Len));
      Resize(&Idx2->ChunkOrderStrs, Idx2->NLevels);
      Idx2->ChunkOrderStrs[I].Len = DecodeTransformOrder(
        Idx2->ChunkOrders[I], Idx2->NChunks3s[I], Idx2->ChunkOrderStrs[I].Data);
      /* compute FileOrders */
      stack_string<64> FileOrder;
      Idx2->FilesPerVol[I] =
        1 << (Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len - ChunkOrderFile.Len);
      // TODO: the following check may fail if the brick size is too close to the size of the
      // volume, and we set NLevels too high
      idx2_Assert(Idx2->BrickOrderStrs[I].Len >= BrickOrderChunk.Len + ChunkOrderFile.Len);
      FileOrder.Len = Log2Ceil(Idx2->FilesPerVol[I]);
      idx2_For (int, J, 0, FileOrder.Len)
      {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len -
                                         ChunkOrderFile.Len - J - 1];
        FileOrder[FileOrder.Len - J - 1] = C;
      }
      Idx2->FileOrders[I] = EncodeTransformOrder(stref(FileOrder.Data, FileOrder.Len));
      Resize(&Idx2->FileOrderStrs, Idx2->NLevels);
      Idx2->FileOrderStrs[I].Len =
        DecodeTransformOrder(Idx2->FileOrders[I], Idx2->NFiles3s[I], Idx2->FileOrderStrs[I].Data);
    }
  }

  { /* compute spatial depths */
    if (!(Idx2->FilesPerDir <= idx2_file::MaxFilesPerDir))
      return idx2_Error(idx2_err_code::TooManyFilesPerDir, "%d", Idx2->FilesPerDir);
    idx2_For (int, I, 0, Idx2->NLevels)
    {
      Idx2->BricksPerFiles[I] = Idx2->BricksPerChunks[I] * Idx2->ChunksPerFiles[I];
      Idx2->FileDirDepths[I].Len = 0;
      i8 DepthAccum = Idx2->FileDirDepths[I][Idx2->FileDirDepths[I].Len++] =
        Log2Ceil(Idx2->BricksPerFiles[I]);
      i8 Len = Idx2->BrickOrderStrs[I].Len /* - Idx2->TformOrderFull.Len*/;
      while (DepthAccum < Len)
      {
        i8 Inc = Min(i8(Len - DepthAccum), Log2Ceil(Idx2->FilesPerDir));
        DepthAccum += (Idx2->FileDirDepths[I][Idx2->FileDirDepths[I].Len++] = Inc);
      }
      if (Idx2->FileDirDepths[I].Len > idx2_file::MaxSpatialDepth)
        return idx2_Error(idx2_err_code::TooManyFilesPerDir);
      Reverse(Begin(Idx2->FileDirDepths[I]),
              Begin(Idx2->FileDirDepths[I]) + Idx2->FileDirDepths[I].Len);
    }
  }

  { /* compute number of chunks per level */
    Idx2->GroupBrick3 = Idx2->BrickDims3 / Dims(Idx2->SubbandsNonExt[0].Grid);
    v3i NBricks3 = (Idx2->Dims3 + Idx2->BrickDims3 - 1) / Idx2->BrickDims3;
    v3i NBricksIter3 = NBricks3;
    idx2_For (int, I, 0, Idx2->NLevels)
    {
      Idx2->NBricks3s[I] = NBricksIter3;
      NBricksIter3 = (NBricksIter3 + Idx2->GroupBrick3 - 1) / Idx2->GroupBrick3;
    }
  }

  { /* compute the transform details, for both the normal transform and for extrapolation */
    ComputeTransformDetails(&Idx2->Td, Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TformOrder);
    int NLevels = Log2Floor(Max(Max(Idx2->BrickDims3.X, Idx2->BrickDims3.Y), Idx2->BrickDims3.Z));
    ComputeTransformDetails(&Idx2->TdExtrpolate, Idx2->BrickDims3, NLevels, Idx2->TformOrder);
  }

  { /* compute the actual number of bytes for each rdo level */
    i64 TotalUncompressedSize = Prod<i64>(Idx2->Dims3) * SizeOf(Idx2->DType);
    Reserve(&Idx2->RdoLevels, Size(Idx2->QualityLevelsIn));
    idx2_ForEach (It, Idx2->QualityLevelsIn)
    {
      PushBack(&Idx2->RdoLevels, TotalUncompressedSize / *It);
    }
  }

  return idx2_Error(idx2_err_code::NoError);
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


// TODO: handle the case where the query extent is larger than the domain itself
grid
GetGrid(const idx2_file& Idx2, const extent& Ext)
{
  auto CroppedExt = Crop(Ext, extent(Idx2.Dims3));
  v3i Strd3(1); // start with stride (1, 1, 1)
  idx2_For (int, D, 0, 3)
    Strd3[D] <<= Idx2.DownsamplingFactor3[D];

  v3i First3 = From(CroppedExt);
  v3i Last3 = Last(CroppedExt);
  Last3 = ((Last3 + Strd3 - 1) / Strd3) * Strd3; // move last to the right
  First3 = (First3 / Strd3) * Strd3; // move first to the left

  return grid(First3, (Last3 - First3) / Strd3 + 1, Strd3);
}


} // namespace idx2
