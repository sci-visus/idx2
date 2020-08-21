#include "idx2_common.h"
#include "idx2_algorithm.h"
#include "idx2_array.h"
#include "idx2_data_types.h"
#include "idx2_expected.h"
#include "idx2_filesystem.h"
#include "idx2_logger.h"
#include "idx2_hashtable.h"
#include "idx2_macros.h"
#include "idx2_scopeguard.h"
#include "idx2_function.h"
#include "idx2_stats.h"
#include "idx2_timer.h"
#include "idx2_varint.h"
#include "idx2_wavelet.h"
#include "idx2_v1.h"
#include "idx2_zfp.h"
#include <string.h>
#define __STDC_FORMAT_MACROS

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif
#include <zstd/zstd.h>
#include <zstd/zstd.c>
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm> // TODO: write my own quicksort

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

// Get non-extrapolated dims
#define idx2_NonExtDims(P3)\
  v3i(P3.X - (P3.X > 1), P3.Y - (P3.Y > 1), P3.Z - (P3.Z > 1))
#define idx2_ExtDims(P3)\
  v3i(P3.X + (P3.X > 1), P3.Y + (P3.Y > 1), P3.Z + (P3.Z > 1))

namespace idx2 {
// TODO: translate the bit plane range to the correct range
// TODO: store min/max values in the metadata

/* not used for now
static v3i
GetPartialResolution(const v3i& Dims3, u8 Mask, const array<subband>& Subbands) {
  v3i Div(0);
  idx2_For(u8, Sb, 0, 8) {
    if (!BitSet(Mask, Sb)) continue;
    v3i Lh3 = Subbands[Sb].LowHigh3;
    idx2_For(int, D, 0, 3) Div[D] = Max(Div[D], Lh3[D]);
  }
  v3i OutDims3 = Dims3;
  idx2_For(int, D, 0, 3) if (Div[D] == 0) OutDims3[D] = (Dims3[D] + 1) >> 1;
  return OutDims3;
}*/

static grid
GetGrid(const extent& Ext, int Iter, u8 Mask, const array<subband>& Subbands) {
  v3i Strd3(1); // start with stride (1, 1, 1)
  idx2_For(int, D, 0, 3) Strd3[D] <<= Iter; // TODO: only work with 1 transform pass per level
  v3i Div(0);
  idx2_For(u8, Sb, 0, 8) {
    if (!BitSet(Mask, Sb)) continue;
    v3i Lh3 = Subbands[Sb].LowHigh3;
    idx2_For(int, D, 0, 3) Div[D] = Max(Div[D], Lh3[D]);
  }
  idx2_For(int, D, 0, 3) if (Div[D] == 0) Strd3[D] <<= 1;
  v3i First3 = From(Ext), Last3 = Last(Ext);
  First3 = ((First3 + Strd3 - 1) / Strd3) * Strd3;
  Last3 = (Last3 / Strd3) * Strd3;
  v3i Dims3 = (Last3 - First3) / Strd3 + 1;
  return grid(First3, Dims3, Strd3);
}

void decode_all::
Init(const idx2_file& Idx2_) { this->Idx2 = &Idx2_; Ext = extent(Idx2_.Dims3); }
extent decode_all::
GetExtent() const { return Ext; }
f64 decode_all::
GetAccuracy() const { return Accuracy; }
int decode_all::
GetQuality() const { return QualityLevel; }
void decode_all::
SetExtent(const extent& Ext_) { this->Ext = Ext_; }
void decode_all::
SetIteration(int Iter_) { this->Iter = Iter_; }
void decode_all::
SetAccuracy(f64 Accuracy_) { this->Accuracy = Accuracy_; }
void decode_all::
SetQuality(int Quality_) { this->QualityLevel = Quality_; }
void decode_all::
SetMask(u8 Mask_) { this->Mask = Mask_; }
int decode_all::GetIteration() const { return Iter; }
u8 decode_all::GetMask() const { return Mask; }
void decode_all::
Destroy() { return; }

idx2_T(t) void
Dealloc(brick_table<t>* BrickTable) {
  idx2_ForEach(BrickIt, BrickTable->Bricks) BrickTable->Alloc->Dealloc(BrickIt.Val->Samples);
  Dealloc(&BrickTable->Bricks);
  idx2_ForEach(BlockSig, BrickTable->BlockSigs) Dealloc(BlockSig);
}

// Compose a key from Brick + Iteration
idx2_Inline u64
GetBrickKey(i8 Iter, u64 Brick) { return (Brick << 4) + Iter; }
// Get the Brick from a Key composed of Brick + Iteration
idx2_Inline u64
BrickFromBrickKey(u64 BrickKey) { return BrickKey >> 4; }
// Get the Iteration from a Key composed of Brick + Iteration
idx2_Inline i8
IterationFromBrickKey(u64 BrickKey) { return i8(BrickKey & 0xF); }

// Compose a Key from Iteration + Level + BitPlane
idx2_Inline u32
GetChannelKey(i16 BitPlane, i8 Iter, i8 Level) { return (u32(BitPlane) << 16) + (u32(Level) << 4) + Iter; }
// Get Level from a Key composed of Iteration + Level + BitPlane
idx2_Inline i8
LevelFromChannelKey(u64 ChannelKey) { return i8((ChannelKey >> 4) & 0xFFFF); }
// Get Iteration from a Key composed of Iteration + Level + BitPlane
idx2_Inline i8
IterationFromChannelKey(u64 ChannelKey) { return i8(ChannelKey & 0xF); }
// Get BitPlane from a Key composed of Iteration + Level + BitPlane
idx2_Inline i16
BitPlaneFromChannelKey(u64 ChannelKey) { return i16(ChannelKey >> 16); }

// Get a Key composed from Iteration + Level
idx2_Inline u16
GetSubChannelKey(i8 Iter, i8 Level) { return (u16(Level) << 4) + Iter; }

static free_list_allocator BrickAlloc_;

void Dealloc(params* P) { Dealloc(&P->RdoLevels); }
void SetName(idx2_file* Idx2, cstr Name) { snprintf(Idx2->Name, sizeof(Idx2->Name), "%s", Name); }
void SetField(idx2_file* Idx2, cstr Field) { snprintf(Idx2->Field, sizeof(Idx2->Field), "%s", Field); }
void SetVersion(idx2_file* Idx2, const v2i& Ver) { Idx2->Version = Ver; }
void SetDimensions(idx2_file* Idx2, const v3i& Dims3) { Idx2->Dims3 = Dims3; }
void SetDataType(idx2_file* Idx2, dtype DType) { Idx2->DType = DType; }
void SetBrickSize(idx2_file* Idx2, const v3i& BrickDims3) { Idx2->BrickDims3 = BrickDims3; }
void SetNumIterations(idx2_file* Idx2, i8 NLevels) { Idx2->NLevels = NLevels; }
void SetAccuracy(idx2_file* Idx2, f64 Accuracy) { Idx2->Accuracy = Accuracy; }
void SetChunksPerFile(idx2_file* Idx2, int ChunksPerFile) { Idx2->ChunksPerFileIn = ChunksPerFile; }
void SetBricksPerChunk(idx2_file* Idx2, int BricksPerChunk) { Idx2->BricksPerChunkIn = BricksPerChunk; }
void SetFilesPerDirectory(idx2_file* Idx2, int FilesPerDir) { Idx2->FilesPerDir = FilesPerDir; }
void SetDir(idx2_file* Idx2, cstr Dir) { Idx2->Dir = Dir; }
void SetGroupLevels(idx2_file* Idx2, bool GroupLevels) { Idx2->GroupLevels = GroupLevels; }
void SetGroupSubLevels(idx2_file* Idx2, bool GroupSubLevels) { Idx2->GroupSubLevels = GroupSubLevels; }
void SetGroupBitPlanes(idx2_file* Idx2, bool GroupBitPlanes) { Idx2->GroupBitPlanes = GroupBitPlanes; }
void SetQualityLevels(idx2_file* Idx2, const array<int>& QualityLevels) { Clear(&Idx2->QualityLevelsIn); idx2_ForEach(It, QualityLevels) { PushBack(&Idx2->QualityLevelsIn, (int)*It); }; }

// TODO: return an error
//static void
//CompressBuf(const buffer& Input, bitstream* Output) {
//  if (Size(Input) == 0) return;
//  const int MaxDstSize = LZ4_compressBound(Size(Input));
//  GrowToAccomodate(Output, MaxDstSize - Size(*Output));
//  const int CpresSize = LZ4_compress_default((cstr)Input.Data, (char*)Output->Stream.Data, Size(Input), MaxDstSize);
//  if (CpresSize <= 0) {
//    fprintf(stderr, "CompressBuf failed\n");
//    exit(1);
//  }
//  Output->BitPtr = CpresSize + Output->Stream.Data;
//}

static void
CompressBufZstd(const buffer& Input, bitstream* Output) {
  if (Size(Input) == 0) return;
  size_t const MaxDstSize = ZSTD_compressBound(Size(Input));
  GrowToAccomodate(Output, MaxDstSize - Size(*Output));
  size_t const CpresSize = ZSTD_compress(Output->Stream.Data, MaxDstSize, Input.Data, Size(Input), 1);
  if (CpresSize <= 0) {
    fprintf(stderr, "CompressBufZstd failed\n");
    exit(1);
  }
  Output->BitPtr = CpresSize + Output->Stream.Data;

}

// TODO: return an error
//static void
//DecompressBuf(const buffer& Input, bitstream* Output, i64 OutputSize) {
//  GrowToAccomodate(Output, OutputSize - Size(*Output));
//  int Result = LZ4_decompress_safe((cstr)Input.Data, (char*)Output->Stream.Data, Size(Input), Size(Output->Stream));
//  if (Result != OutputSize) {
//    fprintf(stderr, "Lz4 decompression failed\n");
//    exit(1);
//  }
//  idx2_Assert(Result == OutputSize);
//  InitRead(Output, Output->Stream);
//}

static void
DecompressBufZstd(const buffer& Input, bitstream* Output) {
  unsigned long long const OutputSize = ZSTD_getFrameContentSize(Input.Data, Size(Input));
  GrowToAccomodate(Output, OutputSize - Size(*Output));
  size_t const Result = ZSTD_decompress(Output->Stream.Data, OutputSize, Input.Data, Size(Input));
  if (Result != OutputSize) {
    fprintf(stderr, "Zstd decompression failed\n");
    exit(1);
  }
}

error<idx2_file_err_code>
Finalize(idx2_file* Idx2) {
  if (!(IsPow2(Idx2->BrickDims3.X) && IsPow2(Idx2->BrickDims3.Y) && IsPow2(Idx2->BrickDims3.Z)))
    return idx2_Error(idx2_file_err_code::BrickSizeNotPowerOfTwo, idx2_PrStrV3i "\n", idx2_PrV3i(Idx2->BrickDims3));
  if (!(Idx2->Dims3 >= Idx2->BrickDims3))
    return idx2_Error(idx2_file_err_code::BrickSizeTooBig, " total dims: " idx2_PrStrV3i ", brick dims: " idx2_PrStrV3i "\n", idx2_PrV3i(Idx2->Dims3), idx2_PrV3i(Idx2->BrickDims3));
  if (!(Idx2->NLevels <= idx2_file::MaxLevels))
    return idx2_Error(idx2_file_err_code::TooManyIterations, "Max # of levels = %d\n", Idx2->MaxLevels);

  char TformOrder[8] = {};
  { /* compute the transform order (try to repeat XYZ++) */
    int J = 0;
    idx2_For(int, D, 0, 3) { if (Idx2->BrickDims3[D] > 1) TformOrder[J++] = char('X' + D); }
    TformOrder[J++] = '+';
    TformOrder[J++] = '+';
    Idx2->TformOrder = EncodeTransformOrder(TformOrder);
    Idx2->TformOrderFull.Len = DecodeTransformOrder(Idx2->TformOrder, Idx2->NTformPasses, Idx2->TformOrderFull.Data);
  }

  { /* build the subbands */
    Idx2->BrickDimsExt3 = idx2_ExtDims(Idx2->BrickDims3);
    BuildSubbands(Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TformOrder, &Idx2->Subbands);
    BuildSubbands(Idx2->BrickDims3, Idx2->NTformPasses, Idx2->TformOrder, &Idx2->SubbandsNonExt);
  }

  { /* compute number of bricks per level */
    Idx2->GroupBrick3 = Idx2->BrickDims3 / Dims(Idx2->SubbandsNonExt[0].Grid);
    v3i NBricks3 = (Idx2->Dims3 + Idx2->BrickDims3 - 1) / Idx2->BrickDims3;
    v3i NBricksIter3 = NBricks3;
    idx2_For(int, I, 0, Idx2->NLevels) {
      Idx2->NBricks3s[I] = NBricksIter3;
      NBricksIter3 = (NBricksIter3 + Idx2->GroupBrick3 - 1) / Idx2->GroupBrick3;
    }
  }

  { /* compute the brick order, by repeating the (per brick) transform order */
    Resize(&Idx2->BrickOrderStrs, Idx2->NLevels);
    idx2_For(int, I, 0, Idx2->NLevels) {
      v3i N3 = Idx2->NBricks3s[I];
      v3i LogN3 = v3i(Log2Ceil(N3.X), Log2Ceil(N3.Y), Log2Ceil(N3.Z));
      int MinLogN3 = Min(LogN3.X, LogN3.Y, LogN3.Z);
      v3i LeftOver3 = LogN3 - v3i(Idx2->BrickDims3.X > 1, Idx2->BrickDims3.Y > 1, Idx2->BrickDims3.Z > 1) * MinLogN3;
      char BrickOrder[128];
      int J = 0;
      idx2_For(int, D, 0, 3) { if (Idx2->BrickDims3[D] == 1) { while (LeftOver3[D]-- > 0) BrickOrder[J++] = char('X' + D); } }
      while (!(LeftOver3 <= 0)) { idx2_For(int, D, 0, 3) { if (LeftOver3[D]-- > 0) BrickOrder[J++] = char('X' + D); } }
      if (J > 0) BrickOrder[J++] = '+';
      idx2_For(size_t, K, 0, sizeof(TformOrder)) BrickOrder[J++] = TformOrder[K];
      Idx2->BrickOrders[I] = EncodeTransformOrder(BrickOrder);
      Idx2->BrickOrderStrs[I].Len = DecodeTransformOrder(Idx2->BrickOrders[I], N3, Idx2->BrickOrderStrs[I].Data);
      idx2_Assert(Idx2->BrickOrderStrs[I].Len == Idx2->BrickOrderStrs[0].Len - I * Idx2->TformOrderFull.Len);
      if (Idx2->BrickOrderStrs[I].Len < Idx2->TformOrderFull.Len) return idx2_Error(idx2_file_err_code::TooManyIterations);
    }
    idx2_For(int, I, 1, Idx2->NLevels) {
      i8 Len = Idx2->BrickOrderStrs[I].Len - Idx2->TformOrderFull.Len;
      if (!(stref((Idx2->BrickOrderStrs[I].Data + Len), Idx2->TformOrderFull.Len) ==
            stref(Idx2->TformOrderFull.Data, Idx2->TformOrderFull.Len)))
        return idx2_Error(idx2_file_err_code::TooManyIterationsOrTransformPasses);
    }
  }

  { /* compute BricksPerChunk3 and BrickOrderChunks */
    Idx2->ChunksPerFiles[0] = Idx2->ChunksPerFileIn;
    Idx2->BricksPerChunks[0] = Idx2->BricksPerChunkIn;
    if (!(Idx2->BricksPerChunks[0] <= idx2_file::MaxBricksPerChunk)) return idx2_Error(idx2_file_err_code::TooManyBricksPerChunk);
    if (!IsPow2(Idx2->BricksPerChunks[0])) return idx2_Error(idx2_file_err_code::BricksPerChunkNotPowerOf2);
    if (!(Idx2->ChunksPerFiles[0] <= idx2_file::MaxChunksPerFile)) return idx2_Error(idx2_file_err_code::TooManyChunksPerFile);
    if (!IsPow2(Idx2->ChunksPerFiles[0])) return idx2_Error(idx2_file_err_code::ChunksPerFileNotPowerOf2);
    idx2_For(int, I, 0, Idx2->NLevels) {
      Idx2->BricksPerChunks[I] = 1 << Min((u8)Log2Ceil(Idx2->BricksPerChunks[0]), Idx2->BrickOrderStrs[I].Len);
      stack_string<64> BrickOrderChunk;
      BrickOrderChunk.Len = Log2Ceil(Idx2->BricksPerChunks[I]);
      Idx2->BricksPerChunk3s[I] = v3i(1);
      idx2_For(int, J, 0, BrickOrderChunk.Len) {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - J - 1];
        Idx2->BricksPerChunk3s[I][C - 'X'] *= 2;
        BrickOrderChunk[BrickOrderChunk.Len - J - 1] = C;
      }
      Idx2->BrickOrderChunks[I] = EncodeTransformOrder(stref(BrickOrderChunk.Data, BrickOrderChunk.Len));
      idx2_Assert(Idx2->BricksPerChunks[I] = Prod(Idx2->BricksPerChunk3s[I]));
      Idx2->NChunks3s[I] = (Idx2->NBricks3s[I] + Idx2->BricksPerChunk3s[I] - 1) / Idx2->BricksPerChunk3s[I];
      /* compute ChunksPerFile3 and ChunkOrderFiles */
      Idx2->ChunksPerFiles[I] = 1 << Min((u8)Log2Ceil(Idx2->ChunksPerFiles[0]), (u8)(Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len));
      idx2_Assert(Idx2->BrickOrderStrs[I].Len >= BrickOrderChunk.Len);
      stack_string<64> ChunkOrderFile;
      ChunkOrderFile.Len = Log2Ceil(Idx2->ChunksPerFiles[I]);
      Idx2->ChunksPerFile3s[I] = v3i(1);
      idx2_For(int, J, 0, ChunkOrderFile.Len) {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len - J - 1];
        Idx2->ChunksPerFile3s[I][C - 'X'] *= 2;
        ChunkOrderFile[ChunkOrderFile.Len - J - 1] = C;
      }
      Idx2->ChunkOrderFiles[I] = EncodeTransformOrder(stref(ChunkOrderFile.Data, ChunkOrderFile.Len));
      idx2_Assert(Idx2->ChunksPerFiles[I] == Prod(Idx2->ChunksPerFile3s[I]));
      Idx2->NFiles3s[I] = (Idx2->NChunks3s[I] + Idx2->ChunksPerFile3s[I] - 1) / Idx2->ChunksPerFile3s[I];
      /* compute ChunkOrders */
      stack_string<64> ChunkOrder;
      Idx2->ChunksPerVol[I] = 1 << (Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len);
      idx2_Assert(Idx2->BrickOrderStrs[I].Len >= BrickOrderChunk.Len);
      ChunkOrder.Len = Log2Ceil(Idx2->ChunksPerVol[I]);
      idx2_For(int, J, 0, ChunkOrder.Len) {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len - J - 1];
        ChunkOrder[ChunkOrder.Len - J- 1] = C;
      }
      Idx2->ChunkOrders[I] = EncodeTransformOrder(stref(ChunkOrder.Data, ChunkOrder.Len));
      Resize(&Idx2->ChunkOrderStrs, Idx2->NLevels);
      Idx2->ChunkOrderStrs[I].Len = DecodeTransformOrder(Idx2->ChunkOrders[I], Idx2->NChunks3s[I], Idx2->ChunkOrderStrs[I].Data);
      /* compute FileOrders */
      stack_string<64> FileOrder;
      Idx2->FilesPerVol[I] = 1 << (Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len - ChunkOrderFile.Len);
      idx2_Assert(Idx2->BrickOrderStrs[I].Len >= BrickOrderChunk.Len + ChunkOrderFile.Len);
      FileOrder.Len = Log2Ceil(Idx2->FilesPerVol[I]);
      idx2_For(int, J, 0, FileOrder.Len) {
        char C = Idx2->BrickOrderStrs[I][Idx2->BrickOrderStrs[I].Len - BrickOrderChunk.Len - ChunkOrderFile.Len - J - 1];
        FileOrder[FileOrder.Len - J- 1] = C;
      }
      Idx2->FileOrders[I] = EncodeTransformOrder(stref(FileOrder.Data, FileOrder.Len));
      Resize(&Idx2->FileOrderStrs, Idx2->NLevels);
      Idx2->FileOrderStrs[I].Len = DecodeTransformOrder(Idx2->FileOrders[I], Idx2->NFiles3s[I], Idx2->FileOrderStrs[I].Data);
    }
  }

  { /* compute spatial depths */
    if (!(Idx2->FilesPerDir <= idx2_file::MaxFilesPerDir)) return idx2_Error(idx2_file_err_code::TooManyFilesPerDir, "%d", Idx2->FilesPerDir);
    idx2_For(int, I, 0, Idx2->NLevels) {
      Idx2->BricksPerFiles[I] = Idx2->BricksPerChunks[I] * Idx2->ChunksPerFiles[I];
      Idx2->FileDirDepths[I].Len = 0;
      i8 DepthAccum = Idx2->FileDirDepths[I][Idx2->FileDirDepths[I].Len++] = Log2Ceil(Idx2->BricksPerFiles[I]);
      i8 Len = Idx2->BrickOrderStrs[I].Len/* - Idx2->TformOrderFull.Len*/;
      while (DepthAccum < Len) {
        i8 Inc = Min(i8(Len - DepthAccum), Log2Ceil(Idx2->FilesPerDir));
        DepthAccum += (Idx2->FileDirDepths[I][Idx2->FileDirDepths[I].Len++] = Inc);
      }
      if (Idx2->FileDirDepths[I].Len > idx2_file::MaxSpatialDepth) return idx2_Error(idx2_file_err_code::TooManyFilesPerDir);
      Reverse(Begin(Idx2->FileDirDepths[I]), Begin(Idx2->FileDirDepths[I]) + Idx2->FileDirDepths[I].Len);
    }
  }

  { /* compute number of chunks per level */
    Idx2->GroupBrick3 = Idx2->BrickDims3 / Dims(Idx2->SubbandsNonExt[0].Grid);
    v3i NBricks3 = (Idx2->Dims3 + Idx2->BrickDims3 - 1) / Idx2->BrickDims3;
    v3i NBricksIter3 = NBricks3;
    idx2_For(int, I, 0, Idx2->NLevels) {
      Idx2->NBricks3s[I] = NBricksIter3;
      NBricksIter3 = (NBricksIter3 + Idx2->GroupBrick3 - 1) / Idx2->GroupBrick3;
    }
  }
  ComputeTransformDetails(&Idx2->Td, Idx2->BrickDimsExt3, Idx2->NTformPasses, Idx2->TformOrder);
  int NLevels = Log2Floor(Max(Max(Idx2->BrickDims3.X, Idx2->BrickDims3.Y), Idx2->BrickDims3.Z));
  ComputeTransformDetails(&Idx2->TdExtrpolate, Idx2->BrickDims3, NLevels, Idx2->TformOrder);

  /* compute the actual number of bytes for each rdo level */
  i64 TotalUncompressedSize = Prod<i64>(Idx2->Dims3) * SizeOf(Idx2->DType);
  Reserve(&Idx2->RdoLevels, Size(Idx2->QualityLevelsIn));
  idx2_ForEach(It, Idx2->QualityLevelsIn) {
    PushBack(&Idx2->RdoLevels, TotalUncompressedSize / *It);
  }

  return idx2_Error(idx2_file_err_code::NoError);
}

