#include "idx2Encode.h"
#include "FileSystem.h"
#include "Function.h"
#include "InputOutput.h"
#include "Statistics.h"
#include "Timer.h"
#include "VarInt.h"
#include "Zfp.h"
#include "idx2Common.h"
#include "idx2Lookup.h"
#include "idx2SparseBricks.h"
#include "idx2Write.h"
#include "sexpr.h"
#include "zstd/zstd.h"
#include <algorithm>


namespace idx2
{


static void
EncodeBrickSubbandExponents_v2(idx2_file* Idx2,
                               encode_data* E,
                               const v3i& NBlocks3,
                               const u64 Brick)
{

  /* query the right sub channel for the block exponents */
  const u64 SubChanKey = GetAddress(0, 0, E->Level, E->Subband, ExponentBitPlane_);
  auto ScIt = Lookup(E->SubChannels, SubChanKey);
  if (!ScIt)
  {
    sub_channel SubChan;
    Init(&SubChan);
    Insert(&ScIt, SubChanKey, SubChan);
  }
  idx2_Assert(ScIt);
  sub_channel* Sc = ScIt.Val;

  /* write the last chunk exponents if this is the first brick of the new chunk */
  bool NewChunk = Brick >= (Sc->LastChunk + 1) * Idx2->BricksPerChunk[E->Level];
  if (NewChunk)
  {
    WriteChunkExponents_v2(*Idx2, E, Sc, E->Level, E->Subband);
    Sc->LastChunk = Brick >> Log2Ceil(Idx2->BricksPerChunk[E->Level]);
  }
  /* write the min exponent */
  GrowToAccomodate(&Sc->BlockExpStream, 2 * Size(E->SubbandExps));
  idx2_For (int, I, 0, Size(E->SubbandExps))
  {
    i16 S = E->SubbandExps[I] + (SizeOf(Idx2->DType) > 4 ? traits<f64>::ExpBias : traits<f32>::ExpBias);
    // we use 16 bits for f64 exponents (instead of 11) so that zstd compression works later
    Write(&Sc->BlockExpStream, S, SizeOf(Idx2->DType) > 4 ? 16 : traits<f32>::ExpBits);
  }
  /* write brick exponent size */
  GrowToAccomodate(&Sc->BrickExpStream, Size(Sc->BlockExpStream));
  WriteStream(&Sc->BrickExpStream, &Sc->BlockExpStream);
  //BlockEMaxStat.Add((f64)Size(Sc->BlockEMaxesStream));
  Rewind(&Sc->BlockExpStream);
  Sc->LastBrick = Brick;
}


static void
EncodeBrickSubbandMetadata_v2(idx2_file* Idx2,
                              encode_data* E,
                              const v3i& NBlocks3,
                              const v3i& SbDims3,
                              const u64 Brick,
                              const u32 LastBlock)
{
  /* pass 2: encode the brick meta info */
  idx2_InclusiveFor (u32, Block, 0, LastBlock)
  {
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2->BlockDims3;
    v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
    bool CodedInNextLevel =
      E->Subband == 0 && E->Level + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextLevel)
      continue;
    /* done at most once per brick, by the last significant block */
    idx2_For (int, I, 0, Size(E->LastSigBlock))
    {
      i16 BpKey = E->LastSigBlock[I].BitPlane;
      if (Block != E->LastSigBlock[I].Block)
        continue;

      u32 ChannelKey = GetChannelKey(BpKey, E->Level, E->Subband);
      auto ChannelIt = Lookup(E->Channels, ChannelKey);
      idx2_Assert(ChannelIt);
      channel* C = ChannelIt.Val;
      /* write brick delta */
      if (C->NBricks == 0)
      { // start of a chunk
        GrowToAccomodate(&C->BrickDeltasStream, 8);
        WriteVarByte(&C->BrickDeltasStream, Brick);
      }
      else
      {
        GrowToAccomodate(&C->BrickDeltasStream, (Brick - C->LastBrick - 1 + 8) / 8);
        WriteUnary(&C->BrickDeltasStream, u32(Brick - C->LastBrick - 1));
      }
      /* write brick size */
      i64 BrickSize = Size(C->BlockStream);
      GrowToAccomodate(&C->BrickSizeStream, 4);
      WriteVarByte(&C->BrickSizeStream, BrickSize);
      /* write brick data */
      GrowToAccomodate(&C->BrickStream, BrickSize);
      WriteStream(&C->BrickStream, &C->BlockStream);
      //BlockStat.Add((f64)Size(C->BlockStream));
      Rewind(&C->BlockStream);
      ++C->NBricks;
      C->LastBrick = Brick;
    } // end bit plane loop
  }   // end zfp block loop
}


