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
#include "idx2Write.h"
#include "sexpr.h"
#include "zstd/zstd.h"
#include <algorithm>


namespace idx2
{


static void
Init(encode_data* E, allocator* Alloc = nullptr);
static void
Dealloc(encode_data* E);


void
CompressBufZstd(const buffer& Input, bitstream* Output)
{
  if (Size(Input) == 0)
    return;
  size_t const MaxDstSize = ZSTD_compressBound(Size(Input));
  GrowToAccomodate(Output, MaxDstSize - Size(*Output));
  size_t const CpresSize =
    ZSTD_compress(Output->Stream.Data, MaxDstSize, Input.Data, Size(Input), 1);
  if (CpresSize <= 0)
  {
    fprintf(stderr, "CompressBufZstd failed\n");
    exit(1);
  }
  Output->BitPtr = CpresSize + Output->Stream.Data;
}


struct rdo_precompute
{
  u64 Address;
  int Start;
  array<int> Hull;             // convex hull
  array<int> TruncationPoints; // bit planes
  idx2_Inline bool operator<(const rdo_precompute& Other) const { return Address < Other.Address; }
};


struct rdo_mini
{
  f64 Distortion;
  i64 Length;
  f64 Lambda;
};


/* Rate distortion optimization is done once per tile and iter/level */
// TODO: change the word "Chunk" to "Tile" elsewhere where it makes sense
// TODO: normalize the distortion by the number of samples a chunk has
static void
RateDistortionOpt(const idx2_file& Idx2, encode_data* E)
{
  if (Size(Idx2.RdoLevels) == 0)
    return;
  constexpr u64 InfInt = 0x7FF0000000000000ull;
  const f64 Inf = *(f64*)(&InfInt);

  auto& ChunkRDOs = E->ChunkRDOs;
  i16 MinBitPlane = traits<i16>::Max, MaxBitPlane = traits<i16>::Min;
  idx2_ForEach (CIt, ChunkRDOs)
  {
    i16 BitPlane = i16(CIt->Address & 0xFFF);
    MinBitPlane = Min(MinBitPlane, BitPlane);
    MaxBitPlane = Max(MaxBitPlane, BitPlane);
  }
  //  InsertionSort(Begin(ChunkRDOs), End(ChunkRDOs)); // TODO: this should be quicksort
  std::sort(Begin(ChunkRDOs), End(ChunkRDOs));
  array<rdo_precompute> RdoPrecomputes;
  Reserve(&RdoPrecomputes, 128); // each rdo_precompute corresponds to a tile
  idx2_CleanUp(idx2_ForEach (It, RdoPrecomputes) {
    Dealloc(&It->Hull);
    Dealloc(&It->TruncationPoints);
  } Dealloc(&RdoPrecomputes););
  int TileStart = -1, TileEnd = -1;
  array<rdo_mini> RdoTile;
  idx2_CleanUp(Dealloc(&RdoTile)); // just for a single tile
  while (true)
  { // loop through all chunks and detect the tile boundaries (Start, End)
    TileStart = TileEnd + 1;
    if (TileStart >= Size(ChunkRDOs))
      break;
    TileEnd = TileStart + 1; // exclusive end
    const auto& C = ChunkRDOs[TileStart];
    while (TileEnd < Size(ChunkRDOs) && (ChunkRDOs[TileEnd].Address >> 12) == (C.Address >> 12))
    {
      ++TileEnd;
    }
    Clear(&RdoTile);
    Reserve(&RdoTile, TileEnd - TileStart + 1);
    i16 Bp = i16(ChunkRDOs[TileStart].Address & 0xFFF) + 1;
    PushBack(&RdoTile, rdo_mini{ pow(2.0, Bp), 0, Inf });
    i64 PrevLength = 0;
    idx2_For (int, Z, TileStart, TileEnd)
    {
      u64 Addr = (ChunkRDOs[Z].Address >> 12) << 12;
      auto LIt = Lookup(&E->ChunkRDOLengths, Addr);
      idx2_Assert(LIt);
      i16 BitPlane = i16(ChunkRDOs[Z].Address & 0xFFF);
      PushBack(&RdoTile, rdo_mini{ pow(2.0, BitPlane), PrevLength += ChunkRDOs[Z].Length, 0.0 });
      ChunkRDOs[Z].Length = PrevLength + *LIt.Val;
    }
    array<int> Hull;
    Reserve(&Hull, Size(RdoTile));
    PushBack(&Hull, 0);
    /* precompute the convex hull and lambdas */
    int HLast = 0;
    idx2_For (int, Z, 1, Size(RdoTile))
    {
      f64 DeltaD = RdoTile[HLast].Distortion - RdoTile[Z].Distortion;
      f64 DeltaL = f64(RdoTile[Z].Length - RdoTile[HLast].Length);
      if (DeltaD > 0)
      {
        while (DeltaD >= DeltaL * RdoTile[HLast].Lambda)
        {
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
    idx2_For (int, Z, 1, Size(RdoTile))
    {
      ChunkRDOs[Z + TileStart - 1].Lambda = RdoTile[Z].Lambda;
    }
    u64 Address = ChunkRDOs[TileStart].Address;
    PushBack(&RdoPrecomputes, rdo_precompute{ Address, TileStart, Hull, array<int>() });
  }

  /* search for a suitable global lambda */
  idx2_For (int, R, 0, Size(Idx2.RdoLevels))
  { // for all quality levels
    printf("optimizing for rdo level %d\n", R);
    f64 LowLambda = -2, HighLambda = -1, Lambda = 1;
    int Count = 0;
    do
    { // search for the best Lambda (TruncationPoints stores the output)
      if (LowLambda > 0 && HighLambda > 0)
      {
        ++Count;
        if (Count >= 20)
        {
          Lambda = HighLambda;
          break;
        }
        Lambda = 0.5 * LowLambda + 0.5 * HighLambda;
      }
      else if (Lambda == 0)
      {
        Lambda = HighLambda;
        break;
      }
      i64 TotalLength = 0;
      idx2_ForEach (TIt, RdoPrecomputes)
      { // for each tile
        int J = 0;
        while (J + 1 < Size(TIt->Hull))
        {
          int Z = TIt->Hull[J + 1] + TIt->Start - 1;
          idx2_Assert(Z >= TIt->Start);
          if (ChunkRDOs[Z].Lambda > Lambda)
            ++J;
          else
            break;
        }
        Resize(&TIt->TruncationPoints, Size(Idx2.RdoLevels));
        TIt->TruncationPoints[R] = J;
        TotalLength += J == 0 ? 0 : ChunkRDOs[TIt->Hull[J] + TIt->Start - 1].Length;
      }
      if (TotalLength > Idx2.RdoLevels[R])
      { // we overshot, need to increase lambda
        LowLambda = Lambda;
        if (HighLambda < 0)
          Lambda *= 2;
      }
      else
      { // we did not overshoot, need to decrease Lambda
        HighLambda = Lambda;
        if (LowLambda < 0)
          Lambda *= 0.5;
      }
      // idx2_Assert(LowLambda <= HighLambda);
    } while (true);
  }

  /* write the truncation points to files */
  //  InsertionSort(Begin(RdoPrecomputes), End(RdoPrecomputes)); // TODO: quicksort
  std::sort(Begin(RdoPrecomputes), End(RdoPrecomputes));
  i64 Pos = 0;
  idx2_RAII(array<i16>, Buffer, Reserve(&Buffer, 128));
  idx2_RAII(bitstream, BitStream, );
  idx2_For (i8, Iter, 0, Idx2.NLevels)
  {
    extent Ext(Idx2.Dims3);
    v3i BrickDims3 = Idx2.BrickDims3 * Pow(Idx2.GroupBrick3, Iter);
    v3i BrickFirst3 = From(Ext) / BrickDims3;
    v3i BrickLast3 = Last(Ext) / BrickDims3;
    extent ExtentInBricks(BrickFirst3, BrickLast3 - BrickFirst3 + 1);
    v3i ChunkDims3 = Idx2.BricksPerChunk3s[Iter] * BrickDims3;
    v3i ChunkFirst3 = From(Ext) / ChunkDims3;
    v3i ChunkLast3 = Last(Ext) / ChunkDims3;
    extent ExtentInChunks(ChunkFirst3, ChunkLast3 - ChunkFirst3 + 1);
    v3i FileDims3 = ChunkDims3 * Idx2.ChunksPerFile3s[Iter];
    v3i FileFirst3 = From(Ext) / FileDims3;
    v3i FileLast3 = Last(Ext) / FileDims3;
    extent ExtentInFiles(FileFirst3, FileLast3 - FileFirst3 + 1);
    extent VolExt(Idx2.Dims3);
    v3i VolBrickFirst3 = From(VolExt) / BrickDims3;
    v3i VolBrickLast3 = Last(VolExt) / BrickDims3;
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
      u64 FirstBrickAddr =
        ((FileAddr * Idx2.ChunksPerFile[Iter]) + 0) * Idx2.BricksPerChunk[Iter] + 0;
      file_id FileId = ConstructFilePathRdos(Idx2, FirstBrickAddr, Iter);
      int NumChunks = 0;
      idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
      idx2_ChunkTraverse(
        ++NumChunks; u64 ChunkAddr = (FileAddr * Idx2.ChunksPerFile[Iter]) + ChunkTop.Address;
        // ChunkInFile = ChunkTop.ChunkInFile;
        idx2_For (i8, Level, 0, Size(Idx2.Subbands)) {
          const auto& Rdo = RdoPrecomputes[Pos];
          i8 RdoIter = (Rdo.Address >> 60) & 0xF;
          u64 RdoAddress = (Rdo.Address >> 18) & 0x3FFFFFFFFFFull;
          i8 RdoLevel = (Rdo.Address >> 12) & 0x3F;
          if (RdoIter == Iter && RdoLevel == Level && RdoAddress == ChunkAddr)
          {
            ++Pos;
            idx2_For (int, R, 0, Size(Idx2.RdoLevels))
            { // for each quality level
              int J = Rdo.TruncationPoints[R];
              if (J == 0)
              {
                PushBack(&Buffer, traits<i16>::Max);
                continue;
              }
              int Z = Rdo.Hull[J] + Rdo.Start - 1;
              u64 Address = ChunkRDOs[Z].Address;
              idx2_Assert((Rdo.Address >> 12) == (Address >> 12));
              i16 BitPlane = i16(Address & 0xFFF);
              PushBack(&Buffer, BitPlane);
            }
          }
          else
          { // somehow the tile is not there (tile produces no chunk i.e. it compresses to 0 bits)
            idx2_For (int, R, 0, Size(Idx2.RdoLevels))
            { // for each quality level
              PushBack(&Buffer, traits<i16>::Max);
            }
          }
        } // end level loop
        ,
        64,
        Idx2.ChunksOrderInFile[Iter],
        FileTop.FileFrom3 * Idx2.ChunksPerFile3s[Iter],
        Idx2.ChunksPerFile3s[Iter],
        ExtentInChunks,
        VolExtentInChunks); // end chunk (tile) traverse
      buffer Buf(Buffer.Buffer.Data, Size(Buffer) * sizeof(i16));
      CompressBufZstd(Buf, &BitStream);
      WriteBuffer(Fp, buffer{ BitStream.Stream.Data, Size(BitStream) });
      WritePOD(Fp, NumChunks);
      fclose(Fp);
      Clear(&Buffer);
      Rewind(&BitStream);
      , 64, Idx2.FilesOrder[Iter], v3i(0), Idx2.NFiles3[Iter], ExtentInFiles, VolExtentInFiles);
  } // end level loop
  idx2_Assert(Pos == Size(RdoPrecomputes));
}


// TODO: return an error code
static void
EncodeSubband(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol)
{
  u64 Brick = E->Brick[E->Iter];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  const i8 NBitPlanes = idx2_BitSizeOf(u64);
  Clear(&E->BlockSigs);
  Reserve(&E->BlockSigs, NBitPlanes);
  Clear(&E->EMaxes);
  Reserve(&E->EMaxes, Prod(NBlocks3));

  /* query the right sub channel for the block exponents */
  u16 SubChanKey = GetSubChannelKey(E->Iter, E->Level);
  auto ScIt = Lookup(&E->SubChannels, SubChanKey);
  if (!ScIt)
  {
    sub_channel SubChan;
    Init(&SubChan);
    Insert(&ScIt, SubChanKey, SubChan);
  }
  idx2_Assert(ScIt);
  sub_channel* Sc = ScIt.Val;

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
    bool CodedInNextIter =
      E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextIter)
      continue;
    /* copy the samples to the local buffer */
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
    PushBack(&E->EMaxes, EMax);
    ForwardZfp((i64*)BlockFloats, NDims);
    ForwardShuffle((i64*)BlockFloats, BlockUInts, NDims);
    /* zfp encode */
    i8 N = 0; // number of significant coefficients in the block so far
    i8 EndBitPlane = Min(i8(BitSizeOf(Idx2->DType)), NBitPlanes);
    idx2_InclusiveForBackward (i8, Bp, NBitPlanes - 1, NBitPlanes - EndBitPlane)
    { // bit plane loop
      i16 RealBp = Bp + EMax;
      bool TooHighPrecision = NBitPlanes - 6 > RealBp - Exponent(Idx2->Accuracy) + 1;
      if (TooHighPrecision)
        break;
      u32 ChannelKey = GetChannelKey(RealBp, E->Iter, E->Level);
      auto ChannelIt = Lookup(&E->Channels, ChannelKey);
      if (!ChannelIt)
      {
        channel Channel;
        Init(&Channel);
        Insert(&ChannelIt, ChannelKey, Channel);
      }
      idx2_Assert(ChannelIt);
      channel* C = ChannelIt.Val;
      /* write block id */
      // u32 BlockDelta = Block;
      int I = 0;
      for (; I < Size(E->BlockSigs); ++I)
      {
        if (E->BlockSigs[I].BitPlane == RealBp)
        {
          idx2_Assert(Block > E->BlockSigs[I].Block);
          // BlockDelta = Block - E->BlockSigs[I].Block - 1;
          E->BlockSigs[I].Block = Block;
          break;
        }
      }
      /* write chunk if this brick is after the last chunk */
      bool FirstSigBlock =
        I == Size(E->BlockSigs); // first block that becomes significant on this bit plane
      bool BrickNotEmpty = Size(C->BrickStream) > 0;
      bool NewChunk =
        Brick >= (C->LastChunk + 1) * Idx2->BricksPerChunk[E->Iter]; // TODO: multiplier?
      if (FirstSigBlock)
      {
        if (NewChunk)
        {
          if (BrickNotEmpty)
            WriteChunk(*Idx2, E, C, E->Iter, E->Level, RealBp);
          C->NBricks = 0;
          C->LastChunk = Brick >> Log2Ceil(Idx2->BricksPerChunk[E->Iter]);
        }
        PushBack(&E->BlockSigs, block_sig{ Block, RealBp });
      }
      /* encode the block */
      GrowIfTooFull(&C->BlockStream);
      Encode(BlockUInts, NVals, Bp, N, &C->BlockStream);
    } // end bit plane loop
  }   // end zfp block loop

  /* write the last chunk exponents if this is the first brick of the new chunk */
  bool NewChunk = Brick >= (Sc->LastChunk + 1) * Idx2->BricksPerChunk[E->Iter];
  if (NewChunk)
  {
    WriteChunkExponents(*Idx2, E, Sc, E->Iter, E->Level);
    Sc->LastChunk = Brick >> Log2Ceil(Idx2->BricksPerChunk[E->Iter]);
  }
  /* write the min emax */
  GrowToAccomodate(&Sc->BlockEMaxesStream, 2 * Size(E->EMaxes));
  idx2_For (int, I, 0, Size(E->EMaxes))
  {
    i16 S = E->EMaxes[I] + (SizeOf(Idx2->DType) > 4 ? traits<f64>::ExpBias : traits<f32>::ExpBias);
    Write(&Sc->BlockEMaxesStream, S, SizeOf(Idx2->DType) > 4 ? 16 : traits<f32>::ExpBits);
  }
  /* write brick emax size */
  i64 BrickEMaxesSz = Size(Sc->BlockEMaxesStream);
  GrowToAccomodate(&Sc->BrickEMaxesStream, BrickEMaxesSz);
  WriteStream(&Sc->BrickEMaxesStream, &Sc->BlockEMaxesStream);
  //BlockEMaxStat.Add((f64)Size(Sc->BlockEMaxesStream));
  Rewind(&Sc->BlockEMaxesStream);
  Sc->LastBrick = Brick;

  /* pass 2: encode the brick meta info */
  idx2_InclusiveFor (u32, Block, 0, LastBlock)
  {
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2->BlockDims3;
    v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
    bool CodedInNextIter =
      E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 == Idx2->BlockDims3;
    if (CodedInNextIter)
      continue;
    /* done at most once per brick */
    idx2_For (int, I, 0, Size(E->BlockSigs))
    { // bit plane loop
      i16 RealBp = E->BlockSigs[I].BitPlane;
      if (Block != E->BlockSigs[I].Block)
        continue;
      u32 ChannelKey = GetChannelKey(RealBp, E->Iter, E->Level);
      auto ChannelIt = Lookup(&E->Channels, ChannelKey);
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
      GrowToAccomodate(&C->BrickSzsStream, 4);
      WriteVarByte(&C->BrickSzsStream, BrickSize);
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
EncodeBrick(idx2_file* Idx2, const params& P, encode_data* E, bool IncIter = false)
{
  idx2_Assert(Idx2->NLevels <= idx2_file::MaxLevels);

  i8 Iter = E->Iter += IncIter;

  u64 Brick = E->Brick[Iter];
  //printf(
  //  "level %d brick " idx2_PrStrV3i " %" PRIu64 "\n", Iter, idx2_PrV3i(E->Bricks3[Iter]), Brick);
  auto BIt = Lookup(&E->BrickPool, GetBrickKey(Iter, Brick));
  idx2_Assert(BIt);
  volume& BVol = BIt.Val->Vol;
  idx2_Assert(BVol.Buffer);

  // TODO: we do not need to pre-extrapolate
  ExtrapolateCdf53(Dims(BIt.Val->ExtentLocal), Idx2->TransformOrder, &BVol);

  /* do wavelet transform */
  if (!P.WaveletOnly)
  {
    if (Iter + 1 < Idx2->NLevels)
      ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, false);
    else
      ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, true);
  }
  else
  {
    ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, false);
  }

  /* recursively encode the brick, one subband at a time */
  idx2_For (i8, Sb, 0, Size(Idx2->Subbands))
  { // subband loop
    const subband& S = Idx2->Subbands[Sb];
    v3i SbDimsNonExt3 = idx2_NonExtDims(Dims(S.Grid));
    i8 NextIter = Iter + 1;
    if (Sb == 0 && NextIter < Idx2->NLevels)
    { // need to encode the parent brick
      /* find the parent brick and create it if not found */
      v3i Brick3 = E->Bricks3[Iter];
      v3i PBrick3 = (E->Bricks3[NextIter] = Brick3 / Idx2->GroupBrick3);
      u64 PBrick = (E->Brick[NextIter] = GetLinearBrick(*Idx2, NextIter, PBrick3));
      u64 PKey = GetBrickKey(NextIter, PBrick);
      auto PbIt = Lookup(&E->BrickPool, PKey);
      if (!PbIt)
      { // instantiate the parent brick in the hash table
        brick_volume PBrickVol;
        Resize(&PBrickVol.Vol, Idx2->BrickDimsExt3, dtype::float64, E->Alloc);
        Fill(idx2_Range(f64, PBrickVol.Vol), 0.0);
        v3i From3 = (Brick3 / Idx2->GroupBrick3) * Idx2->GroupBrick3;
        v3i NChildren3 =
          Dims(Crop(extent(From3, Idx2->GroupBrick3), extent(Idx2->NBricks3[Iter])));
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
      bool LastChild = ++PbIt.Val->NChildren == PbIt.Val->NChildrenMax;
      if (LastChild)
        EncodeBrick(Idx2, P, E, true);
    } // end Sb == 0 && NextIteration < Idx2->NLevels
    E->Level = Sb;
    if (Idx2->Version == v2i(1, 0))
      EncodeSubband(Idx2, E, S.Grid, &BVol);
  } // end subband loop
  Dealloc(&BVol);
  Delete(&E->BrickPool, GetBrickKey(Iter, Brick));
  E->Iter -= IncIter;
}


// static void
// EncodeBrickNASAWithMask
//( /*----------------------------*/
//   idx2_file*    Idx2           ,
//   const params& P              ,
//   encode_data*  E              ,
//   bool          IncIter = false
//) /*----------------------------*/
//{
//   idx2_Assert(Idx2->NLevels <= idx2_file::MaxLevels);
//
//   i8 Iter = E->Iter += IncIter;
//   bool Valid = true;
//   if (Iter == 0 && P.NasaMask.Buffer)
//     Valid = P.NasaMask.At<int8>(v3i(E->Bricks3[Iter].X, E->Bricks3[Iter].Y, 0));
//
//   u64 Brick = E->Brick[Iter];
//   printf("level %d brick " idx2_PrStrV3i " %" PRIu64 "\n", Iter, idx2_PrV3i(E->Bricks3[Iter]),
//   Brick); auto BIt = Lookup(&E->BrickPool, GetBrickKey(Iter, Brick)); idx2_Assert(BIt); volume&
//   BVol = BIt.Val->Vol; idx2_Assert(BVol.Buffer);
//
//   if (Valid) { /* extrapolate the brick to, say 65^3 */
//     // TODO: we do not need to pre-extrapolate
//     ExtrapolateCdf53(Dims(BIt.Val->ExtentLocal), Idx2->TformOrder, &BVol);
//   }
//
//   if (Valid) { /* do wavelet transform */
//     if (!P.WaveletOnly) {
//       if (Iter + 1 < Idx2->NLevels)
//         ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, false);
//       else
//         ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, true);
//     } else {
//       ForwardCdf53(Idx2->BrickDimsExt3, E->Iter, Idx2->Subbands, Idx2->Td, &BVol, false);
//     }
//   }
//
//   if (Valid) { /* compute the min-max tree if needed */
//     if (P.ComputeMinMax) {
////      v3i NBlocks3 = (BrickDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
////      //u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
////      idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
////        f64 BlockMin =
////          v3i Z3(DecodeMorton3(Block));
////        idx2_NextMorton(Block, Z3, NBlocks3);
////        v3i D3 = Z3 * Idx2->BlockDims3;
////        v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
////        BrickVol->At<f64>(From3, Strd3, D3 + S3);
////      }
//    }
//  }
//  /* recursively encode the brick, one subband at a time */
//  idx2_For(i8, Sb, 0, Size(Idx2->Subbands)) { // subband loop
//    const subband& S = Idx2->Subbands[Sb];
//    v3i SbDimsNonExt3 = idx2_NonExtDims(Dims(S.Grid));
//    i8 NextIter = Iter + 1;
//    if (Sb == 0 && NextIter < Idx2->NLevels) { // need to encode the parent brick
//      /* find the parent brick and create it if not found */
//      v3i Brick3 = E->Bricks3[Iter];
//      v3i PBrick3 = (E->Bricks3[NextIter] = Brick3 / Idx2->GroupBrick3);
//      u64 PBrick = (E->Brick[NextIter] = GetLinearBrick(*Idx2, NextIter, PBrick3));
//      u64 PKey = GetBrickKey(NextIter, PBrick);
//      auto PbIt = Lookup(&E->BrickPool, PKey);
//      if (!PbIt) { // instantiate the parent brick in the hash table
//        brick_volume PBrickVol;
//        if (Valid) {
//          PBrickVol.AnyChild = true;
//        } else {
//          ++PbIt.Val->NChildren;
//          goto EXIT;
//        }
//
//        Resize(&PBrickVol.Vol, Idx2->BrickDimsExt3, dtype::float64, E->Alloc);
//        Fill(idx2_Range(f64, PBrickVol.Vol), 0.0);
//        v3i From3 = (Brick3 / Idx2->GroupBrick3) * Idx2->GroupBrick3;
//        v3i NChildren3 = Dims(Crop(extent(From3, Idx2->GroupBrick3),
//        extent(Idx2->NBricks3s[Iter]))); PBrickVol.NChildrenMax = (i8)Prod(NChildren3);
//        PBrickVol.ExtentLocal = extent(NChildren3 * SbDimsNonExt3);
//        Insert(&PbIt, PKey, PBrickVol);
//      }
//      /* copy data to the parent brick and (optionally) encode it */
//      v3i LocalBrickPos3 = Brick3 % Idx2->GroupBrick3;
//      grid SbGridNonExt = S.Grid; SetDims(&SbGridNonExt, SbDimsNonExt3);
//      extent ToGrid(LocalBrickPos3 * SbDimsNonExt3, SbDimsNonExt3);
//      CopyGridExtent<f64, f64>(SbGridNonExt, BVol, ToGrid, &PbIt.Val->Vol);
////      Copy(SbGridNonExt, BVol, ToGrid, &PbIt.Val->Vol);
//      bool LastChild = ++PbIt.Val->NChildren == PbIt.Val->NChildrenMax;
//      if (LastChild) EncodeBrick(Idx2, P, E, true);
//    } // end Sb == 0 && NextIteration < Idx2->NLevels
//    E->Level = Sb;
//    if (Idx2->Version == v2i(1, 0))
//      EncodeSubband(Idx2, E, S.Grid, &BVol);
//  } // end subband loop
// EXIT:
//  Dealloc(&BVol);
//  Delete(&E->BrickPool, GetBrickKey(Iter, Brick));
//  E->Iter -= IncIter;
//}


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


f64 TotalTime_ = 0;


/*
By default, copy brick data from a volume to a local brick buffer.
Can be extended polymorphically to provide other ways of copying.
*/
brick_copier::brick_copier(const volume* InputVolume)
{
  Volume = InputVolume;
}


v2d
brick_copier::Copy(const extent& ExtentGlobal, const extent& ExtentLocal, brick_volume* Brick)
{
  v2d MinMax;
  if (Volume->Type == dtype::float32)
    MinMax = (CopyExtentExtentMinMax<f32, f64>(ExtentGlobal, *Volume, ExtentLocal, &Brick->Vol));
  else if (Volume->Type == dtype::float64)
    MinMax = (CopyExtentExtentMinMax<f64, f64>(ExtentGlobal, *Volume, ExtentLocal, &Brick->Vol));

  return MinMax;
}


error<idx2_err_code>
Encode(idx2_file* Idx2, const params& P, brick_copier& Copier)
{
  const int BrickBytes = Prod(Idx2->BrickDimsExt3) * sizeof(f64);
  BrickAlloc_ = free_list_allocator(BrickBytes);
  idx2_RAII(encode_data, E, Init(&E));
  idx2_BrickTraverse(
    timer Timer; StartTimer(&Timer);
    //    idx2_Assert(GetLinearBrick(*Idx2, 0, Top.BrickFrom3) == Top.Address);
    //    idx2_Assert(GetSpatialBrick(*Idx2, 0, Top.Address) == Top.BrickFrom3);
    // BVol = local brick storage (we will copy brick data from the input to this)
    brick_volume BVol;
    Resize(&BVol.Vol, Idx2->BrickDimsExt3, dtype::float64, E.Alloc);
    Fill(idx2_Range(f64, BVol.Vol), 0.0);
    extent BrickExtent(Top.BrickFrom3 * Idx2->BrickDims3, Idx2->BrickDims3);
    // BrickExtentCrop = the true extent of the brick (boundary bricks are cropped)
    extent BrickExtentCrop = Crop(BrickExtent, extent(Idx2->Dims3));
    BVol.ExtentLocal = Relative(BrickExtentCrop, BrickExtent);
    v2d MinMax = Copier.Copy(BrickExtentCrop, BVol.ExtentLocal, &BVol);
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
    ,
    128,
    Idx2->BricksOrder[E.Iter],
    v3i(0),
    Idx2->NBricks3[E.Iter],
    extent(Idx2->NBricks3[E.Iter]),
    extent(Idx2->NBricks3[E.Iter])
  );

  /* dump the bit streams to files */
  timer Timer;
  StartTimer(&Timer);
  idx2_PropagateIfError(FlushChunks(*Idx2, &E));
  idx2_PropagateIfError(FlushChunkExponents(*Idx2, &E));
  timer RdoTimer;
  StartTimer(&RdoTimer);
  RateDistortionOpt(*Idx2, &E);
  TotalTime_ += Seconds(ElapsedTime(&Timer));
  printf("rdo time                = %f\n", Seconds(ElapsedTime(&RdoTimer)));

  WriteMetaFile(*Idx2, P, idx2_PrintScratch("%s/%s/%s.idx2", P.OutDir, P.Meta.Name, P.Meta.Field));
  printf("num channels            = %" PRIi64 "\n", Size(E.Channels));
  printf("num sub channels        = %" PRIi64 "\n", Size(E.SubChannels));
  PrintStats();
  printf("total time              = %f seconds\n", TotalTime_);
  //  _ASSERTE( _CrtCheckMemory( ) );
  return idx2_Error(idx2_err_code::NoError);
}


error<idx2_err_code>
EncodeBrick(idx2_file* Idx2, const params& P, const v3i& BrickPos3)
{
  // TODO: First, we copy the brick to a buffer backed by memory-mapped file
  // TODO: Then, if this brick
  return idx2_Error(idx2_err_code::NoError);
}

// TODO: make sure the wavelet normalization works across levels
// TODO: progressive decoding (maintaning a buffer (something like a FIFO queue) for the bricks)
// TODO: add a mode that treats the chunks like a row in a table


static void
Init(encode_data* E, allocator* Alloc)
{
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
Dealloc(encode_data* E)
{
  E->Alloc->DeallocAll();
  Dealloc(&E->BrickPool);
  idx2_ForEach (ChannelIt, E->Channels)
    Dealloc(ChannelIt.Val);
  Dealloc(&E->Channels);
  idx2_ForEach (SubChannelIt, E->SubChannels)
    Dealloc(SubChannelIt.Val);
  Dealloc(&E->SubChannels);
  idx2_ForEach (ChunkMetaIt, E->ChunkMeta)
    Dealloc(ChunkMetaIt.Val);
  idx2_ForEach (ChunkEMaxesMetaIt, E->ChunkEMaxesMeta)
    Dealloc(ChunkEMaxesMetaIt.Val);
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


/* ----------- UNUSED: VERSION 0 ----------*/

/* V0_0
- only do wavelet transform (no compression)
- write each iteration to one file
- only support linear decoding of each file */
// void
// EncodeSubbandV0_0(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol) {
//   u64 Brick = E->Brick[E->Iter];
//   v3i SbDims3 = Dims(SbGrid);
//   v3i NBlocks3 = (SbDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
//   u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
//   file_id FileId = ConstructFilePathV0_0(*Idx2, Brick, E->Iter, 0, 0);
//   idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
//   idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
//     v3i Z3(DecodeMorton3(Block));
//     idx2_NextMorton(Block, Z3, NBlocks3);
//     f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(Idx2->BlockDims3));
//     v3i D3 = Z3 * Idx2->BlockDims3;
//     v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
//     bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 ==
//     Idx2->BlockDims3; if (CodedInNextIter) continue;
//     /* copy the samples to the local buffer */
//     v3i S3;
//     idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
//       idx2_Assert(D3 + S3 < SbDims3);
//       BlockFloats[Row(BlockDims3, S3)] = BrickVol->At<f64>(SbGrid, D3 + S3);
//     } idx2_EndFor3 // end sample loop
//       WriteBuffer(Fp, BufFloats);
//   }
// }


/* V0_1:
- do wavelet transform and compression
- write each iteration to one file
- only support linear decoding of each file */
// void
// EncodeSubbandV0_1(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol) {
//   u64 Brick = E->Brick[E->Iter];
//   v3i SbDims3 = Dims(SbGrid);
//   const i8 NBitPlanes = idx2_BitSizeOf(f64);
//   v3i NBlocks3 = (SbDims3 + Idx2->BlockDims3 - 1) / Idx2->BlockDims3;
//   u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
//   file_id FileId = ConstructFilePathV0_0(*Idx2, Brick, E->Iter, 0, 0);
//   idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
//   Rewind(&E->BlockStream);
//   GrowToAccomodate(&E->BlockStream, 8 * 1024 * 1024); // 8MB
//   InitWrite(&E->BlockStream, E->BlockStream.Stream);
//   idx2_InclusiveFor(u32, Block, 0, LastBlock) { // zfp block loop
//     v3i Z3(DecodeMorton3(Block));
//     idx2_NextMorton(Block, Z3, NBlocks3);
//     v3i D3 = Z3 * Idx2->BlockDims3;
//     v3i BlockDims3 = Min(Idx2->BlockDims3, SbDims3 - D3);
//     f64 BlockFloats[4 * 4 * 4] = {}; buffer_t BufFloats(BlockFloats, Prod(BlockDims3));
//     i64 BlockInts[4 * 4 * 4] = {}; buffer_t BufInts(BlockInts, Prod(BlockDims3));
//     u64 BlockUInts[4 * 4 * 4] = {}; buffer_t BufUInts(BlockUInts, Prod(BlockDims3));
//     bool CodedInNextIter = E->Level == 0 && E->Iter + 1 < Idx2->NLevels && BlockDims3 ==
//     Idx2->BlockDims3; if (CodedInNextIter) continue;
//     /* copy the samples to the local buffer */
//     v3i S3;
//     int J = 0;
//     idx2_BeginFor3(S3, v3i(0), BlockDims3, v3i(1)) { // sample loop
//       idx2_Assert(D3 + S3 < SbDims3);
//       BlockFloats[J++] = BrickVol->At<f64>(SbGrid, D3 + S3);
//     } idx2_EndFor3 // end sample loop
//       i8 NDims = (i8)NumDims(BlockDims3);
//     const int NVals = 1 << (2 * NDims);
//     const i8 Prec = idx2_BitSizeOf(f64) - 1 - NDims;
//     // TODO: deal with Float32
//     const i16 EMax = (i16)Quantize(Prec, BufFloats, &BufInts);
//     ForwardZfp(BlockInts, NDims);
//     ForwardShuffle(BlockInts, BlockUInts, NDims);
//     i8 N = 0; // number of significant coefficients in the block so far
//     Write(&E->BlockStream, EMax + traits<f64>::ExpBias, traits<f64>::ExpBits);
//     idx2_InclusiveForBackward(i8, Bp, NBitPlanes - 1, 0) { // bit plane loop
//       i16 RealBp = Bp + EMax;
//       bool TooHighPrecision = NBitPlanes - 6 > RealBp - Exponent(Idx2->Accuracy) + 1;
//       if (TooHighPrecision) break;
//       GrowIfTooFull(&E->BlockStream);
//       Encode(BlockUInts, NVals, Bp, N, &E->BlockStream);
//     }
//   }
//   Flush(&E->BlockStream);
//   WritePOD(Fp, (int)Size(E->BlockStream));
//   WriteBuffer(Fp, ToBuffer(E->BlockStream));
// }


} // namespace idx2