void Dealloc(idx2_file* Idx2) {
  Dealloc(&Idx2->BrickOrderStrs);
  Dealloc(&Idx2->ChunkOrderStrs);
  Dealloc(&Idx2->FileOrderStrs);
  Dealloc(&Idx2->Subbands);
  Dealloc(&Idx2->SubbandsNonExt);
  Dealloc(&Idx2->QualityLevelsIn);
  Dealloc(&Idx2->RdoLevels);
}

// TODO: make sure the wavelet normalization works across levels
// TODO: progressive decoding (maintaning a buffer (something like a FIFO queue) for the bricks)
// TODO: add a mode that treats the chunks like a row in a table

/* Write the metadata file (idx) */
// TODO: return error type
void
WriteMetaFile(const idx2_file& Idx2, const params& P, cstr FileName) {
  FILE* Fp = fopen(FileName, "w");
  fprintf(Fp, "(\n"); // begin (
  fprintf(Fp, "  (common\n");
  fprintf(Fp, "    (type \"Simulation\")\n"); // TODO: add this config to Idx2
  fprintf(Fp, "    (name \"%s\")\n", P.Meta.Name);
  fprintf(Fp, "    (field \"%s\")\n", P.Meta.Field);
  fprintf(Fp, "    (dimensions %d %d %d)\n", idx2_PrV3i(Idx2.Dims3));
  stref DType = ToString(Idx2.DType);
  fprintf(Fp, "    (data-type \"%s\")\n", idx2_PrintScratchN(Size(DType), "%s", DType.ConstPtr));
  fprintf(Fp, "    (min-max %.20f %.20f)\n", Idx2.ValueRange.Min, Idx2.ValueRange.Max);
  fprintf(Fp, "    (accuracy %.20f)\n", Idx2.Accuracy);
  fprintf(Fp, "  )\n"); // end common)
  fprintf(Fp, "  (format\n");
  fprintf(Fp, "    (version %d %d)\n", Idx2.Version[0], Idx2.Version[1]);
  fprintf(Fp, "    (brick-size %d %d %d)\n", idx2_PrV3i(Idx2.BrickDims3));
  char TransformOrder[128];
  DecodeTransformOrder(Idx2.TformOrder, TransformOrder);
  fprintf(Fp, "    (transform-order \"%s\")\n", TransformOrder);
  fprintf(Fp, "    (num-levels %d)\n", Idx2.NLevels);
  fprintf(Fp, "    (transform-passes-per-levels %d)\n", Idx2.NTformPasses);
  fprintf(Fp, "    (bricks-per-tile %d)\n", Idx2.BricksPerChunkIn);
  fprintf(Fp, "    (tiles-per-file %d)\n", Idx2.ChunksPerFileIn);
  fprintf(Fp, "    (files-per-directory %d)\n", Idx2.FilesPerDir);
  fprintf(Fp, "    (group-levels %s)\n", Idx2.GroupLevels ? "true" : "false");
  fprintf(Fp, "    (group-sub-levels %s)\n", Idx2.GroupSubLevels ? "true" : "false");
  fprintf(Fp, "    (group-bit-planes %s)\n", Idx2.GroupBitPlanes ? "true" : "false");
  if (Size(Idx2.QualityLevelsIn) > 0) {
    fprintf(Fp, "    (quality-levels %d", (int)Size(Idx2.QualityLevelsIn));
    idx2_ForEach(QIt, Idx2.QualityLevelsIn) { fprintf(Fp, " %d", *QIt); }
    fprintf(Fp, ")\n");
  }
  fprintf(Fp, "  )\n"); // end format)
  fprintf(Fp, ")\n"); // end )
  fclose(Fp);
}

// TODO: return error type
error<idx2_file_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName) {
  buffer Buf;
  idx2_CleanUp(DeallocBuf(&Buf));
  idx2_PropagateIfError(ReadFile(FileName, &Buf));
  SExprResult Result = ParseSExpr((cstr)Buf.Data, Size(Buf), nullptr);
  if(Result.type == SE_SYNTAX_ERROR) {
    fprintf(stderr, "Error(%d): %s.\n", Result.syntaxError.lineNumber, Result.syntaxError.message);
    return idx2_Error(idx2_file_err_code::SyntaxError);
  } else {
    SExpr* Data = (SExpr*)malloc(sizeof(SExpr) * Result.count);
    idx2_CleanUp(free(Data));
    array<SExpr*> Stack; Reserve(&Stack, Result.count);
    idx2_CleanUp(Dealloc(&Stack));
    // This time we supply the pool
    SExprPool Pool = { Result.count, Data };
    Result = ParseSExpr((cstr)Buf.Data, Size(Buf), &Pool);
    // result.expr contains the successfully parsed SExpr
//    printf("parse .idx file successfully\n");
    PushBack(&Stack, Result.expr);
    bool GotId = false;
    SExpr* LastExpr = nullptr;
    while (Size(Stack) > 0) {
      SExpr* Expr = Back(Stack);
      PopBack(&Stack);
      if (Expr->next)
        PushBack(&Stack, Expr->next);
      if (GotId) {
        if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "version")) {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Version[0] = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Version[1] = Expr->i;
//          printf("Version = %d.%d\n", Idx2->Version[0], Idx2->Version[1]);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "name")) {
          idx2_Assert(Expr->type == SE_STRING);
          snprintf(Idx2->Name, Expr->s.len + 1, "%s", (cstr)Buf.Data + Expr->s.start);
//          printf("Name = %s\n", Idx2->Name);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "field")) {
          idx2_Assert(Expr->type == SE_STRING);
          snprintf(Idx2->Field, Expr->s.len + 1, "%s", (cstr)Buf.Data + Expr->s.start);
//          printf("Field = %s\n", Idx2->Field);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "dimensions")) {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Dims3.X = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Dims3.Y = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Dims3.Z = Expr->i;
//          printf("Dims = %d %d %d\n", idx2_PrV3i(Idx2->Dims3));
        } if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "accuracy")) {
          idx2_Assert(Expr->type == SE_FLOAT);
          Idx2->Accuracy = Expr->f;
//          printf("Accuracy = %.17g\n", Idx2->Accuracy);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "data-type")) {
          idx2_Assert(Expr->type == SE_STRING);
          Idx2->DType = StringTo<dtype>()(stref((cstr)Buf.Data + Expr->s.start, Expr->s.len));
//          printf("Data type = %.*s\n", ToString(Idx2->DType).Size, ToString(Idx2->DType).ConstPtr);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "min-max")) {
          idx2_Assert(Expr->type == SE_FLOAT || Expr->type == SE_INT);
          Idx2->ValueRange.Min = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_FLOAT || Expr->type == SE_INT);
          Idx2->ValueRange.Max = Expr->i;
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "brick-size")) {
          v3i BrickDims3(0);
          idx2_Assert(Expr->type == SE_INT);
          BrickDims3.X = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          BrickDims3.Y = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          BrickDims3.Z = Expr->i;
          SetBrickSize(Idx2, BrickDims3);
//          printf("Brick size %d %d %d\n", idx2_PrV3i(Idx2->BrickDims3));
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "transform-order")) {
          idx2_Assert(Expr->type == SE_STRING);
          Idx2->TformOrder = EncodeTransformOrder(stref((cstr)Buf.Data + Expr->s.start, Expr->s.len));
          char TransformOrder[128];
          DecodeTransformOrder(Idx2->TformOrder, TransformOrder);
//          printf("Transform order = %s\n", TransformOrder);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "num-levels")) {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->NLevels = i8(Expr->i);
//          printf("Num levels = %d\n", Idx2->NLevels);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "bricks-per-tile")) {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->BricksPerChunkIn = Expr->i;
//          printf("Bricks per chunk = %d\n", Idx2->BricksPerChunks[0]);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "tiles-per-file")) {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->ChunksPerFileIn = Expr->i;
//          printf("Chunks per file = %d\n", Idx2->ChunksPerFiles[0]);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "files-per-directory")) {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->FilesPerDir = Expr->i;
//          printf("Files per directory = %d\n", Idx2->FilesPerDir);
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "group-levels")) {
          idx2_Assert(Expr->type == SE_BOOL);
          Idx2->GroupLevels = Expr->i;
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "group-sub-levels")) {
          idx2_Assert(Expr->type == SE_BOOL);
          Idx2->GroupSubLevels = Expr->i;
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "group-bit-planes")) {
          idx2_Assert(Expr->type == SE_BOOL);
          Idx2->GroupBitPlanes = Expr->i;
        } else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "quality-levels")) {
          int NumQualityLevels = Expr->i;
          Resize(&Idx2->QualityLevelsIn, NumQualityLevels);
          idx2_For(int, I, 0, NumQualityLevels) {
            Idx2->QualityLevelsIn[I] = Expr->i;
            Expr = Expr->next;
          }
        }
      }
      if (Expr->type == SE_ID) {
        LastExpr = Expr;
        GotId = true;
      } else if (Expr->type == SE_LIST) {
        PushBack(&Stack, Expr->head);
        GotId = false;
      } else {
        GotId = false;
      }
    }
  }
  return idx2_Error(idx2_file_err_code::NoError);
}

static void
Init(channel* C) {
  InitWrite(&C->BrickStream, 16384);
  InitWrite(&C->BrickDeltasStream, 32);
  InitWrite(&C->BrickSzsStream, 256);
  InitWrite(&C->BlockStream, 256);
}

static void
Dealloc(channel* C) {
  Dealloc(&C->BrickDeltasStream);
  Dealloc(&C->BrickSzsStream);
  Dealloc(&C->BrickStream);
  Dealloc(&C->BlockStream);
}