static void
EncodeSubbandBlocks_v2(idx2_file* Idx2,
                       encode_data* E,
                       const grid& SbGrid,
                       const v3i& SbDims3,
                       const v3i& NBlocks3,
                       const u64 Brick,
                       const u32 LastBlock,
                       volume* BrickVol)
{
  const i8 NBitPlanes = idx2_BitSizeOf(u64);
  Clear(&E->LastSigBlock);
  Reserve(&E->LastSigBlock, NBitPlanes);
  Clear(&E->SubbandExps);
  Reserve(&E->SubbandExps, Prod(NBlocks3));

  /* pass 1: compress the blocks */
  idx2_InclusiveFor (u32, Block, 0, LastBlock)
  { // zfp block loop
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
    u64 BlockUInts[4 * 4 * 4];
    buffer_t BufUInts(BlockUInts, NVals);
    bool CodedInNextLevel =
      E->Subband == 0 && E->Level + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextLevel)
      continue;

    /* copy the samples to the local buffer and zfp transform them */
    v3i S3;
    int J = 0;
    v3i From3 = From(SbGrid), Strd3 = Strd(SbGrid);
    idx2_BeginFor3 (S3, v3i(0), BlockDims3, v3i(1))
    { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BlockFloats[J++] = BrickVol->At<f64>(From3, Strd3, D3 + S3);
    }
    idx2_EndFor3; // end sample loop
    /* zfp transform and shuffle */
    const i16 EMax = SizeOf(Idx2->DType) > 4 ? (i16)QuantizeF64(Prec, BufFloats, &BufInts)
                                             : (i16)QuantizeF32(Prec, BufFloats, &BufInts);
    PushBack(&E->SubbandExps, EMax);
    ForwardZfp((i64*)BlockFloats, NDims);
    ForwardShuffle((i64*)BlockFloats, BlockUInts, NDims);

    /* zfp encode */
    i8 N = 0; // number of significant coefficients in the block so far
    i8 EndBitPlane = Min(i8(BitSizeOf(Idx2->DType)), NBitPlanes);
    int Bpc = Idx2->BitPlanesPerChunk;
    idx2_InclusiveForBackward (i8, Bp, NBitPlanes - 1, NBitPlanes - EndBitPlane)
    { // bit plane loop
      i16 RealBp = Bp + EMax;
      i16 BpKey = (RealBp + BitPlaneKeyBias_) / Bpc; // make it so that the BpKey is positive
      bool TooHighPrecision = NBitPlanes - 6 > RealBp - Exponent(Idx2->Tolerance) + 1;
      if (TooHighPrecision)
      {
        if ((RealBp + BitPlaneKeyBias_) % Bpc == 0) // make sure we encode full "block" of BpKey
          break;
      }

      u32 ChannelKey = GetChannelKey(BpKey, E->Level, E->Subband);
      auto ChannelIt = Lookup(E->Channels, ChannelKey);
      if (!ChannelIt)
      {
        channel Channel;
        Init(&Channel);
        Insert(&ChannelIt, ChannelKey, Channel);
      }
      idx2_Assert(ChannelIt);
      channel* C = ChannelIt.Val;

      /* record the last significant block on this bit plane */
      // u32 BlockDelta = Block;
      int I = 0;
      for (; I < Size(E->LastSigBlock); ++I)
      {
        if (E->LastSigBlock[I].BitPlane == BpKey)
        {
          idx2_Assert(Block > E->LastSigBlock[I].Block);
          // BlockDelta = Block - E->BlockSigs[I].Block - 1;
          E->LastSigBlock[I].Block = Block;
          break;
        }
      }

      /* write chunk if this brick is after the last chunk */
      // first block that becomes significant on this bit plane
      // TODO: need to think about the logic of LastSigBlock in conjunction with BpKey
      bool FirstSigBlock = (I == Size(E->LastSigBlock));
      bool BrickNotEmpty = Size(C->BrickStream) > 0;
      bool NewChunk = Brick >= (C->LastChunk + 1) * Idx2->BricksPerChunk[E->Level];
      if (FirstSigBlock)
      {
        if (NewChunk)
        {
          if (BrickNotEmpty)
            WriteChunk_v2(*Idx2, E, C, E->Level, E->Subband, BpKey);
          C->NBricks = 0;
          C->LastChunk = Brick >> Log2Ceil(Idx2->BricksPerChunk[E->Level]);
        }
        PushBack(&E->LastSigBlock, block_sig{ Block, BpKey });
      }

      /* encode the block */
      GrowIfTooFull(&C->BlockStream);
      Encode(BlockUInts, NVals, Bp, N, &C->BlockStream);
    } // end bit plane loop
  } // end zfp block loop
}


  // TODO: return an error code
static void
EncodeSubband_v2(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol)
{
  const u64 Brick = E->Brick[E->Level];
  const v3i SbDims3 = Dims(SbGrid);
  const v3i NBlocks3 = (SbDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
  const u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));

  EncodeSubbandBlocks_v2(Idx2, E, SbGrid, SbDims3, NBlocks3, Brick, LastBlock, BrickVol);
  EncodeBrickSubbandExponents_v2(Idx2, E, NBlocks3, Brick);
  EncodeBrickSubbandMetadata_v2(Idx2, E, NBlocks3, SbDims3, Brick, LastBlock);
}


