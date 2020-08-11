#include "mg_common.h"
#include "mg_filesystem.h"
#include "mg_signal_processing.h"
#include "mg_volume.h"
#include "mg_wz.h"
#include "mg_zfp.h"

namespace mg {

static mg_Inline u64
GetFileAddressV0_0(int BricksPerFile, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  (void)BricksPerFile;
  (void)Brick;
  (void)Level;
  (void)BitPlane;
  return u64(Iter);
}

static file_id
ConstructFilePathV0_0(const wz& Wz, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  #define mg_PrintIteration mg_Print(&Pr, "/I%02x", Iter);
  #define mg_PrintExtension mg_Print(&Pr, ".bin");
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  mg_Print(&Pr, "%s/%s/", Wz.Name, Wz.Field);
  mg_PrintIteration; mg_PrintExtension;
  u64 FileId = GetFileAddressV0_0(Wz.BricksPerFiles[Iter], Brick, Iter, Level, BitPlane);
  return file_id{stref{FilePath, Pr.Size}, FileId};
  #undef mg_PrintIteration
  #undef mg_PrintExtension
}

#define mg_NextMorton(Morton, Row3, Dims3)\
  if (!(Row3 < Dims3)) {\
    int B = Lsb(Morton);\
    mg_Assert(B >= 0);\
    Morton = (((Morton >> (B + 1)) + 1) << (B + 1)) - 1;\
    continue;\
  }

/* V0_0
- only do wavelet transform (no compression)
- write each iteration to one file
- only support linear decoding of each file */
void
EncodeSubbandV0_0(wz* Wz, encode_data* E, const grid& SbGrid, volume* BrickVol) {
  u64 Brick = E->Brick[E->Iter];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Wz->BlockDims3 - 1) / Wz->BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(*Wz, Brick, E->Iter, 0, 0);
  mg_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  mg_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    mg_NextMorton(Block, Z3, NBlocks3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(Wz->BlockDims3));
    v3i D3 = Z3 * Wz->BlockDims3;
    v3i BlockDims3 = Min(Wz->BlockDims3, SbDims3 - D3);
    bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Wz->NIterations && BlockDims3 == Wz->BlockDims3;
    if (CodedInNextIter) continue;
    /* copy the samples to the local buffer */
    v3i S3;
    mg_BeginFor3(S3, v3i::Zero, BlockDims3, v3i::One) { // sample loop
      mg_Assert(D3 + S3 < SbDims3);
      BlockFloats[Row(BlockDims3, S3)] = BrickVol->At<f64>(SbGrid, D3 + S3);
    } mg_EndFor3 // end sample loop
    WriteBuffer(Fp, BufFloats);
  }
}

/* NOTE: in v0.0, we only support reading the data from beginning to end on each iteration */
error<wz_err_code>
DecodeSubbandV0_0(const wz& Wz, decode_data* D, const grid& SbGrid, volume* BVol) {
  u64 Brick = D->Brick[D->Iter];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Wz.BlockDims3 - 1) / Wz.BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(Wz, Brick, D->Iter, 0, 0);
  mg_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"),, if (Fp) fclose(Fp));
  mg_FSeek(Fp, D->Offsets[D->Iter], SEEK_SET);
  /* first, read the block exponents */
  mg_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    mg_NextMorton(Block, Z3, NBlocks3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(Wz.BlockDims3));
    v3i D3 = Z3 * Wz.BlockDims3;
    v3i BlockDims3 = Min(Wz.BlockDims3, SbDims3 - D3);
    bool CodedInNextIter = D->Level == 0 && D->Iter + 1 < Wz.NIterations && BlockDims3 == Wz.BlockDims3;
    if (CodedInNextIter) continue;
    ReadBuffer(Fp, &BufFloats);
    v3i S3;
    mg_BeginFor3(S3, v3i::Zero, BlockDims3, v3i::One) { // sample loop
      mg_Assert(D3 + S3 < SbDims3);
      BVol->At<f64>(SbGrid, D3 + S3) = BlockFloats[Row(BlockDims3, S3)];
    } mg_EndFor3 // end sample loop
  }
  D->Offsets[D->Iter] = mg_FTell(Fp);
  return mg_Error(wz_err_code::NoError);
}