static void
Init(sub_channel* Sc) {
  InitWrite(&Sc->BlockEMaxesStream, 64);
  InitWrite(&Sc->BrickEMaxesStream, 8192);
}

static void
Dealloc(sub_channel* Sc) {
  Dealloc(&Sc->BlockEMaxesStream);
  Dealloc(&Sc->BrickEMaxesStream);
}

struct index_key {
  u64 LinearBrick;
  u32 BitStreamKey; // key consisting of bit plane, level, and sub-level
  idx2_Inline bool operator==(const index_key& Other) const {
    return LinearBrick == Other.LinearBrick &&  BitStreamKey == Other.BitStreamKey;
  }
};

struct brick_index {
  u64 LinearBrick = 0;
  u64 Offset = 0;
};

idx2_Inline u64
Hash(const index_key& IdxKey) {
  return (IdxKey.LinearBrick + 1) * (1 + IdxKey.BitStreamKey);
}

static u64
GetLinearBrick(const idx2_file& Idx2, int Iter, v3i Brick3) {
  u64 LinearBrick = 0;
  int Size = Idx2.BrickOrderStrs[Iter].Len;
  for (int I = Size - 1; I >= 0; --I) {
    int D = Idx2.BrickOrderStrs[Iter][I] - 'X';
    LinearBrick |= (Brick3[D] & u64(1)) << (Size - I - 1);
    Brick3[D] >>= 1;
  }
  return LinearBrick;
}

/* only used for debugging
static v3i
GetSpatialBrick(const idx2_file& Idx2, int Iter, u64 LinearBrick) {
  int Size = Idx2.BrickOrderStrs[Iter].Len;
  v3i Brick3(0);
  for (int I = 0; I < Size; ++I) {
    int D = Idx2.BrickOrderStrs[Iter][I] - 'X';
    int J = Size - I - 1;
    Brick3[D] |= (LinearBrick & (u64(1) << J)) >> J;
    Brick3[D] <<= 1;
  }
  return Brick3 >> 1;
}*/

/* only used for debugging
static u64
GetLinearChunk(const idx2_file& Idx2, int Iter, v3i Chunk3) {
  u64 LinearChunk = 0;
  int Size = Idx2.ChunkOrderStrs[Iter].Len;
  for (int I = Size - 1; I >= 0; --I) {
    int D = Idx2.ChunkOrderStrs[Iter][I] - 'X';
    LinearChunk |= (Chunk3[D] & u64(1)) << (Size - I - 1);
    Chunk3[D] >>= 1;
  }
  return LinearChunk;
}*/

static file_id ConstructFilePath(const idx2_file& Idx2, u64 Brick, i8 Iter, i8 Level, i16 BitPlane);
static file_id ConstructFilePathExponents(const idx2_file& Idx2, u64 Brick, i8 Iter, i8 Level);

/* used only for debugging
static u64
GetLinearFile(const idx2_file& Idx2, int Iter, v3i File3) {
  u64 LinearChunk = 0;
  int Size = Idx2.FileOrderStrs[Iter].Len;
  for (int I = Size - 1; I >= 0; --I) {
    int D = Idx2.FileOrderStrs[Iter][I] - 'X';
    LinearChunk |= (File3[D] & u64(1)) << (Size - I - 1);
    File3[D] >>= 1;
  }
  return LinearChunk;
}*/

// TODO: write a struct to help with bit packing / unpacking so we don't have to manually edit these
// TODO: make the following inline
static u64
GetFileAddressExp(int BricksPerFile, u64 Brick, i8 Iter, i8 Level) {
  return (u64(Iter) << 60) + // 4 bits
         u64((Brick >> Log2Ceil(BricksPerFile)) << 18) + // 42 bits
         (u64(Level) << 12); // 6 bits
}

static u64
GetFileAddressRdo(int BricksPerFile, u64 Brick, i8 Iter) {
  return (u64(Iter) << 60) + // 4 bits
         u64((Brick >> Log2Ceil(BricksPerFile)) << 18); // 42 bits
}


static u64
GetFileAddress(const idx2_file& Idx2, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  return (Idx2.GroupLevels ? 0 : u64(Iter) << 60) + // 4 bits
         u64((Brick >> Log2Ceil(Idx2.BricksPerFiles[Iter])) << 18) + // 42 bits
         (Idx2.GroupSubLevels ? 0 : u64(Level) << 12) + // 6 bits
         (Idx2.GroupBitPlanes ? 0 : u64(BitPlane) & 0xFFF); // 12 bits
}

static u64
GetChunkAddress(const idx2_file& Idx2, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  return (u64(Iter) << 60) + // 4 bits
         u64((Brick >> Log2Ceil(Idx2.BricksPerChunks[Iter])) << 18) + // 42 bits
         (u64(Level << 12)) + // 6 bits
         (u64(BitPlane) & 0xFFF); // 12 bits
}

static file_id
ConstructFilePath(const idx2_file& Idx2, u64 BrickAddress) {
  i16 BitPlane = i16(BrickAddress & 0xFFF);
  i8 Level = (BrickAddress >> 12) & 0x3F;
  i8 Iter = (BrickAddress >> 60) & 0xF;
  u64 Brick = ((BrickAddress >> 18) & 0x3FFFFFFFFFFull) << Log2Ceil(Idx2.BricksPerFiles[Iter]);
  return ConstructFilePath(Idx2, Brick, Iter, Level, BitPlane);
}

static idx2_Inline file_id
ConstructFilePathExponents(const idx2_file& Idx2, u64 BrickAddress) {
  i8 Level = (BrickAddress >> 12) & 0x3F;
  i8 Iter = (BrickAddress >> 60) & 0xF;
  u64 Brick = ((BrickAddress >> 18) & 0x3FFFFFFFFFFull) << Log2Ceil(Idx2.BricksPerFiles[Iter]);
  return ConstructFilePathExponents(Idx2, Brick, Iter, Level);
}

static file_id
ConstructFilePath(const idx2_file& Idx2, u64 Brick, i8 Level, i8 SubLevel, i16 BitPlane) {
  #define idx2_PrintLevel idx2_Print(&Pr, "/L%02x", Level);
  #define idx2_PrintBrick\
    for (int Depth = 0; Depth + 1 < Idx2.FileDirDepths[Level].Len; ++Depth) {\
      int BitLen = idx2_BitSizeOf(u64) - Idx2.BrickOrderStrs[Level].Len + Idx2.FileDirDepths[Level][Depth];\
      idx2_Print(&Pr, "/B%" PRIx64, TakeFirstBits(Brick, BitLen));\
      Brick <<= Idx2.FileDirDepths[Level][Depth];\
      Shift += Idx2.FileDirDepths[Level][Depth];\
    }
  #define idx2_PrintSubLevel idx2_Print(&Pr, "/S%02x", SubLevel);
  #define idx2_PrintBitPlane idx2_Print(&Pr, "/P%04hx", BitPlane);
  #define idx2_PrintExtension idx2_Print(&Pr, ".bin");
  u64 BrickBackup = Brick;
  int Shift = 0;
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%s/%s/%s/BrickData/", Idx2.Dir, Idx2.Name, Idx2.Field);
  if (!Idx2.GroupBitPlanes) { idx2_PrintBitPlane; }
  if (!Idx2.GroupLevels) { idx2_PrintLevel; }
  if (!Idx2.GroupSubLevels) { idx2_PrintSubLevel; }
  idx2_PrintBrick;
  idx2_PrintExtension;
  u64 FileId = GetFileAddress(Idx2, BrickBackup, Level, SubLevel, BitPlane);
  return file_id{stref{FilePath, Pr.Size}, FileId};
  #undef idx2_PrintLevel
  #undef idx2_PrintBrick
  #undef idx2_PrintSubLevel
  #undef idx2_PrintBitPlane
  #undef idx2_PrintExtension
}

