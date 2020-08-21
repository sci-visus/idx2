#include "idx2_common.h"
#include "idx2_filesystem.h"
#include "idx2_function.h"
#include "idx2_volume.h"
#include "idx2_v1.h"
#include "idx2_zfp.h"

namespace idx2 {

static idx2_Inline u64
GetFileAddressV0_0(int BricksPerFile, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  (void)BricksPerFile;
  (void)Brick;
  (void)Level;
  (void)BitPlane;
  return u64(Iter);
}

static file_id
ConstructFilePathV0_0(const idx2_file& Idx2, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  #define idx2_PrintIteration idx2_Print(&Pr, "/I%02x", Iter);
  #define idx2_PrintExtension idx2_Print(&Pr, ".bin");
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%s/%s/", Idx2.Name, Idx2.Field);
  idx2_PrintIteration; idx2_PrintExtension;
  u64 FileId = GetFileAddressV0_0(Idx2.BricksPerFiles[Iter], Brick, Iter, Level, BitPlane);
  return file_id{stref{FilePath, Pr.Size}, FileId};
  #undef idx2_PrintIteration
  #undef idx2_PrintExtension
}

#define idx2_NextMorton(Morton, Row3, Dims3)\
  if (!(Row3 < Dims3)) {\
    int B = Lsb(Morton);\
    idx2_Assert(B >= 0);\
    Morton = (((Morton >> (B + 1)) + 1) << (B + 1)) - 1;\
    continue;\
  }

/* V0_0
- only do wavelet transform (no compression)
- write each iteration to one file
- only support linear decoding of each file */
void
EncodeSubbandV0_0(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol) {
  u64 Brick = E->Brick[E->Iter];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(*Idx2, Brick, E->Iter, 0, 0);
  idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(Idx2->BlockDims3));
    v3i D3 = Z3 * Idx2->BlockDims3;
    v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
    bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextIter) continue;
    /* copy the samples to the local buffer */
    v3i S3;
    idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BlockFloats[Row(BlockDims3, S3)] = BrickVol->At<f64>(SbGrid, D3 + S3);
    } idx2_EndFor3 // end sample loop
    WriteBuffer(Fp, BufFloats);
  }
}

/* NOTE: in v0.0, we only support reading the data from beginning to end on each iteration */
error<idx2_file_err_code>
DecodeSubbandV0_0(const idx2_file& Idx2, decode_data* D, const grid& SbGrid, volume* BVol) {
  u64 Brick = D->Brick[D->Iter];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Idx2.BlockDims3 - 1) / Idx2.BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(Idx2, Brick, D->Iter, 0, 0);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"),, if (Fp) fclose(Fp));
  idx2_FSeek(Fp, D->Offsets[D->Iter], SEEK_SET);
  /* first, read the block exponents */
  idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(Idx2.BlockDims3));
    v3i D3 = Z3 * Idx2.BlockDims3;
    v3i BlockDims3 = Min(Idx2.BlockDims3, SbDims3 - D3);
    bool CodedInNextIter = D->Level == 0 && D->Iter + 1 < Idx2.NLevels && BlockDims3 == Idx2.BlockDims3;
    if (CodedInNextIter) continue;
    ReadBuffer(Fp, &BufFloats);
    v3i S3;
    idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BVol->At<f64>(SbGrid, D3 + S3) = BlockFloats[Row(BlockDims3, S3)];
    } idx2_EndFor3 // end sample loop
  }
  D->Offsets[D->Iter] = idx2_FTell(Fp);
  return idx2_Error(idx2_file_err_code::NoError);
}