/* V0_1:
- do wavelet transform and compression
- write each iteration to one file
- only support linear decoding of each file */
void
EncodeSubbandV0_1(wz* Wz, encode_data* E, const grid& SbGrid, volume* BrickVol) {
  u64 Brick = E->Brick[E->Iter];
  v3i SbDims3 = Dims(SbGrid);
  const i8 NBitPlanes = mg_BitSizeOf(f64);
  v3i NBlocks3 = (SbDims3 + Wz->BlockDims3 - 1) / Wz->BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(*Wz, Brick, E->Iter, 0, 0);
  mg_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  Rewind(&E->BlockStream);
  GrowToAccomodate(&E->BlockStream, 8 * 1024 * 1024); // 8MB
  InitWrite(&E->BlockStream, E->BlockStream.Stream);
  mg_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    mg_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Wz->BlockDims3;
    v3i BlockDims3 = Min(Wz->BlockDims3, SbDims3 - D3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(BlockDims3));
    i64 BlockInts  [4 * 4 * 4] = {}; buffer_t BufInts  (BlockInts  , Prod(BlockDims3));
    u64 BlockUInts [4 * 4 * 4] = {}; buffer_t BufUInts (BlockUInts , Prod(BlockDims3));
    bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Wz->NIterations && BlockDims3 == Wz->BlockDims3;
    if (CodedInNextIter) continue;
    /* copy the samples to the local buffer */
    v3i S3;
    int J = 0;
    mg_BeginFor3(S3, v3i::Zero, BlockDims3, v3i::One) { // sample loop
      mg_Assert(D3 + S3 < SbDims3);
      BlockFloats[J++] = BrickVol->At<f64>(SbGrid, D3 + S3);
    } mg_EndFor3 // end sample loop
    i8 NDims = NumDims(BlockDims3);
    const int NVals = 1 << (2 * NDims);
    const i8 Prec = mg_BitSizeOf(f64) - 1 - NDims;
    // TODO: deal with Float32
    const i16 EMax = (i16)Quantize(Prec, BufFloats, &BufInts);
    ForwardZfp(BlockInts, NDims);
    ForwardShuffle(BlockInts, BlockUInts, NDims);
    i8 N = 0; // number of significant coefficients in the block so far
    Write(&E->BlockStream, EMax + traits<f64>::ExpBias, traits<f64>::ExpBits);
    mg_InclusiveForBackward(i8, Bp, NBitPlanes - 1, 0) { // bit plane loop
      i16 RealBp = Bp + EMax;
      bool TooHighPrecision = NBitPlanes - 6 > RealBp - Exponent(Wz->Accuracy) + 1;
      if (TooHighPrecision) break;
      GrowIfTooFull(&E->BlockStream);
      Encode(BlockUInts, NVals, Bp, N, &E->BlockStream);
    }
  }
  Flush(&E->BlockStream);
  WritePOD(Fp, (int)Size(E->BlockStream));
  WriteBuffer(Fp, ToBuffer(E->BlockStream));
}

error<wz_err_code>
DecodeSubbandV0_1(const wz& Wz, decode_data* D, const grid& SbGrid, volume* BVol) {
  u64 Brick = D->Brick[D->Iter];
  v3i SbDims3 = Dims(SbGrid);
  const i8 NBitPlanes = mg_BitSizeOf(f64);
  v3i NBlocks3 = (SbDims3 + Wz.BlockDims3 - 1) / Wz.BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(Wz, Brick, D->Iter, 0, 0);
  mg_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"),, if (Fp) fclose(Fp));
  mg_FSeek(Fp, D->Offsets[D->Iter], SEEK_SET);
  int Sz = 0; ReadPOD(Fp, &Sz);
  Rewind(&D->BlockStream);
  GrowToAccomodate(&D->BlockStream, Max(Sz, 8 * 1024 * 1024));
  ReadBuffer(Fp, &D->BlockStream.Stream, Sz);
  InitRead(&D->BlockStream, D->BlockStream.Stream);
  /* first, read the block exponents */
  mg_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    mg_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Wz.BlockDims3;
    v3i BlockDims3 = Min(Wz.BlockDims3, SbDims3 - D3);
    f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(BlockDims3));
    i64 BlockInts  [4 * 4 * 4] = {}; buffer_t BufInts  (BlockInts  , Prod(BlockDims3));
    u64 BlockUInts [4 * 4 * 4] = {}; buffer_t BufUInts (BlockUInts , Prod(BlockDims3));
    bool CodedInNextIter = D->Level == 0 && D->Iter + 1 < Wz.NIterations && BlockDims3 == Wz.BlockDims3;
    if (CodedInNextIter) continue;
    int NDims = NumDims(BlockDims3);
    const int NVals = 1 << (2 * NDims);
    const int Prec = mg_BitSizeOf(f64) - 1 - NDims;
    i16 EMax = Read(&D->BlockStream, traits<f64>::ExpBits) - traits<f64>::ExpBias;
    i8 N = 0;
    mg_InclusiveForBackward(i8, Bp, NBitPlanes - 1, 0) { // bit plane loop
      i16 RealBp = Bp + EMax;
      if (NBitPlanes - 6 > RealBp - Exponent(Wz.Accuracy) + 1) break;
      Decode(BlockUInts, NVals, Bp, N, &D->BlockStream);
    }
    InverseShuffle(BlockUInts, BlockInts, NDims);
    InverseZfp(BlockInts, NDims);
    Dequantize(EMax, Prec, BufInts, &BufFloats);
    v3i S3;
    int J = 0;
    mg_BeginFor3(S3, v3i::Zero, BlockDims3, v3i::One) { // sample loop
      mg_Assert(D3 + S3 < SbDims3);
      BVol->At<f64>(SbGrid, D3 + S3) = BlockFloats[J++];
    } mg_EndFor3 // end sample loop
  }
  D->Offsets[D->Iter] = mg_FTell(Fp);
  return mg_Error(wz_err_code::NoError);
}

#undef mg_NextMorton

} // namespace mg