static file_id
ConstructFilePathExponents(const idx2_file& Idx2, u64 Brick, i8 Level, i8 SubLevel) {
  #define idx2_PrintLevel idx2_Print(&Pr, "/L%02x", Level);
  #define idx2_PrintBrick\
    for (int Depth = 0; Depth + 1 < Idx2.FileDirDepths[Level].Len; ++Depth) {\
      int BitLen = idx2_BitSizeOf(u64) - Idx2.BrickOrderStrs[Level].Len + Idx2.FileDirDepths[Level][Depth];\
      idx2_Print(&Pr, "/B%" PRIx64, TakeFirstBits(Brick, BitLen));\
      Brick <<= Idx2.FileDirDepths[Level][Depth];\
      Shift += Idx2.FileDirDepths[Level][Depth];\
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
  u64 FileId = GetFileAddressExp(Idx2.BricksPerFiles[Level], BrickBackup, Level, SubLevel);
  return file_id{stref{FilePath, Pr.Size}, FileId};
  #undef idx2_PrintLevel
  #undef idx2_PrintSubLevel
  #undef idx2_PrintBrick
  #undef idx2_PrintExtension
}

static file_id
ConstructFilePathRdos(const idx2_file& Idx2, u64 Brick, i8 Level) {
  #define idx2_PrintLevel idx2_Print(&Pr, "/L%02x", Level);
  #define idx2_PrintBrick\
    for (int Depth = 0; Depth + 1 < Idx2.FileDirDepths[Level].Len; ++Depth) {\
      int BitLen = idx2_BitSizeOf(u64) - Idx2.BrickOrderStrs[Level].Len + Idx2.FileDirDepths[Level][Depth];\
      idx2_Print(&Pr, "/B%" PRIx64, TakeFirstBits(Brick, BitLen));\
      Brick <<= Idx2.FileDirDepths[Level][Depth];\
      Shift += Idx2.FileDirDepths[Level][Depth];\
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
  u64 FileId = GetFileAddressRdo(Idx2.BricksPerFiles[Level], BrickBackup, Level);
  return file_id{stref{FilePath, Pr.Size}, FileId};
  #undef idx2_PrintLevel
  #undef idx2_PrintBrick
  #undef idx2_PrintExtension
}

void Dealloc(chunk_meta_info* Cm) {
  Dealloc(&Cm->Addrs);
  Dealloc(&Cm->Sizes);
}

static void
Init(encode_data* E, allocator* Alloc = nullptr) {
  Init(&E->BrickPool, 9);
  Init(&E->Channels, 10);
  Init(&E->SubChannels, 5);
  E->Alloc = Alloc ? Alloc : &BrickAlloc_;
  Init(&E->ChunkMeta, 8);
  Init(&E->ChunkEMaxesMeta, 5);
  InitWrite(&E->CpresEMaxes, 32768);
  InitWrite(&E->CpresChunkAddrs, 16384);
  InitWrite(&E->ChunkStream, 16384);
  InitWrite(&E->ChunkEMaxesStream, 32768);
  Init(&E->ChunkRDOLengths, 10);
}

static void
Dealloc(encode_data* E) {
  E->Alloc->DeallocAll();
  Dealloc(&E->BrickPool);
  idx2_ForEach(ChannelIt, E->Channels) Dealloc(ChannelIt.Val);
  Dealloc(&E->Channels);
  idx2_ForEach(SubChannelIt, E->SubChannels) Dealloc(SubChannelIt.Val);
  Dealloc(&E->SubChannels);
  idx2_ForEach(ChunkMetaIt, E->ChunkMeta) Dealloc(ChunkMetaIt.Val);
  idx2_ForEach(ChunkEMaxesMetaIt, E->ChunkEMaxesMeta) Dealloc(ChunkEMaxesMetaIt.Val);
  Dealloc(&E->ChunkMeta);
  Dealloc(&E->CpresEMaxes);
  Dealloc(&E->CpresChunkAddrs);
  Dealloc(&E->ChunkStream);
  Dealloc(&E->ChunkEMaxesStream);
  Dealloc(&E->BlockSigs);
  Dealloc(&E->EMaxes);
  Dealloc(&E->BlockStream);
  Dealloc(&E->ChunkRDOs);
  Dealloc(&E->ChunkRDOLengths);
}

#define idx2_NextMorton(Morton, Row3, Dims3)\
  if (!(Row3 < Dims3)) {\
    int B = Lsb(Morton);\
    idx2_Assert(B >= 0);\
    Morton = (((Morton >> (B + 1)) + 1) << (B + 1)) - 1;\
    continue;\
  }

/* Upscale a single brick to a given resolution level */
// TODO: upscale across levels
idx2_T(t) static void
UpscaleBrick(
  const grid& Grid, int TformOrder, const brick<t>& Brick, int Level,
  const grid& OutGrid, volume* OutBrickVol)
{
  idx2_Assert(Level >= Brick.Level);
  idx2_Assert(OutBrickVol->Type == dtype::float64);
  v3i Dims3 = Dims(Grid);
  volume BrickVol(buffer((byte*)Brick.Samples, Prod(Dims3) * sizeof(f64)), Dims3, dtype::float64);
  if (Level > Brick.Level)
    *OutBrickVol = 0;
  Copy(Relative(Grid, Grid), BrickVol, Relative(Grid, OutGrid), OutBrickVol);
  if (Level > Brick.Level)
    InverseCdf53(Dims(*OutBrickVol), Dims(*OutBrickVol), Level - Brick.Level, TformOrder, OutBrickVol, true);
}

/* Flatten a brick table. the function allocates memory for its output. */
// TODO: upscale across levels
idx2_T(t) static void
FlattenBrickTable(
  const array<grid>& LevelGrids, int TformOrder, const brick_table<t>& BrickTable, volume* VolOut)
{
  idx2_Assert(Size(BrickTable.Bricks) > 0);
  /* determine the maximum level of all bricks in the table */
  int MaxLevel = 0;
  auto ItEnd = End(BrickTable.Bricks);
  for (auto It = Begin(BrickTable.Bricks); It != ItEnd; ++It) {
    int Iteration = *(It.Key) & 0xF;
    idx2_Assert(Iteration == 0); // TODO: for now we only support one level
    MaxLevel = Max(MaxLevel, (int)It.Val->Level);
  }
  /* allocate memory for VolOut */
  v3i DimsExt3 = Dims(LevelGrids[MaxLevel]);
  v3i Dims3 = idx2_NonExtDims(DimsExt3);
  idx2_Assert(IsPow2(Dims3.X) && IsPow2(Dims3.Y) && IsPow2(Dims3.Z));
  auto It = Begin(BrickTable.Bricks);
  extent Ext(DecodeMorton3(*(It.Key) >> 4));
  for (++It; It != ItEnd; ++It) {
    v3i P3 = DecodeMorton3(*(It.Key) >> 4);
    Ext = BoundingBox(Ext, extent(P3 * Dims3, Dims3));
  }
  Resize(VolOut, Dims(Ext));
  /* upscale every brick */
  volume BrickVol(DimsExt3, dtype::float64);
  idx2_CleanUp(Dealloc(&BrickVol));
  for (auto It = Begin(BrickTable.Bricks); It != ItEnd; ++It) {
    v3i P3 = DecodeMorton3(*(It.Key) >> 4);
    UpscaleBrick(LevelGrids[It.Val->Level], TformOrder, *(It.Val), MaxLevel, LevelGrids[MaxLevel], &BrickVol);
    Copy(extent(Dims3), BrickVol, extent(P3 * Dims3, Dims3), VolOut);
  }
}

/* book-keeping stuffs */
static stat BrickDeltasStat;
static stat BrickSzsStat;
static stat BrickStreamStat;
static stat ChunkStreamStat;

// TODO: return error
static void
WriteChunk(const idx2_file& Idx2, encode_data* E, channel* C, i8 Iter, i8 Level, i16 BitPlane) {
  BrickDeltasStat.Add((f64)Size(C->BrickDeltasStream)); // brick deltas
  BrickSzsStat.Add((f64)Size(C->BrickSzsStream)); // brick sizes
  BrickStreamStat.Add((f64)Size(C->BrickStream)); // brick data
  i64 ChunkSize = Size(C->BrickDeltasStream) + Size(C->BrickSzsStream) + Size(C->BrickStream) + 64;
  Rewind(&E->ChunkStream);
  GrowToAccomodate(&E->ChunkStream, ChunkSize);
  WriteVarByte(&E->ChunkStream, C->NBricks);
  WriteStream(&E->ChunkStream, &C->BrickDeltasStream);
  WriteStream(&E->ChunkStream, &C->BrickSzsStream);
  WriteStream(&E->ChunkStream, &C->BrickStream);
  Flush(&E->ChunkStream);
  ChunkStreamStat.Add((f64)Size(E->ChunkStream));

  /* we are done with these, rewind */
  Rewind(&C->BrickDeltasStream);
  Rewind(&C->BrickSzsStream);
  Rewind(&C->BrickStream);

  /* write to file */
  file_id FileId = ConstructFilePath(Idx2, C->LastBrick, Iter, Level, BitPlane);
  idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  WriteBuffer(Fp, ToBuffer(E->ChunkStream));
  /* keep track of the chunk addresses and sizes */
  auto ChunkMetaIt = Lookup(&E->ChunkMeta, FileId.Id);
  if (!ChunkMetaIt) {
    chunk_meta_info Cm;
    InitWrite(&Cm.Sizes, 128);
    Insert(&ChunkMetaIt, FileId.Id, Cm);
  }
  idx2_Assert(ChunkMetaIt);
  chunk_meta_info* ChunkMeta = ChunkMetaIt.Val;
  GrowToAccomodate(&ChunkMeta->Sizes, 4);
  WriteVarByte(&ChunkMeta->Sizes, Size(E->ChunkStream));
  u64 ChunkAddress = GetChunkAddress(Idx2, C->LastBrick, Iter, Level, BitPlane);
  PushBack(&ChunkMeta->Addrs, ChunkAddress);
//  printf("chunk %x level %d bit plane %d offset %llu size %d\n", ChunkAddress, Level, BitPlane, Where, (i64)Size(Channel->ChunkStream));
  PushBack(&E->ChunkRDOs, rdo_chunk{ChunkAddress, Size(E->ChunkStream), 0.0});
  Rewind(&E->ChunkStream);
}

stat BrickEMaxesStat;
stat ChunkEMaxesStat;

// Write once per chunk
static void
WriteChunkExponents(const idx2_file& Idx2, encode_data* E, sub_channel* Sc, i8 Iter, i8 Level) {
  /* brick exponents */
  Flush(&Sc->BrickEMaxesStream);
  BrickEMaxesStat.Add((f64)Size(Sc->BrickEMaxesStream));
  Rewind(&E->ChunkEMaxesStream);
  CompressBufZstd(ToBuffer(Sc->BrickEMaxesStream), &E->ChunkEMaxesStream);
  ChunkEMaxesStat.Add((f64)Size(E->ChunkEMaxesStream));

  /* rewind */
  Rewind(&Sc->BrickEMaxesStream);

  /* write to file */
  file_id FileId = ConstructFilePathExponents(Idx2, Sc->LastBrick, Iter, Level);
  idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  WriteBuffer(Fp, ToBuffer(E->ChunkEMaxesStream));
  /* keep track of the chunk sizes */
  auto ChunkEMaxesMetaIt = Lookup(&E->ChunkEMaxesMeta, FileId.Id);
  if (!ChunkEMaxesMetaIt) {
    bitstream ChunkEMaxSzs;
    InitWrite(&ChunkEMaxSzs, 128);
    Insert(&ChunkEMaxesMetaIt, FileId.Id, ChunkEMaxSzs);
  }
  bitstream* ChunkEMaxSzs = ChunkEMaxesMetaIt.Val;
  GrowToAccomodate(ChunkEMaxSzs, 4);
  WriteVarByte(ChunkEMaxSzs, Size(E->ChunkEMaxesStream));

  u64 ChunkAddress = GetChunkAddress(Idx2, Sc->LastBrick, Iter, Level, 0);
  Insert(&E->ChunkRDOLengths, ChunkAddress, (u32)Size(E->ChunkEMaxesStream));

  Rewind(&E->ChunkEMaxesStream);
}

struct sub_channel_ptr {
  i8 Iteration = 0;
  i8 Level = 0;
  sub_channel* ChunkEMaxesPtr = nullptr;
  idx2_Inline bool operator<(const sub_channel_ptr& Other) const {
    if (Iteration == Other.Iteration) return Level < Other.Level;
    return Iteration < Other.Iteration;
  }
};

stat ChunkEMaxSzsStat;

// TODO: check the error path
error<idx2_file_err_code>
FlushChunkExponents(const idx2_file& Idx2, encode_data* E) {
  idx2_ForEach(ScIt, E->SubChannels) {
    i8 Iteration = IterationFromChannelKey(*ScIt.Key);
    i8 Level = LevelFromChannelKey(*ScIt.Key);
    WriteChunkExponents(Idx2, E, ScIt.Val, Iteration, Level);
  }
  idx2_ForEach(CemIt, E->ChunkEMaxesMeta) {
    bitstream* ChunkEMaxSzs = CemIt.Val;
    file_id FileId = ConstructFilePathExponents(Idx2, *CemIt.Key);
    idx2_Assert(FileId.Id == *CemIt.Key);
    /* write chunk emax sizes */
    idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
    Flush(ChunkEMaxSzs);
    ChunkEMaxSzsStat.Add((f64)Size(*ChunkEMaxSzs));
    WriteBuffer(Fp, ToBuffer(*ChunkEMaxSzs));
    WritePOD(Fp, (int)Size(*ChunkEMaxSzs));
  }
  return idx2_Error(idx2_file_err_code::NoError);
}

#define idx2_FileTraverse(Body, StackSize, FileOrderIn, FileFrom3In, FileDims3In, ExtentInFiles, Extent2)\
  {\
    file_traverse FileStack[StackSize];\
    int FileTopIdx = 0;\
    v3i FileDims3Ext((int)NextPow2(FileDims3In.X), (int)NextPow2(FileDims3In.Y), (int)NextPow2(FileDims3In.Z));\
    FileStack[FileTopIdx] = file_traverse{FileOrderIn, FileOrderIn, FileFrom3In, FileFrom3In + FileDims3Ext, u64(0)};\
    while (FileTopIdx >= 0) {\
      file_traverse& FileTop = FileStack[FileTopIdx];\
      int FD = FileTop.FileOrder & 0x3;\
      FileTop.FileOrder >>= 2;\
      if (FD == 3) {\
        if (FileTop.FileOrder == 3) FileTop.FileOrder = FileTop.PrevOrder;\
        else FileTop.PrevOrder = FileTop.FileOrder;\
        continue;\
      }\
      --FileTopIdx;\
      if (FileTop.FileTo3 - FileTop.FileFrom3 == 1) {\
        { Body }\
        continue;\
      }\
      file_traverse First = FileTop, Second = FileTop;\
      First.FileTo3[FD] = FileTop.FileFrom3[FD] + (FileTop.FileTo3[FD] - FileTop.FileFrom3[FD]) / 2;\
      Second.FileFrom3[FD] = FileTop.FileFrom3[FD] + (FileTop.FileTo3[FD] - FileTop.FileFrom3[FD]) / 2;\
      extent Skip(First.FileFrom3, First.FileTo3 - First.FileFrom3);\
      First.Address = FileTop.Address;\
      Second.Address = FileTop.Address + Prod<u64>(First.FileTo3 - First.FileFrom3);\
      if (Second.FileFrom3 < To(ExtentInFiles) && From(ExtentInFiles) < Second.FileTo3) FileStack[++FileTopIdx] = Second;\
      if (First.FileFrom3 < To(ExtentInFiles) && From(ExtentInFiles) < First.FileTo3)   FileStack[++FileTopIdx] = First;\
    }\
  }

#define idx2_ChunkTraverse(Body, StackSize, ChunkOrderIn, ChunkFrom3In, ChunkDims3In, ExtentInChunks, Extent2)\
  {\
    chunk_traverse ChunkStack[StackSize];\
    int ChunkTopIdx = 0;\
    v3i ChunkDims3Ext((int)NextPow2(ChunkDims3In.X), (int)NextPow2(ChunkDims3In.Y), (int)NextPow2(ChunkDims3In.Z));\
    ChunkStack[ChunkTopIdx] = chunk_traverse{ChunkOrderIn, ChunkOrderIn, ChunkFrom3In, ChunkFrom3In + ChunkDims3Ext, u64(0)};\
    while (ChunkTopIdx >= 0) {\
      chunk_traverse& ChunkTop = ChunkStack[ChunkTopIdx];\
      int CD = ChunkTop.ChunkOrder & 0x3;\
      ChunkTop.ChunkOrder >>= 2;\
      if (CD == 3) {\
        if (ChunkTop.ChunkOrder == 3) ChunkTop.ChunkOrder = ChunkTop.PrevOrder;\
        else ChunkTop.PrevOrder = ChunkTop.ChunkOrder;\
        continue;\
      }\
      --ChunkTopIdx;\
      if (ChunkTop.ChunkTo3 - ChunkTop.ChunkFrom3 == 1) {\
        { Body }\
        continue;\
      }\
      chunk_traverse First = ChunkTop, Second = ChunkTop;\
      First.ChunkTo3[CD] = ChunkTop.ChunkFrom3[CD] + (ChunkTop.ChunkTo3[CD] - ChunkTop.ChunkFrom3[CD]) / 2;\
      Second.ChunkFrom3[CD] = ChunkTop.ChunkFrom3[CD] + (ChunkTop.ChunkTo3[CD] - ChunkTop.ChunkFrom3[CD]) / 2;\
      extent Skip(First.ChunkFrom3, First.ChunkTo3 - First.ChunkFrom3);\
      Second.NChunksBefore = First.NChunksBefore + Prod<u64>(Dims(Crop(Skip, ExtentInChunks)));\
      Second.ChunkInFile = First.ChunkInFile + Prod<i32>(Dims(Crop(Skip, Extent2)));\
      First.Address = ChunkTop.Address;\
      Second.Address = ChunkTop.Address + Prod<u64>(First.ChunkTo3 - First.ChunkFrom3);\
      if (Second.ChunkFrom3 < To(ExtentInChunks) && From(ExtentInChunks) < Second.ChunkTo3) ChunkStack[++ChunkTopIdx] = Second;\
      if (First.ChunkFrom3 < To(ExtentInChunks) && From(ExtentInChunks) < First.ChunkTo3)   ChunkStack[++ChunkTopIdx] = First;\
    }\
  }

#define idx2_BrickTraverse(Body, StackSize, BrickOrderIn, BrickFrom3In, BrickDims3In, ExtentInBricks, Extent2)\
  {\
    brick_traverse Stack[StackSize];\
    int TopIdx = 0;\
    v3i BrickDims3Ext((int)NextPow2(BrickDims3In.X), (int)NextPow2(BrickDims3In.Y), (int)NextPow2(BrickDims3In.Z));\
    Stack[TopIdx] = brick_traverse{BrickOrderIn, BrickOrderIn, BrickFrom3In, BrickFrom3In + BrickDims3Ext, u64(0)};\
    while (TopIdx >= 0) {\
      brick_traverse& Top = Stack[TopIdx];\
      int DD = Top.BrickOrder & 0x3;\
      Top.BrickOrder >>= 2;\
      if (DD == 3) {\
        if (Top.BrickOrder == 3) Top.BrickOrder = Top.PrevOrder;\
        else Top.PrevOrder = Top.BrickOrder;\
        continue;\
      }\
      --TopIdx;\
      if (Top.BrickTo3 - Top.BrickFrom3 == 1) {\
        { Body }\
        continue;\
      }\
      brick_traverse First = Top, Second = Top;\
      First.BrickTo3[DD] = Top.BrickFrom3[DD] + (Top.BrickTo3[DD] - Top.BrickFrom3[DD]) / 2;\
      Second.BrickFrom3[DD] = Top.BrickFrom3[DD] + (Top.BrickTo3[DD] - Top.BrickFrom3[DD]) / 2;\
      extent Skip(First.BrickFrom3, First.BrickTo3 - First.BrickFrom3);\
      Second.NBricksBefore = First.NBricksBefore + Prod<u64>(Dims(Crop(Skip, ExtentInBricks)));\
      Second.BrickInChunk = First.BrickInChunk + Prod<i32>(Dims(Crop(Skip, Extent2)));\
      First.Address = Top.Address;\
      Second.Address = Top.Address + Prod<u64>(First.BrickTo3 - First.BrickFrom3);\
      if (Second.BrickFrom3 < To(ExtentInBricks) && From(ExtentInBricks) < Second.BrickTo3) Stack[++TopIdx] = Second;\
      if (First.BrickFrom3 < To(ExtentInBricks) && From(ExtentInBricks) < First.BrickTo3)   Stack[++TopIdx] = First;\
    }\
  }

struct rdo_precompute {
  u64 Address;
  int Start;
  array<int> Hull; // convex hull
  array<int> TruncationPoints; // bit planes
  idx2_Inline bool operator<(const rdo_precompute& Other) const { return Address < Other.Address; }
};

struct rdo_mini {
  f64 Distortion;
  i64 Length;
  f64 Lambda;
};

/* Rate distortion optimization is done once per tile and iter/level */
// TODO: change the word "Chunk" to "Tile" elsewhere where it makes sense
// TODO: normalize the distortion by the number of samples a chunk has
static void
RateDistortionOpt(const idx2_file& Idx2, encode_data* E) {
  if (Size(Idx2.RdoLevels) == 0) return;
  constexpr u64 InfInt = 0x7FF0000000000000ull;
  const f64 Inf = *(f64*)(&InfInt);

  auto & ChunkRDOs = E->ChunkRDOs;
  i16 MinBitPlane = traits<i16>::Max, MaxBitPlane = traits<i16>::Min;
  idx2_ForEach(CIt, ChunkRDOs) {
    i16 BitPlane = i16(CIt->Address & 0xFFF);
    MinBitPlane = Min(MinBitPlane, BitPlane);
    MaxBitPlane = Max(MaxBitPlane, BitPlane);
  }
//  InsertionSort(Begin(ChunkRDOs), End(ChunkRDOs)); // TODO: this should be quicksort
  std::sort(Begin(ChunkRDOs), End(ChunkRDOs));
  array<rdo_precompute> RdoPrecomputes; Reserve(&RdoPrecomputes, 128); // each rdo_precompute corresponds to a tile
  idx2_CleanUp(
    idx2_ForEach(It, RdoPrecomputes) {
      Dealloc(&It->Hull);
      Dealloc(&It->TruncationPoints);
    }
    Dealloc(&RdoPrecomputes);
  );
  int TileStart = -1, TileEnd = -1;
  array<rdo_mini> RdoTile; idx2_CleanUp(Dealloc(&RdoTile)); // just for a single tile
  while (true) { // loop through all chunks and detect the tile boundaries (Start, End)
    TileStart = TileEnd + 1;
    if (TileStart >= Size(ChunkRDOs)) break;
    TileEnd = TileStart + 1; // exclusive end
    const auto & C = ChunkRDOs[TileStart];
    while (TileEnd < Size(ChunkRDOs) && (ChunkRDOs[TileEnd].Address >> 12) == (C.Address >> 12)) {
      ++TileEnd;
    }
    Clear(&RdoTile);
    Reserve(&RdoTile, TileEnd - TileStart + 1);
    i16 Bp = i16(ChunkRDOs[TileStart].Address & 0xFFF) + 1;
    PushBack(&RdoTile, rdo_mini{pow(2.0, Bp), 0, Inf});
    i64 PrevLength = 0;
    idx2_For(int, Z, TileStart, TileEnd) {
      u64 Addr = (ChunkRDOs[Z].Address >> 12) << 12;
      auto LIt = Lookup(&E->ChunkRDOLengths, Addr);
      idx2_Assert(LIt);
      i16 BitPlane = i16(ChunkRDOs[Z].Address & 0xFFF);
      PushBack(&RdoTile, rdo_mini{pow(2.0, BitPlane), PrevLength += ChunkRDOs[Z].Length, 0.0});
      ChunkRDOs[Z].Length = PrevLength + *LIt.Val;
    }
    array<int> Hull;
    Reserve(&Hull, Size(RdoTile)); PushBack(&Hull, 0);
    /* precompute the convex hull and lambdas */
    int HLast = 0;
    idx2_For(int, Z, 1, Size(RdoTile)) {
      f64 DeltaD = RdoTile[HLast].Distortion - RdoTile[Z].Distortion;
      f64 DeltaL = f64(RdoTile[Z].Length - RdoTile[HLast].Length);
      if (DeltaD > 0) {
        while (DeltaD >= DeltaL * RdoTile[HLast].Lambda) {
          idx2_Assert(Size(Hull) > 0);
          PopBack(&Hull);
          HLast = Back(Hull);
          DeltaD = RdoTile[HLast].Distortion - RdoTile[Z].Distortion;
          DeltaL = f64(RdoTile[Z].Length - RdoTile[HLast].Length);
        }
        HLast = Z; // TODO: or Z + 1?
        PushBack(&Hull, HLast);
        RdoTile[HLast].Lambda = DeltaD / DeltaL;
      }
    } // end Z loop
    idx2_Assert(TileEnd - TileStart + 1 == Size(RdoTile));
    idx2_For(int, Z, 1, Size(RdoTile)) {
      ChunkRDOs[Z + TileStart - 1].Lambda = RdoTile[Z].Lambda;
    }
    u64 Address = ChunkRDOs[TileStart].Address;
    PushBack(&RdoPrecomputes, rdo_precompute{Address, TileStart, Hull, array<int>()});
  }

  /* search for a suitable global lambda */
  idx2_For(int, R, 0, Size(Idx2.RdoLevels)) { // for all quality levels
    printf("optimizing for rdo level %d\n", R);
    f64 LowLambda = -2, HighLambda = -1, Lambda = 1;
    int Count = 0;
    do { // search for the best Lambda (TruncationPoints stores the output)
      if (LowLambda > 0 && HighLambda > 0) {
        ++Count;
        if (Count >= 20) { Lambda = HighLambda; break; }
        Lambda = 0.5 * LowLambda + 0.5 * HighLambda;
      } else if (Lambda == 0) {
        Lambda = HighLambda;
        break;
      }
      i64 TotalLength = 0;
      idx2_ForEach(TIt, RdoPrecomputes) { // for each tile
        int J = 0;
        while (J + 1 < Size(TIt->Hull)) {
          int Z = TIt->Hull[J + 1] + TIt->Start - 1;
          idx2_Assert(Z >= TIt->Start);
          if (ChunkRDOs[Z].Lambda > Lambda) { ++J; } else break;
        }
        Resize(&TIt->TruncationPoints, Size(Idx2.RdoLevels));
        TIt->TruncationPoints[R] = J;
        TotalLength += J == 0 ? 0 : ChunkRDOs[TIt->Hull[J] + TIt->Start - 1].Length;
      }
      if (TotalLength > Idx2.RdoLevels[R]) { // we overshot, need to increase lambda
        LowLambda = Lambda;
        if (HighLambda < 0) { Lambda *= 2; }
      } else { // we did not overshoot, need to decrease Lambda
        HighLambda = Lambda;
        if (LowLambda < 0) { Lambda *= 0.5; }
      }
      //idx2_Assert(LowLambda <= HighLambda);
    } while (true);
  }

  /* write the truncation points to files */
//  InsertionSort(Begin(RdoPrecomputes), End(RdoPrecomputes)); // TODO: quicksort
  std::sort(Begin(RdoPrecomputes), End(RdoPrecomputes));
  i64 Pos = 0;
  idx2_RAII(array<i16>, Buffer, Reserve(&Buffer, 128));
  idx2_RAII(bitstream, BitStream,);
  idx2_For(i8, Iter, 0, Idx2.NLevels) {
    extent Ext(Idx2.Dims3);
    v3i BrickDims3 = Idx2.BrickDims3 * Pow(Idx2.GroupBrick3, Iter);
    v3i BrickFirst3 = From(Ext) / BrickDims3;
    v3i BrickLast3 = Last(Ext) / BrickDims3;
    extent ExtentInBricks(BrickFirst3, BrickLast3 - BrickFirst3 + 1);
    v3i ChunkDims3 = Idx2.BricksPerChunk3s[Iter] * BrickDims3;
    v3i ChunkFirst3 = From(Ext) / ChunkDims3;
    v3i ChunkLast3 = Last(Ext) /  ChunkDims3;
    extent ExtentInChunks(ChunkFirst3, ChunkLast3 - ChunkFirst3 + 1);
    v3i FileDims3 = ChunkDims3 * Idx2.ChunksPerFile3s[Iter];
    v3i FileFirst3 = From(Ext) / FileDims3;
    v3i FileLast3 = Last(Ext) / FileDims3;
    extent ExtentInFiles(FileFirst3, FileLast3 - FileFirst3 + 1);
    extent VolExt(Idx2.Dims3);
    v3i VolBrickFirst3 = From(VolExt) / BrickDims3;
    v3i VolBrickLast3 = Last(VolExt)  / BrickDims3;
    extent VolExtentInBricks(VolBrickFirst3, VolBrickLast3 - VolBrickFirst3 + 1);
    v3i VolChunkFirst3 = From(VolExt) / ChunkDims3;
    v3i VolChunkLast3 = Last(VolExt) / ChunkDims3;
    extent VolExtentInChunks(VolChunkFirst3, VolChunkLast3 - VolChunkFirst3 + 1);
    v3i VolFileFirst3 = From(VolExt) / FileDims3;
    v3i VolFileLast3 = Last(VolExt) / FileDims3;
    extent VolExtentInFiles(VolFileFirst3, VolFileLast3 - VolFileFirst3 + 1);
    idx2_FileTraverse(
      u64 FileAddr = FileTop.Address;
      // int ChunkInFile = 0;
      u64 FirstBrickAddr = ((FileAddr * Idx2.ChunksPerFiles[Iter]) + 0) * Idx2.BricksPerChunks[Iter] + 0;
      file_id FileId = ConstructFilePathRdos(Idx2, FirstBrickAddr, Iter);
      int NumChunks = 0;
      idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
      idx2_ChunkTraverse(
        ++NumChunks;
        u64 ChunkAddr = (FileAddr * Idx2.ChunksPerFiles[Iter]) + ChunkTop.Address;
        // ChunkInFile = ChunkTop.ChunkInFile;
        idx2_For(i8, Level, 0, Size(Idx2.Subbands)) {
          const auto& Rdo = RdoPrecomputes[Pos];
          i8 RdoIter = (Rdo.Address >> 60) & 0xF;
          u64 RdoAddress = (Rdo.Address >> 18) & 0x3FFFFFFFFFFull;
          i8 RdoLevel = (Rdo.Address >> 12) & 0x3F;
          if (RdoIter == Iter && RdoLevel == Level && RdoAddress == ChunkAddr) {
            ++Pos;
            idx2_For(int, R, 0, Size(Idx2.RdoLevels)) { // for each quality level
              int J = Rdo.TruncationPoints[R];
              if (J == 0) { PushBack(&Buffer, traits<i16>::Max); continue; }
              int Z = Rdo.Hull[J] + Rdo.Start - 1;
              u64 Address = ChunkRDOs[Z].Address;
              idx2_Assert((Rdo.Address >> 12) == (Address >> 12));
              i16 BitPlane = i16(Address & 0xFFF);
              PushBack(&Buffer, BitPlane);
            }
          } else { // somehow the tile is not there (tile produces no chunk i.e. it compresses to 0 bits)
            idx2_For(int, R, 0, Size(Idx2.RdoLevels)) { // for each quality level
              PushBack(&Buffer, traits<i16>::Max);
            }
          }
        } // end level loop
        , 64
        , Idx2.ChunkOrderFiles[Iter]
        , FileTop.FileFrom3 * Idx2.ChunksPerFile3s[Iter]
        , Idx2.ChunksPerFile3s[Iter]
        , ExtentInChunks
        , VolExtentInChunks
      ); // end chunk (tile) traverse
      buffer Buf(Buffer.Buffer.Data, Size(Buffer) * sizeof(i16));
      CompressBufZstd(Buf, &BitStream);
      WriteBuffer(Fp, buffer{BitStream.Stream.Data, Size(BitStream)});
      WritePOD(Fp, NumChunks);
      fclose(Fp);
      Clear(&Buffer);
      Rewind(&BitStream);
      , 64
      , Idx2.FileOrders[Iter]
      , v3i(0)
      , Idx2.NFiles3s[Iter]
      , ExtentInFiles
      , VolExtentInFiles
    );
  } // end level loop
  idx2_Assert(Pos == Size(RdoPrecomputes));
}

static stat BlockStat;
static stat BlockEMaxStat;

// TODO: return an error code
static void
EncodeSubband(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol) {
  u64 Brick = E->Brick[E->Iter];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  const i8 NBitPlanes = idx2_BitSizeOf(u64);
  Clear(&E->BlockSigs); Reserve(&E->BlockSigs, NBitPlanes);
  Clear(&E->EMaxes); Reserve(&E->EMaxes, Prod(NBlocks3));

  /* query the right sub channel for the block exponents */
  u16 SubChanKey = GetSubChannelKey(E->Iter, E->Level);
  auto ScIt = Lookup(&E->SubChannels, SubChanKey);
  if (!ScIt) {
    sub_channel SubChan; Init(&SubChan);
    Insert(&ScIt, SubChanKey, SubChan);
  }
  idx2_Assert(ScIt);
  sub_channel* Sc = ScIt.Val;

  /* pass 1: compress the blocks */
  idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2->BlockDims3;
    v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
    const i8 NDims = (i8)NumDims(BlockDims3);
    const int NVals = 1 << (2 * NDims);
    const i8 Prec = NBitPlanes - 1 - NDims;
    f64 BlockFloats[4 * 4 * 4];
    buffer_t BufFloats(BlockFloats, NVals);
    buffer_t BufInts((i64*)BlockFloats, NVals);
    u64 BlockUInts [4 * 4 * 4];
    buffer_t BufUInts (BlockUInts , NVals);
    bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextIter) continue;
    /* copy the samples to the local buffer */
    v3i S3;
    int J = 0;
    v3i From3 = From(SbGrid), Strd3 = Strd(SbGrid);
    idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BlockFloats[J++] = BrickVol->At<f64>(From3, Strd3, D3 + S3);
    } idx2_EndFor3 // end sample loop
    /* zfp transform and shuffle */
    const i16 EMax = SizeOf(Idx2->DType) > 4 ? (i16)QuantizeF64(Prec, BufFloats, &BufInts) : (i16)QuantizeF32(Prec, BufFloats, &BufInts);
    PushBack(&E->EMaxes, EMax);
    ForwardZfp((i64*)BlockFloats, NDims);
    ForwardShuffle((i64*)BlockFloats, BlockUInts, NDims);
    /* zfp encode */
    i8 N = 0; // number of significant coefficients in the block so far
    i8 EndBitPlane = Min(i8(BitSizeOf(Idx2->DType) + (24 + NDims)), NBitPlanes); // TODO: why 24 (this is only based on empirical experiments with float32, for other types it might be different)?
    idx2_InclusiveForBackward(i8, Bp, NBitPlanes - 1, NBitPlanes - EndBitPlane) { // bit plane loop
      i16 RealBp = Bp + EMax;
      bool TooHighPrecision = NBitPlanes  - 6 > RealBp - Exponent(Idx2->Accuracy) + 1;
      if (TooHighPrecision) break;
      u32 ChannelKey = GetChannelKey(RealBp, E->Iter, E->Level);
      auto ChannelIt = Lookup(&E->Channels, ChannelKey);
      if (!ChannelIt) {
        channel Channel; Init(&Channel);
        Insert(&ChannelIt, ChannelKey, Channel);
      }
      idx2_Assert(ChannelIt);
      channel* C = ChannelIt.Val;
      /* write block id */
      // u32 BlockDelta = Block;
      int I = 0;
      for (; I < Size(E->BlockSigs); ++I) {
        if (E->BlockSigs[I].BitPlane == RealBp) {
          idx2_Assert(Block > E->BlockSigs[I].Block);
          // BlockDelta = Block - E->BlockSigs[I].Block - 1;
          E->BlockSigs[I].Block = Block;
          break;
        }
      }
      /* write chunk if this brick is after the last chunk */
      bool FirstSigBlock = I == Size(E->BlockSigs); // first block that becomes significant on this bit plane
      bool BrickNotEmpty = Size(C->BrickStream) > 0;
      bool NewChunk = Brick >= (C->LastChunk + 1) * Idx2->BricksPerChunks[E->Iter]; // TODO: multiplier?
      if (FirstSigBlock) {
        if (NewChunk) {
          if (BrickNotEmpty) WriteChunk(*Idx2, E, C, E->Iter, E->Level, RealBp);
          C->NBricks = 0;
          C->LastChunk = Brick >> Log2Ceil(Idx2->BricksPerChunks[E->Iter]);
        }
        PushBack(&E->BlockSigs, block_sig{Block, RealBp});
      }
      /* encode the block */
      GrowIfTooFull(&C->BlockStream);
      Encode(BlockUInts, NVals, Bp, N, &C->BlockStream);
    } // end bit plane loop
  } // end zfp block loop

  /* write the last chunk exponents if this is the first brick of the new chunk */
  bool NewChunk = Brick >= (Sc->LastChunk + 1) * Idx2->BricksPerChunks[E->Iter];
  if (NewChunk) {
    WriteChunkExponents(*Idx2, E, Sc, E->Iter, E->Level);
    Sc->LastChunk = Brick >> Log2Ceil(Idx2->BricksPerChunks[E->Iter]);
  }
  /* write the min emax */
  GrowToAccomodate(&Sc->BlockEMaxesStream, 2 * Size(E->EMaxes));
  idx2_For(int, I, 0, Size(E->EMaxes)) {
    i16 S = E->EMaxes[I] + (SizeOf(Idx2->DType) > 4 ? traits<f64>::ExpBias : traits<f32>::ExpBias);
    Write(&Sc->BlockEMaxesStream, S, SizeOf(Idx2->DType) > 4 ? 16 : traits<f32>::ExpBits);
  }
  /* write brick emax size */
  i64 BrickEMaxesSz = Size(Sc->BlockEMaxesStream);
  GrowToAccomodate(&Sc->BrickEMaxesStream, BrickEMaxesSz);
  WriteStream(&Sc->BrickEMaxesStream, &Sc->BlockEMaxesStream);
  BlockEMaxStat.Add((f64)Size(Sc->BlockEMaxesStream));
  Rewind(&Sc->BlockEMaxesStream);
  Sc->LastBrick = Brick;

  /* pass 2: encode the brick meta info */
  idx2_InclusiveFor(u32, Block, 0, LastBlock) {
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2->BlockDims3;
    v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
    bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextIter) continue;
    /* done at most once per brick */
    idx2_For(int, I, 0, Size(E->BlockSigs)) { // bit plane loop
      i16 RealBp = E->BlockSigs[I].BitPlane;
      if (Block != E->BlockSigs[I].Block) continue;
      u32 ChannelKey = GetChannelKey(RealBp, E->Iter, E->Level);
      auto ChannelIt = Lookup(&E->Channels, ChannelKey);
      idx2_Assert(ChannelIt);
      channel* C = ChannelIt.Val;
      /* write brick delta */
      if (C->NBricks == 0) {// start of a chunk
        GrowToAccomodate(&C->BrickDeltasStream, 8);
        WriteVarByte(&C->BrickDeltasStream, Brick);
      } else { // not start of a chunk
        GrowToAccomodate(&C->BrickDeltasStream, (Brick - C->LastBrick - 1 + 8) / 8);
        WriteUnary(&C->BrickDeltasStream, u32(Brick - C->LastBrick - 1));
      }
      /* write brick size */
      i64 BrickSize = Size(C->BlockStream);
      GrowToAccomodate(&C->BrickSzsStream, 4);
      WriteVarByte(&C->BrickSzsStream, BrickSize);
      /* write brick data */
      GrowToAccomodate(&C->BrickStream, BrickSize);
      WriteStream(&C->BrickStream, &C->BlockStream);
      BlockStat.Add((f64)Size(C->BlockStream));
      Rewind(&C->BlockStream);
      ++C->NBricks;
      C->LastBrick = Brick;
    } // end bit plane loop
  } // end zfp block loop
}