/* V0_1:
- do wavelet transform and compression
- write each iteration to one file
- only support linear decoding of each file */
void
EncodeSubbandV0_1(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol) {
  u64 Brick = E->Brick[E->Iter];
  v3i SbDims3 = Dims(SbGrid);
  const i8 NBitPlanes = idx2_BitSizeOf(f64);
  v3i NBlocks3 = (SbDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(*Idx2, Brick, E->Iter, 0, 0);
  idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  Rewind(&E->BlockStream);
  GrowToAccomodate(&E->BlockStream, 8 * 1024 * 1024); // 8MB
  InitWrite(&E->BlockStream, E->BlockStream.Stream);
  idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2->BlockDims3;
    v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(BlockDims3));
    i64 BlockInts  [4 * 4 * 4] = {}; buffer_t BufInts  (BlockInts  , Prod(BlockDims3));
    u64 BlockUInts [4 * 4 * 4] = {}; buffer_t BufUInts (BlockUInts , Prod(BlockDims3));
    bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextIter) continue;
    /* copy the samples to the local buffer */
    v3i S3;
    int J = 0;
    idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BlockFloats[J++] = BrickVol->At<f64>(SbGrid, D3 + S3);
    } idx2_EndFor3 // end sample loop
    i8 NDims = (i8)NumDims(BlockDims3);
    const int NVals = 1 << (2 * NDims);
    const i8 Prec = idx2_BitSizeOf(f64) - 1 - NDims;
    // TODO: deal with Float32
    const i16 EMax = (i16)Quantize(Prec, BufFloats, &BufInts);
    ForwardZfp(BlockInts, NDims);
    ForwardShuffle(BlockInts, BlockUInts, NDims);
    i8 N = 0; // number of significant coefficients in the block so far
    Write(&E->BlockStream, EMax + traits<f64>::ExpBias, traits<f64>::ExpBits);
    idx2_InclusiveForBackward(i8, Bp, NBitPlanes - 1, 0) { // bit plane loop
      i16 RealBp = Bp + EMax;
      bool TooHighPrecision = NBitPlanes - 6 > RealBp - Exponent(Idx2->Accuracy) + 1;
      if (TooHighPrecision) break;
      GrowIfTooFull(&E->BlockStream);
      Encode(BlockUInts, NVals, Bp, N, &E->BlockStream);
    }
  }
  Flush(&E->BlockStream);
  WritePOD(Fp, (int)Size(E->BlockStream));
  WriteBuffer(Fp, ToBuffer(E->BlockStream));
}

error<idx2_file_err_code>
DecodeSubbandV0_1(const idx2_file& Idx2, decode_data* D, const grid& SbGrid, volume* BVol) {
  u64 Brick = D->Brick[D->Iter];
  v3i SbDims3 = Dims(SbGrid);
  const i8 NBitPlanes = idx2_BitSizeOf(f64);
  v3i NBlocks3 = (SbDims3 + Idx2.BlockDims3 - 1) / Idx2.BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(Idx2, Brick, D->Iter, 0, 0);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"),, if (Fp) fclose(Fp));
  idx2_FSeek(Fp, D->Offsets[D->Iter], SEEK_SET);
  int Sz = 0; ReadPOD(Fp, &Sz);
  Rewind(&D->BlockStream);
  GrowToAccomodate(&D->BlockStream, Max(Sz, 8 * 1024 * 1024));
  ReadBuffer(Fp, &D->BlockStream.Stream, Sz);
  InitRead(&D->BlockStream, D->BlockStream.Stream);
  /* first, read the block exponents */
  idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2.BlockDims3;
    v3i BlockDims3 = Min(Idx2.BlockDims3, SbDims3 - D3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(BlockDims3));
    i64 BlockInts  [4 * 4 * 4] = {}; buffer_t BufInts  (BlockInts  , Prod(BlockDims3));
    u64 BlockUInts [4 * 4 * 4] = {}; buffer_t BufUInts (BlockUInts , Prod(BlockDims3));
    bool CodedInNextIter = D->Level == 0 && D->Iter + 1 < Idx2.NLevels && BlockDims3 == Idx2.BlockDims3;
    if (CodedInNextIter) continue;
    int NDims = NumDims(BlockDims3);
    const int NVals = 1 << (2 * NDims);
    const int Prec = idx2_BitSizeOf(f64) - 1 - NDims;
    i16 EMax = i16(Read(&D->BlockStream, traits<f64>::ExpBits) - traits<f64>::ExpBias);
    i8 N = 0;
    idx2_InclusiveForBackward(i8, Bp, NBitPlanes - 1, 0) { // bit plane loop
      i16 RealBp = Bp + EMax;
      if (NBitPlanes - 6 > RealBp - Exponent(Idx2.Accuracy) + 1) break;
      Decode(BlockUInts, NVals, Bp, N, &D->BlockStream);
    }
    InverseShuffle(BlockUInts, BlockInts, NDims);
    InverseZfp(BlockInts, NDims);
    Dequantize(EMax, Prec, BufInts, &BufFloats);
    v3i S3;
    int J = 0;
    idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BVol->At<f64>(SbGrid, D3 + S3) = BlockFloats[J++];
    } idx2_EndFor3 // end sample loop
  }
  D->Offsets[D->Iter] = idx2_FTell(Fp);
  return idx2_Error(idx2_file_err_code::NoError);
}

#undef idx2_NextMorton

} // namespace idx2