static void
EncodeBrick_v2(idx2_file* Idx2, const params& P, encode_data* E, bool IncrementLevel = false)
{
  idx2_Assert(Idx2->NLevels <= idx2_file::MaxLevels);

  i8 Level = E->Level += IncrementLevel;

  u64 Brick = E->Brick[Level];
  //printf(
  //  "level %d brick " idx2_PrStrV3i " %" PRIu64 "\n", Iter, idx2_PrV3i(E->Bricks3[Iter]), Brick);
  auto BIt = Lookup(E->BrickPool, GetBrickKey(Level, Brick));
  idx2_Assert(BIt);
  volume& BVol = BIt.Val->Vol;
  idx2_Assert(BVol.Buffer);

  // TODO: we do not need to pre-extrapolate, instead just compute and store the extrapolated values
  ExtrapolateCdf53(Dims(BIt.Val->ExtentLocal), Idx2->TransformOrder, &BVol);

  /* do wavelet transform */
  bool CoarsestLevel = Level + 1 == Idx2->NLevels; // only normalize
  ForwardCdf53(Idx2->BrickDimsExt3, E->Level, Idx2->Subbands, Idx2->TransformDetails, &BVol, CoarsestLevel);

  /* recursively encode the brick, one subband at a time */
  idx2_For (i8, Sb, 0, Size(Idx2->Subbands))
  { // subband loop
    const subband& S = Idx2->Subbands[Sb];
    v3i SbDimsNonExt3 = idx2_NonExtDims(Dims(S.Grid));
    i8 NextLevel = Level + 1;
    if (Sb == 0 && NextLevel < Idx2->NLevels)
    { // need to encode the parent brick
      /* find the parent brick and create it if not found */
      v3i Brick3 = E->Bricks3[Level];
      v3i PBrick3 = (E->Bricks3[NextLevel] = Brick3 / Idx2->GroupBrick3);
      u64 PBrick = (E->Brick[NextLevel] = GetLinearBrick(*Idx2, NextLevel, PBrick3));
      u64 PKey = GetBrickKey(NextLevel, PBrick);
      auto PbIt = Lookup(E->BrickPool, PKey);
      if (!PbIt)
      { // instantiate the parent brick in the hash table
        brick_volume PBrickVol;
        Resize(&PBrickVol.Vol, Idx2->BrickDimsExt3, dtype::float64, E->Alloc);
        Fill(idx2_Range(f64, PBrickVol.Vol), 0.0);
        v3i From3 = (Brick3 / Idx2->GroupBrick3) * Idx2->GroupBrick3;
        v3i NChildren3 =
          Dims(Crop(extent(From3, Idx2->GroupBrick3), extent(Idx2->NBricks3[Level])));
        PBrickVol.NChildrenMax = (i8)Prod(NChildren3);
        PBrickVol.ExtentLocal = extent(NChildren3 * SbDimsNonExt3);
        Insert(&PbIt, PKey, PBrickVol);
      }
      /* copy data to the parent brick and (optionally) encode it */
      v3i LocalBrickPos3 = Brick3 % Idx2->GroupBrick3;
      grid SbGridNonExt = S.Grid;
      SetDims(&SbGridNonExt, SbDimsNonExt3);
      extent ToGrid(LocalBrickPos3 * SbDimsNonExt3, SbDimsNonExt3);
      CopyGridExtent<f64, f64>(SbGridNonExt, BVol, ToGrid, &PbIt.Val->Vol);
      //      Copy(SbGridNonExt, BVol, ToGrid, &PbIt.Val->Vol);
      bool LastChild = ++PbIt.Val->NChildrenDecoded == PbIt.Val->NChildrenMax;
      if (LastChild)
        EncodeBrick_v2(Idx2, P, E, true);
    } // end Sb == 0 && NextIteration < Idx2->NLevels
    E->Subband = Sb;
    EncodeSubband_v2(Idx2, E, S.Grid, &BVol);
  } // end subband loop
  Dealloc(&BVol);
  Delete(&E->BrickPool, GetBrickKey(Level, Brick));
  E->Level -= IncrementLevel;
}


// TODO: return true error code
struct channel_ptr
{
  i8 Iteration = 0;
  i8 Level = 0;
  i16 BitPlane = 0;
  channel* ChunkPtr = nullptr;
  idx2_Inline bool operator<(const channel_ptr& Other) const
  {
    if (Iteration == Other.Iteration)
    {
      if (BitPlane == Other.BitPlane)
        return Level < Other.Level;
      return BitPlane > Other.BitPlane;
    }
    return Iteration < Other.Iteration;
  }
};

static error<idx2_err_code>
TraverseFiles(const traverse_item& File)
{
}

error<idx2_err_code>
Encode_v2(idx2_file* Idx2, const params& P, brick_copier& Copier)
{
  u64 TraverseOrder;
  v3i FileFrom3;
  v3i FileTo3;
  extent Extent;
  extent VolExtent;

  //TraverseHierarchy();

  return idx2_Error(idx2_err_code::NoError);
}


} // namespace idx2