static inline void
EncodeBrick(idx2_file* Idx2, const params& P, encode_data* E, bool IncIter = false) {
  idx2_Assert(Idx2->NLevels <= idx2_file::MaxLevels);
  i8 Iter = E->Iter += IncIter;
  u64 Brick = E->Brick[Iter];
  printf("level %d brick " idx2_PrStrV3i " %" PRIu64 "\n", Iter, idx2_PrV3i(E->Bricks3[Iter]), Brick);
  auto BIt = Lookup(&E->BrickPool, GetBrickKey(Iter, Brick));
  idx2_Assert(BIt);
  volume& BVol = BIt.Val->Vol;
  idx2_Assert(BVol.Buffer);
  if (Dims(BIt.Val->ExtentLocal) == Idx2->BrickDims3) {
    ExtrapolateCdf53(Idx2->TdExtrpolate, &BVol);
  } else { // brick at the boundary
    v3i N3 = Dims(BIt.Val->ExtentLocal);
    transform_details Td;
    int NLevels = Log2Floor(Max(Max(N3.X, N3.Y), N3.Z));
    ComputeTransformDetails(&Td, N3, NLevels, Idx2->TformOrder);
    ExtrapolateCdf53(Td, &BVol);
  }
  ExtrapolateCdf53(Dims(BIt.Val->ExtentLocal), Idx2->TformOrder, &BVol);
  if (!P.WaveletOnly) {
    if (Iter + 1 < Idx2->NLevels)
      ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, false);
    else
      ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, true);
  } else {
    ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, false);
  }
  /* recursively encode the brick, one subband at a time */
  idx2_For(i8, Sb, 0, Size(Idx2->Subbands)) { // subband loop
    const subband& S = Idx2->Subbands[Sb];
    v3i SbDimsNonExt3 = idx2_NonExtDims(Dims(S.Grid));
    i8 NextIter = Iter + 1;
    if (Sb == 0 && NextIter < Idx2->NLevels) { // need to encode the parent brick
      /* find the parent brick and create it if not found */
      v3i Brick3 = E->Bricks3[Iter];
      v3i PBrick3 = (E->Bricks3[NextIter] = Brick3 / Idx2->GroupBrick3);
      u64 PBrick = (E->Brick[NextIter] = GetLinearBrick(*Idx2, NextIter, PBrick3));
      u64 PKey = GetBrickKey(NextIter, PBrick);
      auto PbIt = Lookup(&E->BrickPool, PKey);
      if (!PbIt) { // instantiate the parent brick in the hash table
        brick_volume PBrickVol;
        Resize(&PBrickVol.Vol, Idx2->BrickDimsExt3, dtype::float64, E->Alloc);
        Fill(idx2_Range(f64, PBrickVol.Vol), 0.0);
        v3i From3 = (Brick3 / Idx2->GroupBrick3) * Idx2->GroupBrick3;
        v3i NChildren3 = Dims(Crop(extent(From3, Idx2->GroupBrick3), extent(Idx2->NBricks3s[Iter])));
        PBrickVol.NChildrenMax = (i8)Prod(NChildren3);
        PBrickVol.ExtentLocal = extent(NChildren3 * SbDimsNonExt3);
        Insert(&PbIt, PKey, PBrickVol);
      }
      /* copy data to the parent brick and (optionally) encode it */
      v3i LocalBrickPos3 = Brick3 % Idx2->GroupBrick3;
      grid SbGridNonExt = S.Grid; SetDims(&SbGridNonExt, SbDimsNonExt3);
      extent ToGrid(LocalBrickPos3 * SbDimsNonExt3, SbDimsNonExt3);
      CopyGridExtent<f64, f64>(SbGridNonExt, BVol, ToGrid, &PbIt.Val->Vol);
//      Copy(SbGridNonExt, BVol, ToGrid, &PbIt.Val->Vol);
      bool LastChild = ++PbIt.Val->NChildren == PbIt.Val->NChildrenMax;
      if (LastChild) EncodeBrick(Idx2, P, E, true);
    } // end Sb == 0 && NextIteration < Idx2->NLevels
    E->Level = Sb;
    if      (Idx2->Version == v2i(0, 0)) EncodeSubbandV0_0(Idx2, E, S.Grid, &BVol);
    else if (Idx2->Version == v2i(0, 1)) EncodeSubbandV0_1(Idx2, E, S.Grid, &BVol);
    else if (Idx2->Version == v2i(1, 0)) EncodeSubband(Idx2, E, S.Grid, &BVol);
  } // end subband loop
  Dealloc(&BVol);
  Delete(&E->BrickPool, GetBrickKey(Iter, Brick));
  E->Iter -= IncIter;
}

// TODO: return true error code
struct channel_ptr {
  i8 Iteration = 0;
  i8 Level = 0;
  i16 BitPlane = 0;
  channel* ChunkPtr = nullptr;
  idx2_Inline bool operator<(const channel_ptr& Other) const {
    if (Iteration == Other.Iteration) {
      if (BitPlane == Other.BitPlane) return Level < Other.Level;
      return BitPlane > Other.BitPlane;
    }
    return Iteration < Other.Iteration;
  }
};

static stat CpresChunkAddrsStat;
static stat ChunkAddrsStat;
static stat ChunkSzsStat;
// TODO: check the error path
error<idx2_file_err_code>
FlushChunks(const idx2_file& Idx2, encode_data* E) {
  Reserve(&E->SortedChannels, Size(E->Channels));
  Clear(&E->SortedChannels);
  idx2_ForEach(Ch, E->Channels) {
    PushBack(&E->SortedChannels, t2<u32, channel*>{*Ch.Key, Ch.Val});
  }
  InsertionSort(Begin(E->SortedChannels), End(E->SortedChannels));
  idx2_ForEach(Ch, E->SortedChannels) {
    i8 Iter = IterationFromChannelKey(Ch->First);
    i8 Level = LevelFromChannelKey(Ch->First);
    i16 BitPlane = BitPlaneFromChannelKey(Ch->First);
    WriteChunk(Idx2, E, Ch->Second, Iter, Level, BitPlane);
  }

  /* write the chunk meta */
  idx2_ForEach(CmIt, E->ChunkMeta) {
    chunk_meta_info* Cm = CmIt.Val;
    file_id FileId = ConstructFilePath(Idx2, *CmIt.Key);
    if (FileId.Id != *CmIt.Key) {
      FileId = ConstructFilePath(Idx2, *CmIt.Key);
    }
    idx2_Assert(FileId.Id == *CmIt.Key);
    /* compress and write chunk sizes */
    idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
    Flush(&Cm->Sizes);
    WriteBuffer(Fp, ToBuffer(Cm->Sizes));
    ChunkSzsStat.Add((f64)Size(Cm->Sizes));
    WritePOD(Fp, (int)Size(Cm->Sizes));
    /* compress and write chunk addresses */
    CompressBufZstd(ToBuffer(Cm->Addrs), &E->CpresChunkAddrs);
    WriteBuffer(Fp, ToBuffer(E->CpresChunkAddrs));
    WritePOD(Fp, (int)Size(E->CpresChunkAddrs));
    WritePOD(Fp, (int)Size(Cm->Addrs)); // number of chunks
    ChunkAddrsStat.Add((f64)Size(Cm->Addrs) * sizeof(Cm->Addrs[0]));
    CpresChunkAddrsStat.Add((f64)Size(E->CpresChunkAddrs));
  }
  return idx2_Error(idx2_file_err_code::NoError);
}

f64 TotalTime_ = 0;

// TODO: handle types different than float64
error<idx2_file_err_code>
Encode(idx2_file* Idx2, const params& P, const volume& Vol) {
  const int BrickBytes = Prod(Idx2->BrickDimsExt3) * sizeof(f64);
  BrickAlloc_ = free_list_allocator(BrickBytes);
  idx2_RAII(encode_data, E, Init(&E));
  idx2_BrickTraverse(
    timer Timer; StartTimer(&Timer);
//    idx2_Assert(GetLinearBrick(*Idx2, 0, Top.BrickFrom3) == Top.Address);
//    idx2_Assert(GetSpatialBrick(*Idx2, 0, Top.Address) == Top.BrickFrom3);
    brick_volume BVol;
    Resize(&BVol.Vol, Idx2->BrickDimsExt3, dtype::float64, E.Alloc);
    Fill(idx2_Range(f64, BVol.Vol), 0.0);
    extent BrickExtent(Top.BrickFrom3 * Idx2->BrickDims3, Idx2->BrickDims3);
    extent BrickExtentCrop = Crop(BrickExtent, extent(Idx2->Dims3));
    BVol.ExtentLocal = Relative(BrickExtentCrop, BrickExtent);
    v2d MinMax;
    if (Vol.Type == dtype::float32)
      MinMax = (CopyExtentExtentMinMax<f32, f64>(BrickExtentCrop, Vol, BVol.ExtentLocal, &BVol.Vol));
    else if (Vol.Type == dtype::float64)
      MinMax = (CopyExtentExtentMinMax<f64, f64>(BrickExtentCrop, Vol, BVol.ExtentLocal, &BVol.Vol));
    Idx2->ValueRange.Min = Min(Idx2->ValueRange.Min, MinMax.Min);
    Idx2->ValueRange.Max = Max(Idx2->ValueRange.Max, MinMax.Max);
//    Copy(BrickExtentCrop, Vol, BVol.ExtentLocal, &BVol.Vol);
    E.Iter = 0;
    E.Bricks3[E.Iter] = Top.BrickFrom3;
    E.Brick[E.Iter] = GetLinearBrick(*Idx2, E.Iter, E.Bricks3[E.Iter]);
    idx2_Assert(E.Brick[E.Iter] == Top.Address);
    u64 BrickKey = GetBrickKey(E.Iter, E.Brick[E.Iter]);
    Insert(&E.BrickPool, BrickKey, BVol);
    EncodeBrick(Idx2, P, &E);
    TotalTime_ += Seconds(ElapsedTime(&Timer));
    , 128
    , Idx2->BrickOrders[E.Iter]
    , v3i(0)
    , Idx2->NBricks3s[E.Iter]
    , extent(Idx2->NBricks3s[E.Iter])
    , extent(Idx2->NBricks3s[E.Iter])
  );

  /* dump the bit streams to files */
  timer Timer; StartTimer(&Timer);
  idx2_PropagateIfError(FlushChunks(*Idx2, &E));
  idx2_PropagateIfError(FlushChunkExponents(*Idx2, &E));
  timer RdoTimer; StartTimer(&RdoTimer);
  RateDistortionOpt(*Idx2, &E);
  TotalTime_ += Seconds(ElapsedTime(&Timer));
  printf("rdo time                = %f\n", Seconds(ElapsedTime(&RdoTimer)));

  WriteMetaFile(*Idx2, P, idx2_PrintScratch("%s/%s/%s.idx", P.OutDir, P.Meta.Name, P.Meta.Field));
  printf("num channels            = %" PRIi64 "\n", Size(E.Channels));
  printf("num sub channels        = %" PRIi64 "\n", Size(E.SubChannels));
  printf("num chunks              = %" PRIi64 "\n", ChunkStreamStat.Count());
  printf("brick deltas      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickDeltasStat.Sum(), BrickDeltasStat.Avg(), BrickDeltasStat.StdDev());
  printf("brick sizes       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickSzsStat.Sum(), BrickSzsStat.Avg(), BrickSzsStat.StdDev());
  printf("brick stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickStreamStat.Sum(), BrickStreamStat.Avg(), BrickStreamStat.StdDev());
  printf("block stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BlockStat.Sum(), BlockStat.Avg(), BlockStat.StdDev());
  printf("chunk sizes       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkSzsStat.Sum(), ChunkSzsStat.Avg(), ChunkSzsStat.StdDev());
  printf("chunk addrs       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkAddrsStat.Sum(), ChunkAddrsStat.Avg(), ChunkAddrsStat.StdDev());
  printf("cpres chunk addrs total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", CpresChunkAddrsStat.Sum(), CpresChunkAddrsStat.Avg(), CpresChunkAddrsStat.StdDev());
  printf("chunk stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkStreamStat.Sum(), ChunkStreamStat.Avg(), ChunkStreamStat.StdDev());
  printf("brick exps        total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickEMaxesStat.Sum(), BrickEMaxesStat.Avg(), BrickEMaxesStat.StdDev());
  printf("block exps        total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BlockEMaxStat.Sum(), BlockEMaxStat.Avg(), BlockEMaxStat.StdDev());
  printf("chunk exp sizes   total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkEMaxSzsStat.Sum(), ChunkEMaxSzsStat.Avg(), ChunkEMaxSzsStat.StdDev());
  printf("chunk exps stream total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkEMaxesStat.Sum(), ChunkEMaxesStat.Avg(), ChunkEMaxesStat.StdDev());
  printf("total time              = %f seconds\n", TotalTime_);
//  _ASSERTE( _CrtCheckMemory( ) );
  return idx2_Error(idx2_file_err_code::NoError);
}

/* ---------------------------------------------------------------------------------------------- */
/*                                     DECODER                                                    */
/* ---------------------------------------------------------------------------------------------- */

static idx2_Inline bool
IsEmpty(const chunk_exp_cache& ChunkExpCache) {
  return Size(ChunkExpCache.BrickExpsStream.Stream) == 0;
}
static void
Dealloc(chunk_exp_cache* ChunkExpCache) {
  Dealloc(&ChunkExpCache->BrickExpsStream);
}
static void
Dealloc(chunk_rdo_cache* ChunkRdoCache) {
  Dealloc(&ChunkRdoCache->TruncationPoints);
}

static void
Dealloc(chunk_cache* ChunkCache) {
  Dealloc(&ChunkCache->Bricks);
  Dealloc(&ChunkCache->BrickSzs);
  Dealloc(&ChunkCache->ChunkStream);
}

static void
Dealloc(file_exp_cache* FileExpCache) {
  idx2_ForEach(ChunkExpCacheIt, FileExpCache->ChunkExpCaches) Dealloc(ChunkExpCacheIt);
  Dealloc(&FileExpCache->ChunkExpCaches);
  Dealloc(&FileExpCache->ChunkExpSzs);
}
static void
Dealloc(file_rdo_cache* FileRdoCache) {
  idx2_ForEach(TileRdoCacheIt, FileRdoCache->TileRdoCaches) { Dealloc(TileRdoCacheIt); }
}

static void
Dealloc(file_cache* FileCache) {
  Dealloc(&FileCache->ChunkSizes);
  idx2_ForEach(ChunkCacheIt, FileCache->ChunkCaches) Dealloc(ChunkCacheIt.Val);
  Dealloc(&FileCache->ChunkCaches);
}

static void
Init(file_cache_table* FileCacheTable) {
  Init(&FileCacheTable->FileCaches, 8);
  Init(&FileCacheTable->FileExpCaches, 5);
  Init(&FileCacheTable->FileRdoCaches, 5);
}
static void
Dealloc(file_cache_table* FileCacheTable) {
  idx2_ForEach(FileCacheIt, FileCacheTable->FileCaches) Dealloc(FileCacheIt.Val);
  Dealloc(&FileCacheTable->FileCaches);
  idx2_ForEach(FileExpCacheIt, FileCacheTable->FileExpCaches) Dealloc(FileExpCacheIt.Val);
  idx2_ForEach(FileRdoCacheIt, FileCacheTable->FileRdoCaches) Dealloc(FileRdoCacheIt.Val);
}

static void
Init(decode_data* D, allocator* Alloc = nullptr) {
  Init(&D->BrickPool, 5);
  D->Alloc = Alloc ? Alloc : &BrickAlloc_;
  Init(&D->FcTable);
  Init(&D->Streams, 7);
//  Reserve(&D->RequestedChunks, 64);
}

static void
Dealloc(decode_data* D) {
  D->Alloc->DeallocAll();
  idx2_ForEach(BrickVolIt, D->BrickPool) Dealloc(&BrickVolIt.Val->Vol);
  Dealloc(&D->BrickPool);
  Dealloc(&D->FcTable);
  Dealloc(&D->BlockStream);
  Dealloc(&D->Streams);
  DeallocBuf(&D->CompressedChunkExps);
  Dealloc(&D->ChunkEMaxSzsStream);
  Dealloc(&D->ChunkAddrsStream);
  Dealloc(&D->ChunkSzsStream);
//  Dealloc(&D->RequestedChunks);
}

u64 DecodeIOTime_ = 0;
u64 DataMovementTime_ = 0;
u64 BytesRdos_ = 0;
u64 BytesExps_ = 0;
u64 BytesData_ = 0;

static error<idx2_file_err_code>
ReadFileRdos(const idx2_file& Idx2, hash_table<u64, file_rdo_cache>::iterator* FileRdoCacheIt, const file_id& FileId) {
  timer IOTimer;
  StartTimer(&IOTimer);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"),, if (Fp) fclose(Fp));
  idx2_FSeek(Fp, 0, SEEK_END);
  int NumChunks = 0;
  i64 Sz = idx2_FTell(Fp) - sizeof(NumChunks);
  ReadBackwardPOD(Fp, &NumChunks);
  BytesRdos_ += sizeof(NumChunks);
  file_rdo_cache FileRdoCache;
  Resize(&FileRdoCache.TileRdoCaches, NumChunks);
  idx2_RAII(buffer, CompresBuf, AllocBuf(&CompresBuf, Sz), DeallocBuf(&CompresBuf));
  ReadBackwardBuffer(Fp, &CompresBuf);
  DecodeIOTime_ += ElapsedTime(&IOTimer);
  BytesRdos_ += Size(CompresBuf);
  idx2_RAII(bitstream, Bs,);
  DecompressBufZstd(CompresBuf, &Bs);
  int Pos = 0;
  idx2_For(int, I, 0, Size(FileRdoCache.TileRdoCaches)) {
    chunk_rdo_cache& TileRdoCache = FileRdoCache.TileRdoCaches[I];
    Resize(&TileRdoCache.TruncationPoints, Size(Idx2.RdoLevels) * Size(Idx2.Subbands));
    idx2_ForEach(It, TileRdoCache.TruncationPoints) *It = ((const i16*)Bs.Stream.Data)[Pos++];
  }
  Insert(FileRdoCacheIt, FileId.Id, FileRdoCache);
  return idx2_Error(idx2_file_err_code::NoError);
}

static error<idx2_file_err_code>
ReadFileExponents(decode_data* D, hash_table<u64, file_exp_cache>::iterator* FileExpCacheIt, const file_id& FileId) {
  timer IOTimer;
  StartTimer(&IOTimer);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"),, if (Fp) fclose(Fp));
  idx2_FSeek(Fp, 0, SEEK_END);
  int ChunkEMaxSzsSz = 0; ReadBackwardPOD(Fp, &ChunkEMaxSzsSz);
  Rewind(&D->ChunkEMaxSzsStream);
  GrowToAccomodate(&D->ChunkEMaxSzsStream, ChunkEMaxSzsSz - Size(D->ChunkEMaxSzsStream));
  ReadBackwardBuffer(Fp, &D->ChunkEMaxSzsStream.Stream, ChunkEMaxSzsSz);
  BytesExps_ += sizeof(int) + ChunkEMaxSzsSz;
  DecodeIOTime_ += ElapsedTime(&IOTimer);
  InitRead(&D->ChunkEMaxSzsStream, D->ChunkEMaxSzsStream.Stream);
  file_exp_cache FileExpCache;
  Reserve(&FileExpCache.ChunkExpSzs, ChunkEMaxSzsSz);
  i32 ChunkEMaxSz = 0;
  while (Size(D->ChunkEMaxSzsStream) < ChunkEMaxSzsSz) {
    PushBack(&FileExpCache.ChunkExpSzs, ChunkEMaxSz += (i32)ReadVarByte(&D->ChunkEMaxSzsStream));
  }
  Resize(&FileExpCache.ChunkExpCaches, Size(FileExpCache.ChunkExpSzs));
  idx2_Assert(Size(D->ChunkEMaxSzsStream) == ChunkEMaxSzsSz);
  Insert(FileExpCacheIt, FileId.Id, FileExpCache);
  return idx2_Error(idx2_file_err_code::NoError);
}

/* Given a brick address, open the file associated with the brick and cache its chunk information */
static error<idx2_file_err_code>
ReadFile(decode_data* D, hash_table<u64, file_cache>::iterator* FileCacheIt, const file_id& FileId) {
  timer IOTimer;
  StartTimer(&IOTimer);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"),, if (Fp) fclose(Fp));
  idx2_FSeek(Fp, 0, SEEK_END);
  int NChunks = 0; ReadBackwardPOD(Fp, &NChunks);
  // TODO: check if there are too many NChunks

  /* read and decompress chunk addresses */
  int IniChunkAddrsSz = NChunks * (int)sizeof(u64);
  int ChunkAddrsSz; ReadBackwardPOD(Fp, &ChunkAddrsSz);
  idx2_RAII(buffer, CpresChunkAddrs, AllocBuf(&CpresChunkAddrs, ChunkAddrsSz), DeallocBuf(&CpresChunkAddrs)); // TODO: move to decode_data
  ReadBackwardBuffer(Fp, &CpresChunkAddrs, ChunkAddrsSz);
  BytesData_ += ChunkAddrsSz;
  DecodeIOTime_ += ElapsedTime(&IOTimer);
  Rewind(&D->ChunkAddrsStream);
  GrowToAccomodate(&D->ChunkAddrsStream, IniChunkAddrsSz - Size(D->ChunkAddrsStream));
  DecompressBufZstd(CpresChunkAddrs, &D->ChunkAddrsStream);

  /* read chunk sizes */
  ResetTimer(&IOTimer);
  int ChunkSizesSz = 0;
  ReadBackwardPOD(Fp, &ChunkSizesSz);
  Rewind(&D->ChunkSzsStream);
  GrowToAccomodate(&D->ChunkSzsStream, ChunkSizesSz - Size(D->ChunkSzsStream));
  ReadBackwardBuffer(Fp, &D->ChunkSzsStream.Stream, ChunkSizesSz);
  BytesData_ += ChunkSizesSz;
  DecodeIOTime_ += ElapsedTime(&IOTimer);
  InitRead(&D->ChunkSzsStream, D->ChunkSzsStream.Stream);

  /* parse the chunk addresses and cache in memory */
  file_cache FileCache;
  i64 AccumSize = 0;
  Init(&FileCache.ChunkCaches, 10);
  idx2_For(int, I, 0, NChunks) {
    i64 ChunkSize = ReadVarByte(&D->ChunkSzsStream);
    u64 ChunkAddr = *((u64*)D->ChunkAddrsStream.Stream.Data + I);
    chunk_cache ChunkCache; ChunkCache.ChunkPos = I;
    Insert(&FileCache.ChunkCaches, ChunkAddr, ChunkCache);
    PushBack(&FileCache.ChunkSizes, AccumSize += ChunkSize);
  }
  idx2_Assert(Size(D->ChunkSzsStream) == ChunkSizesSz);
  Insert(FileCacheIt, FileId.Id, FileCache);
  return idx2_Error(idx2_file_err_code::NoError);
}

static void
DecompressChunk(bitstream* ChunkStream, chunk_cache* ChunkCache, u64 ChunkAddress, int L) {
  (void)L; u64 Brk = ((ChunkAddress >> 18) & 0x3FFFFFFFFFFull); (void)Brk;
  InitRead(ChunkStream, ChunkStream->Stream);
  int NBricks = (int)ReadVarByte(ChunkStream); idx2_Assert(NBricks > 0);

  /* decompress and store the brick ids */
  u64 Brick = ReadVarByte(ChunkStream);
  Resize(&ChunkCache->Bricks, NBricks);
  ChunkCache->Bricks[0] = Brick;
  idx2_For(int, I, 1, NBricks) {
    Brick += ReadUnary(ChunkStream) + 1;
    ChunkCache->Bricks[I] = Brick;
    idx2_Assert(Brk == (Brick >> L));
  }
  Resize(&ChunkCache->BrickSzs, NBricks);

  /* decompress and store the brick sizes */
  i32 BrickSize = 0;
  SeekToNextByte(ChunkStream);
  idx2_ForEach(BrickSzIt, ChunkCache->BrickSzs) *BrickSzIt = BrickSize += (i32)ReadVarByte(ChunkStream);
  ChunkCache->ChunkStream = *ChunkStream;
}

/* Given a brick address, read the exponent chunk associated with the brick and cache it */
// TODO: remove the last two params (already stored in D)
static expected<const chunk_exp_cache*, idx2_file_err_code>
ReadChunkExponents(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter, i8 Level) {
  file_id FileId = ConstructFilePathExponents(Idx2, Brick, Iter, Level);
  auto FileExpCacheIt = Lookup(&D->FcTable.FileExpCaches, FileId.Id);
  if (!FileExpCacheIt) {
    auto ReadFileOk = ReadFileExponents(D, &FileExpCacheIt, FileId);
    if (!ReadFileOk) idx2_PropagateError(ReadFileOk);
  }
  if (!FileExpCacheIt) return idx2_Error(idx2_file_err_code::FileNotFound);
  file_exp_cache* FileExpCache = FileExpCacheIt.Val;
  idx2_Assert(D->ChunkInFile < Size(FileExpCache->ChunkExpSzs));

  /* find the appropriate chunk */
  if (IsEmpty(FileExpCache->ChunkExpCaches[D->ChunkInFile])) {
    timer IOTimer;
    StartTimer(&IOTimer);
    idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
    idx2_Assert(Fp); // TODO: return an error
    i32 ChunkExpOffset = D->ChunkInFile == 0 ? 0 : FileExpCache->ChunkExpSzs[D->ChunkInFile - 1];
    i32 ChunkExpSize = FileExpCache->ChunkExpSzs[D->ChunkInFile] - ChunkExpOffset;
    idx2_FSeek(Fp, ChunkExpOffset, SEEK_SET);
    chunk_exp_cache ChunkExpCache;
    bitstream& ChunkExpStream = ChunkExpCache.BrickExpsStream;
    // TODO: calculate the number of bricks in this chunk in a different way to verify correctness
    Resize(&D->CompressedChunkExps, ChunkExpSize);
    ReadBuffer(Fp, &D->CompressedChunkExps, ChunkExpSize);
    DecompressBufZstd(buffer{D->CompressedChunkExps.Data, ChunkExpSize}, &ChunkExpStream);
    BytesExps_ += ChunkExpSize;
    DecodeIOTime_ += ElapsedTime(&IOTimer);
    InitRead(&ChunkExpStream, ChunkExpStream.Stream);
    FileExpCache->ChunkExpCaches[D->ChunkInFile] = ChunkExpCache;
  }
  return &FileExpCache->ChunkExpCaches[D->ChunkInFile];
}

static expected<const chunk_rdo_cache*, idx2_file_err_code>
ReadChunkRdos(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter) {
  file_id FileId = ConstructFilePathRdos(Idx2, Brick, Iter);
  auto FileRdoCacheIt = Lookup(&D->FcTable.FileRdoCaches, FileId.Id);
  if (!FileRdoCacheIt) {
    auto ReadFileOk = ReadFileRdos(Idx2, &FileRdoCacheIt, FileId);
    if (!ReadFileOk) idx2_PropagateError(ReadFileOk);
  }
  if (!FileRdoCacheIt) return idx2_Error(idx2_file_err_code::FileNotFound);
  file_rdo_cache* FileRdoCache = FileRdoCacheIt.Val;
  return &FileRdoCache->TileRdoCaches[D->ChunkInFile];
}

/* Given a brick address, read the chunk associated with the brick and cache the chunk */
static expected<const chunk_cache*, idx2_file_err_code>
ReadChunk(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  file_id FileId = ConstructFilePath(Idx2, Brick, Iter, Level, BitPlane);
  auto FileCacheIt = Lookup(&D->FcTable.FileCaches, FileId.Id);
  if (!FileCacheIt) {
    auto ReadFileOk = ReadFile(D, &FileCacheIt, FileId);
    if (!ReadFileOk) idx2_PropagateError(ReadFileOk);
  }
  if (!FileCacheIt) return idx2_Error(idx2_file_err_code::FileNotFound);

  /* find the appropriate chunk */
  u64 ChunkAddress = GetChunkAddress(Idx2, Brick, Iter, Level, BitPlane);
  file_cache* FileCache = FileCacheIt.Val;
  decltype(FileCache->ChunkCaches)::iterator ChunkCacheIt;
  ChunkCacheIt = Lookup(&FileCache->ChunkCaches, ChunkAddress);
  if (!ChunkCacheIt) return idx2_Error(idx2_file_err_code::ChunkNotFound);
  chunk_cache* ChunkCache = ChunkCacheIt.Val;
  if (Size(ChunkCache->ChunkStream.Stream) == 0) {
    timer IOTimer;
    StartTimer(&IOTimer);
    idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
    i32 ChunkPos = ChunkCache->ChunkPos;
    i64 ChunkOffset = ChunkPos > 0 ? FileCache->ChunkSizes[ChunkPos - 1] : 0;
    i64 ChunkSize = FileCache->ChunkSizes[ChunkPos] - ChunkOffset;
    idx2_FSeek(Fp, ChunkOffset, SEEK_SET);
    bitstream ChunkStream; InitWrite(&ChunkStream, ChunkSize); // NOTE: not a memory leak since we will keep track of this in ChunkCache
    ReadBuffer(Fp, &ChunkStream.Stream);
    BytesData_ += Size(ChunkStream.Stream);
    DecodeIOTime_ += ElapsedTime(&IOTimer);
    DecompressChunk(&ChunkStream, ChunkCache, ChunkAddress, Log2Ceil(Idx2.BricksPerChunks[Iter])); // TODO: check for error
//    PushBack(&D->RequestedChunks, t2<u64, u64>{ChunkAddress, FileId.Id});
  }
  return ChunkCacheIt.Val;
}

/* decode the subband of a brick */
// TODO: we can detect the precision and switch to the avx2 version that uses float for better
// performance
// TODO: if a block does not decode any bit plane, no need to copy data afterwards
static error<idx2_file_err_code>
DecodeSubband(const idx2_file& Idx2, decode_data* D, f64 Accuracy, const grid& SbGrid, volume* BVol) {
  u64 Brick = D->Brick[D->Iter];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Idx2.BlockDims3 - 1) / Idx2.BlockDims3;
  /* read the rdo information if present */
  int MinBitPlane = traits<i16>::Min;
  if (Size(Idx2.RdoLevels) > 0 && D->QualityLevel >= 0) {
//    printf("reading rdo\n");
    auto ReadChunkRdoResult = ReadChunkRdos(Idx2, D, Brick, D->Iter);
    if (!ReadChunkRdoResult) return Error(ReadChunkRdoResult);
    const chunk_rdo_cache* ChunkRdoCache = Value(ReadChunkRdoResult);
    int Ql = Min(D->QualityLevel, (int)Size(Idx2.RdoLevels) - 1);
    MinBitPlane = ChunkRdoCache->TruncationPoints[D->Level * Size(Idx2.RdoLevels) + Ql];
  }
  if (MinBitPlane == traits<i16>::Max) return idx2_Error(idx2_file_err_code::NoError);
  int BlockCount = Prod(NBlocks3);
  if (D->Level == 0 && D->Iter + 1 < Idx2.NLevels) {
    BlockCount -= Prod(SbDims3 / Idx2.BlockDims3);
  }
  /* first, read the block exponents */
  auto ReadChunkExpResult = ReadChunkExponents(Idx2, D, Brick, D->Iter, D->Level);
  if (!ReadChunkExpResult) return Error(ReadChunkExpResult);
  const chunk_exp_cache* ChunkExpCache = Value(ReadChunkExpResult);
  i32 BrickExpOffset = (D->BrickInChunk * BlockCount) * (SizeOf(Idx2.DType) > 4 ? 2 : 1);
  bitstream BrickExpsStream = ChunkExpCache->BrickExpsStream;
  SeekToByte(&BrickExpsStream, BrickExpOffset);
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  const i8 NBitPlanes = idx2_BitSizeOf(u64);
  /* gather the streams (for the different bit planes) */
  auto& Streams = D->Streams;
  Clear(&Streams);
  idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2.BlockDims3;
    v3i BlockDims3 = Min(Idx2.BlockDims3, SbDims3 - D3);
    const int NDims = NumDims(BlockDims3);
    const int NVals = 1 << (2 * NDims);
    const int Prec = NBitPlanes - 1 - NDims;
    f64 BlockFloats[4 * 4 * 4];
    buffer_t BufFloats(BlockFloats, NVals);
    buffer_t BufInts((i64*)BlockFloats, NVals);
    u64 BlockUInts [4 * 4 * 4] = {}; buffer_t BufUInts (BlockUInts , Prod(BlockDims3));
    bool CodedInNextIter = D->Level == 0 && D->Iter + 1 < Idx2.NLevels && BlockDims3 == Idx2.BlockDims3;
    if (CodedInNextIter) continue;
    i16 EMax = SizeOf(Idx2.DType) > 4 ? (i16)Read(&BrickExpsStream, 16) - traits<f64>::ExpBias
                                    : (i16)Read(&BrickExpsStream, traits<f32>::ExpBits) - traits<f32>::ExpBias;
    i8 N = 0;
    i8 EndBitPlane = Min(i8(BitSizeOf(Idx2.DType) + (24 + NDims)), NBitPlanes);
    int NBitPlanesDecoded = Exponent(Accuracy) - 6 - EMax + 1;
    i8 NBps = 0;
    idx2_InclusiveForBackward(i8, Bp, NBitPlanes - 1, NBitPlanes - EndBitPlane) { // bit plane loop
      i16 RealBp = Bp + EMax;
      if (NBitPlanes - 6 > RealBp - Exponent(Accuracy) + 1) break;
      if (RealBp < MinBitPlane) break;
      auto StreamIt = Lookup(&Streams, RealBp);
      bitstream* Stream = nullptr;
      if (!StreamIt) { // first block in the brick
        auto ReadChunkResult = ReadChunk(Idx2, D, Brick, D->Iter, D->Level, RealBp);
        if (!ReadChunkResult) { idx2_Assert(false); return Error(ReadChunkResult); }
        const chunk_cache* ChunkCache = Value(ReadChunkResult);
        auto BrickIt = BinarySearch(idx2_Range(ChunkCache->Bricks), Brick);
        idx2_Assert(BrickIt != End(ChunkCache->Bricks));
        idx2_Assert(*BrickIt == Brick);
        i64 BrickInChunk = BrickIt - Begin(ChunkCache->Bricks);
        idx2_Assert(BrickInChunk < Size(ChunkCache->BrickSzs));
        i64 BrickOffset = BrickInChunk == 0 ? 0 : ChunkCache->BrickSzs[BrickInChunk - 1];
        BrickOffset += Size(ChunkCache->ChunkStream);
        Insert(&StreamIt, RealBp, ChunkCache->ChunkStream);
        Stream = StreamIt.Val;
        SeekToByte(Stream, BrickOffset);
      } else {
        Stream = StreamIt.Val;
      }
      /* zfp decode */
      ++NBps;
//      timer Timer; StartTimer(&Timer);
      if (NBitPlanesDecoded <= 8)
        Decode(BlockUInts, NVals, Bp, N, Stream);
      else
        DecodeTest(&BlockUInts[NBitPlanes - 1 - Bp], NVals, N, Stream);
//      DecodeTime_ += Seconds(ElapsedTime(&Timer));
    } // end bit plane loop
    if (NBitPlanesDecoded > 8) {
//      timer Timer; StartTimer(&Timer);
      TransposeRecursive(BlockUInts, NBps);
//      DecodeTime_ += Seconds(ElapsedTime(&Timer));
    }
    if (NBps > 0) {
      InverseShuffle(BlockUInts, (i64*)BlockFloats, NDims);
      InverseZfp((i64*)BlockFloats, NDims);
      Dequantize(EMax, Prec, BufInts, &BufFloats);
      v3i S3;
      int J = 0;
      v3i From3 = From(SbGrid), Strd3 = Strd(SbGrid);
      timer DataTimer;
      StartTimer(&DataTimer);
      idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
        idx2_Assert(D3 + S3 < SbDims3);
        BVol->At<f64>(From3, Strd3, D3 + S3) = BlockFloats[J++];
      } idx2_EndFor3 // end sample loop
      DataMovementTime_ += ElapsedTime(&DataTimer);
    }
  }
  return idx2_Error(idx2_file_err_code::NoError);
}

static void
DecodeBrick(const idx2_file& Idx2, const params& P, decode_data* D, u8 Mask, f64 Accuracy) {
  i8 Iter = D->Iter;
  u64 Brick = D->Brick[Iter];
//  if ((Brick >> Idx2.BricksPerChunks[Iter]) != D->LastTile) {
//    idx2_ForEach(It, D->RequestedChunks) {
//      auto& FileCache = D->FcTable.FileCaches[It->Second];
//      auto ChunkCacheIt = Lookup(&FileCache.ChunkCaches, It->First);
//      Dealloc(ChunkCacheIt.Val);
//      Delete(&FileCache.ChunkCaches, It->First); // TODO: write a delete that works on iterator
//    }
//    Clear(&D->RequestedChunks);
//    D->LastTile = Brick >> Idx2.BricksPerChunks[Iter];
//  }
//  printf("level %d brick " idx2_PrStrV3i " %llu\n", Iter, idx2_PrV3i(D->Bricks3[Iter]), Brick);
  auto BrickIt = Lookup(&D->BrickPool, GetBrickKey(Iter, Brick));
  idx2_Assert(BrickIt);
  volume& BVol = BrickIt.Val->Vol;

  /* construct a list of subbands to decode */
  // TODO: test this logic
  idx2_Assert(Size(Idx2.Subbands) <= 8);
  u8 DecodeSbMask = Mask; // TODO: need change if we support more than one transform pass per brick
  idx2_For(u8, Sb, 0, 8) {
    if (!BitSet(Mask, Sb)) continue;
    idx2_For(u8, S, 0, 8) if ((Sb | S) <= Sb) DecodeSbMask = SetBit(DecodeSbMask, S);
  } // end subband loop

  /* recursively decode the brick, one subband at a time */
  idx2_Assert(Size(Idx2.Subbands) == 8);
  idx2_For(i8, Sb, 0, (i8)Size(Idx2.Subbands)) {
    if (!BitSet(DecodeSbMask, Sb)) continue;
    const subband& S = Idx2.Subbands[Sb];
    v3i SbDimsNonExt3 = idx2_NonExtDims(Dims(S.Grid));
    i8 NextIter = Iter + 1;
    if (Sb == 0 && NextIter < Idx2.NLevels) { // need to decode the parent brick first
      /* find and decode the parent */
      v3i Brick3 = D->Bricks3[D->Iter];
      v3i PBrick3 = (D->Bricks3[NextIter] = Brick3 / Idx2.GroupBrick3);
      u64 PBrick = (D->Brick[NextIter] = GetLinearBrick(Idx2, NextIter, PBrick3));
      u64 PKey = GetBrickKey(NextIter, PBrick);
      auto PbIt = Lookup(&D->BrickPool, PKey);
      idx2_Assert(PbIt);
      // TODO: problem: here we will need access to D->LinearChunkInFile/D->LinearBrickInChunk for
      // the parent, which won't be computed correctly by the outside code, so for now we have to
      // stick to decoding from higher level down
      /* copy data from the parent's to my buffer */
      if (PbIt.Val->NChildren == 0) {
        v3i From3 = (Brick3 / Idx2.GroupBrick3) * Idx2.GroupBrick3;
        v3i NChildren3 = Dims(Crop(extent(From3, Idx2.GroupBrick3), extent(Idx2.NBricks3s[Iter])));
        PbIt.Val->NChildrenMax = (i8)Prod(NChildren3);
      }
      ++PbIt.Val->NChildren;
      v3i LocalBrickPos3 = Brick3 % Idx2.GroupBrick3;
      grid SbGridNonExt = S.Grid; SetDims(&SbGridNonExt, SbDimsNonExt3);
      extent ToGrid(LocalBrickPos3 * SbDimsNonExt3, SbDimsNonExt3);
      CopyExtentGrid<f64, f64>(ToGrid, PbIt.Val->Vol, SbGridNonExt, &BVol);
      if (PbIt.Val->NChildren == PbIt.Val->NChildrenMax) { // last child
        Dealloc(&PbIt.Val->Vol);
        Delete(&D->BrickPool, PKey);
      }
    }
    D->Level = Sb;
    if (Sb == 0 || Iter >= D->EffIter) { // NOTE: the check for Sb == 0 prevents the output volume from having blocking artifacts
      if      (Idx2.Version == v2i(0, 0)) DecodeSubbandV0_0(Idx2, D, S.Grid, &BVol);
      else if (Idx2.Version == v2i(0, 1)) DecodeSubbandV0_1(Idx2, D, S.Grid, &BVol);
      else if (Idx2.Version == v2i(1, 0)) DecodeSubband(Idx2, D, Accuracy, S.Grid, &BVol);
    }
  } // end subband loop
  // TODO: inverse transform only to the necessary level
  if (!P.WaveletOnly) {
//    printf("inverting\n");
    if (Iter + 1 < Idx2.NLevels)
      InverseCdf53(Idx2.BrickDimsExt3, D->Iter, Idx2.Subbands, Idx2.Td, &BVol, false);
    else
      InverseCdf53(Idx2.BrickDimsExt3, D->Iter, Idx2.Subbands, Idx2.Td, &BVol, true);
  }
}

/* TODO: dealloc chunks after we are done with them */
void
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf) {
  timer DecodeTimer; StartTimer(&DecodeTimer);
  // TODO: we should add a --effective-mask
  u8 OutMask = P.DecodeLevel == P.OutputLevel ? P.DecodeMask : 128;
  grid OutGrid = GetGrid(P.DecodeExtent, P.OutputLevel, OutMask, Idx2.Subbands);
  printf("output grid = " idx2_PrStrGrid "\n", idx2_PrGrid(OutGrid));
  mmap_volume OutVol;
  volume OutVolMem;
  idx2_CleanUp(if (P.OutMode == params::out_mode::WriteToFile) { Unmap(&OutVol); });
  if (P.OutMode == params::out_mode::WriteToFile) {
    metadata Met;
    memcpy(Met.Name, Idx2.Name, sizeof(Met.Name));
    memcpy(Met.Field, Idx2.Field, sizeof(Met.Field));
    Met.Dims3 = Dims(OutGrid);
    Met.DType = Idx2.DType;
  //  printf("zfp decode time = %f\n", DecodeTime_);
    cstr OutFile = P.OutFile ? idx2_PrintScratch("%s/%s", P.OutDir, P.OutFile)
                             : idx2_PrintScratch("%s/%s", P.OutDir, ToRawFileName(Met));
//    idx2_RAII(mmap_volume, OutVol, (void)OutVol, Unmap(&OutVol));
    MapVolume(OutFile, Met.Dims3, Met.DType, &OutVol, map_mode::Write);
    printf("writing output volume to %s\n", OutFile);
  } else if (P.OutMode == params::out_mode::KeepInMemory) {
    OutVolMem.Buffer = *OutBuf;
    SetDims(&OutVolMem, Dims(OutGrid));
    OutVolMem.Type = Idx2.DType;
  }
  const int BrickBytes = Prod(Idx2.BrickDimsExt3) * sizeof(f64);
  BrickAlloc_ = free_list_allocator(BrickBytes);
  // TODO: move the decode_data into idx2_file itself
  idx2_RAII(decode_data, D, Init(&D, &BrickAlloc_));
//  D.QualityLevel = Dw->GetQuality();
  D.EffIter = P.DecodeLevel; // effective level (levels smaller than this won't be decoded)
  f64 Accuracy = Max(Idx2.Accuracy, P.DecodeAccuracy);
//  i64 CountZeroes = 0;
  idx2_InclusiveForBackward(i8, Iter, Idx2.NLevels - 1, 0) {
    if (Iter < P.OutputLevel) break;
    extent Ext = P.DecodeExtent; // this is in unit of samples
    v3i BrickDims3 = Idx2.BrickDims3 * Pow(Idx2.GroupBrick3, Iter);
    v3i BrickFirst3 = From(Ext) / BrickDims3;
    v3i BrickLast3 = Last(Ext) / BrickDims3;
    extent ExtentInBricks(BrickFirst3, BrickLast3 - BrickFirst3 + 1);
    v3i ChunkDims3 = Idx2.BricksPerChunk3s[Iter] * BrickDims3;
    v3i ChunkFirst3 = From(Ext) / ChunkDims3;
    v3i ChunkLast3 = Last(Ext) /  ChunkDims3;
    extent ExtentInChunks(ChunkFirst3, ChunkLast3 - ChunkFirst3 + 1);
    v3i FileDims3 = ChunkDims3 * Idx2.ChunksPerFile3s[Iter];
    v3i FileFirst3 = From(Ext) / FileDims3;
    v3i FileLast3 = Last(Ext) / FileDims3;
    extent ExtentInFiles(FileFirst3, FileLast3 - FileFirst3 + 1);
    extent VolExt(Idx2.Dims3);
    v3i VolBrickFirst3 = From(VolExt) / BrickDims3;
    v3i VolBrickLast3 = Last(VolExt)  / BrickDims3;
    extent VolExtentInBricks(VolBrickFirst3, VolBrickLast3 - VolBrickFirst3 + 1);
    v3i VolChunkFirst3 = From(VolExt) / ChunkDims3;
    v3i VolChunkLast3 = Last(VolExt) / ChunkDims3;
    extent VolExtentInChunks(VolChunkFirst3, VolChunkLast3 - VolChunkFirst3 + 1);
    v3i VolFileFirst3 = From(VolExt) / FileDims3;
    v3i VolFileLast3 = Last(VolExt) / FileDims3;
    extent VolExtentInFiles(VolFileFirst3, VolFileLast3 - VolFileFirst3 + 1);
    idx2_FileTraverse(
//      u64 FileAddr = FileTop.Address;
//      idx2_Assert(FileAddr == GetLinearFile(Idx2, Iter, FileTop.FileFrom3));
      idx2_ChunkTraverse(
//        u64 ChunkAddr = (FileAddr * Idx2.ChunksPerFiles[Iter]) + ChunkTop.Address;
//        idx2_Assert(ChunkAddr == GetLinearChunk(Idx2, Iter, ChunkTop.ChunkFrom3));
        D.ChunkInFile = ChunkTop.ChunkInFile;
        idx2_BrickTraverse(
          D.BrickInChunk = Top.BrickInChunk;
//          u64 BrickAddr = (ChunkAddr * Idx2.BricksPerChunks[Iter]) + Top.Address;
//          idx2_Assert(BrickAddr == GetLinearBrick(Idx2, Iter, Top.BrickFrom3));
          brick_volume BVol;
          Resize(&BVol.Vol, Idx2.BrickDimsExt3, dtype::float64, D.Alloc);
          // TODO: for progressive decompression, copy the data from BrickTable to BrickVol
          Fill(idx2_Range(f64, BVol.Vol), 0.0);
          D.Iter = Iter;
          D.Bricks3[Iter] = Top.BrickFrom3;
          D.Brick[Iter] = GetLinearBrick(Idx2, Iter, Top.BrickFrom3);
          u64 BrickKey = GetBrickKey(Iter, D.Brick[Iter]);
          Insert(&D.BrickPool, BrickKey, BVol);
          u8 Mask = Iter == P.DecodeLevel ? P.DecodeMask : (Iter < P.DecodeLevel ? 0x1 : 0xFF);
          DecodeBrick(Idx2, P, &D, Mask, Accuracy);
          if (Iter == P.OutputLevel) {
            grid BrickGrid(Top.BrickFrom3 * BrickDims3, Idx2.BrickDims3, v3i(1 << Iter)); // TODO: the 1 << Iter is only true for 1 transform pass per level
            grid OutBrickGrid = Crop(OutGrid, BrickGrid);
            grid BrickGridLocal = Relative(OutBrickGrid, BrickGrid);
            if (P.OutMode == params::out_mode::WriteToFile) {
              if (OutVol.Vol.Type == dtype::float32)
                (CopyGridGrid<f64, f32>(BrickGridLocal, BVol.Vol, Relative(OutBrickGrid, OutGrid), &OutVol.Vol));
              else if (OutVol.Vol.Type == dtype::float64)
                (CopyGridGrid<f64, f64>(BrickGridLocal, BVol.Vol, Relative(OutBrickGrid, OutGrid), &OutVol.Vol));
//              CountZeroes += CopyGridGridCountZeroes(BrickGridLocal, BVol.Vol, Relative(OutBrickGrid, OutGrid), &OutVol.Vol);
            } else if (P.OutMode == params::out_mode::KeepInMemory) {
              if (OutVolMem.Type == dtype::float32)
                (CopyGridGrid<f64, f32>(BrickGridLocal, BVol.Vol, Relative(OutBrickGrid, OutGrid), &OutVolMem));
              else if (OutVolMem.Type == dtype::float64)
                (CopyGridGrid<f64, f64>(BrickGridLocal, BVol.Vol, Relative(OutBrickGrid, OutGrid), &OutVolMem));
            }
            Dealloc(&BVol.Vol);
            Delete(&D.BrickPool, BrickKey); // TODO: also delete the parent bricks once we are done
          }
          , 64
          , Idx2.BrickOrderChunks[Iter]
          , ChunkTop.ChunkFrom3 * Idx2.BricksPerChunk3s[Iter]
          , Idx2.BricksPerChunk3s[Iter]
          , ExtentInBricks
          , VolExtentInBricks
        );
        , 64
        , Idx2.ChunkOrderFiles[Iter]
        , FileTop.FileFrom3 * Idx2.ChunksPerFile3s[Iter]
        , Idx2.ChunksPerFile3s[Iter]
        , ExtentInChunks
        , VolExtentInChunks
      );
      , 64
      , Idx2.FileOrders[Iter]
      , v3i(0)
      , Idx2.NFiles3s[Iter]
      , ExtentInFiles
      , VolExtentInFiles
    );
  } // end level loop
//  printf("count zeroes        = %lld\n", CountZeroes);
  printf("total decode time   = %f\n", Seconds(ElapsedTime(&DecodeTimer)));
  printf("io time             = %f\n", Seconds(DecodeIOTime_));
  printf("data movement time  = %f\n", Seconds(DataMovementTime_));
  printf("rdo   bytes read    = %" PRIi64 "\n", BytesRdos_);
  printf("exp   bytes read    = %" PRIi64 "\n", BytesExps_);
  printf("data  bytes read    = %" PRIi64 "\n", BytesData_);
  printf("total bytes read    = %" PRIi64 "\n", BytesRdos_ + BytesExps_ + BytesData_);
}

} // namespace idx2
//
#undef idx2_NextMorton
#undef idx2_NonExtDims
#undef idx2_ExtDims
