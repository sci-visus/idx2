#include "idx2Decode.h"
#include "Array.h"
#include "BitStream.h"
#include "Expected.h"
#include "Function.h"
#include "HashTable.h"
#include "Memory.h"
#include "Timer.h"
#include "VarInt.h"
#include "Zfp.h"
#include "idx2Lookup.h"
#include "idx2Read.h"
#include "sexpr.h"
#include "zstd/zstd.h"


namespace idx2
{


static void
Init(decode_data* D, allocator* Alloc = nullptr)
{
  Init(&D->BrickPool, 5);
  D->Alloc = Alloc ? Alloc : &BrickAlloc_;
  Init(&D->FileCacheTable);
  Init(&D->Streams, 7);
}


static void
Dealloc(decode_data* D)
{
  D->Alloc->DeallocAll();
  idx2_ForEach (BrickVolIt, D->BrickPool)
    Dealloc(&BrickVolIt.Val->Vol);
  Dealloc(&D->BrickPool);
  DeallocFileCacheTable(&D->FileCacheTable);
  Dealloc(&D->BlockStream);
  Dealloc(&D->Streams);
  DeallocBuf(&D->CompressedChunkExps);
  Dealloc(&D->ChunkExpSizeStream);
  Dealloc(&D->ChunkAddrsStream);
  Dealloc(&D->ChunkSizeStream);
}


error<idx2_err_code>
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf);

static error<idx2_err_code>
DecodeSubband(const idx2_file& Idx2,
              decode_data* D,
              f64 Accuracy,
              const grid& SbGrid,
              volume* BVol);

static error<idx2_err_code>
DecodeBrick(const idx2_file& Idx2, const params& P, decode_data* D, u8 Mask, f64 Accuracy);


void
DecompressBufZstd(const buffer& Input, bitstream* Output)
{
  unsigned long long const OutputSize = ZSTD_getFrameContentSize(Input.Data, Size(Input));
  GrowToAccomodate(Output, OutputSize - Size(*Output));
  size_t const Result = ZSTD_decompress(Output->Stream.Data, OutputSize, Input.Data, Size(Input));
  if (Result != OutputSize)
  {
    fprintf(stderr, "Zstd decompression failed\n");
    exit(1);
  }
}


/* decode the subband of a brick */
// TODO: we can detect the precision and switch to the avx2 version that uses float for better
// performance
// TODO: if a block does not decode any bit plane, no need to copy data afterwards
static error<idx2_err_code>
DecodeSubband(const idx2_file& Idx2, decode_data* D, f64 Accuracy, const grid& SbGrid, volume* BVol)
{
  u64 Brick = D->Brick[D->Level];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Idx2.BlockDims3 - 1) / Idx2.BlockDims3;
  int BlockCount = Prod(NBlocks3);
  if (D->Subband == 0 && D->Level + 1 < Idx2.NLevels)
    BlockCount -= Prod(SbDims3 / Idx2.BlockDims3);

  /* first, read the block exponents */
  auto ReadChunkExpResult = ReadChunkExponents(Idx2, D, Brick, D->Level, D->Subband);
  if (!ReadChunkExpResult)
    return Error(ReadChunkExpResult);

  const chunk_exp_cache* ChunkExpCache = Value(ReadChunkExpResult);
  i32 BrickExpOffset = (D->BrickInChunk * BlockCount) * (SizeOf(Idx2.DType) > 4 ? 2 : 1);
  bitstream BrickExpsStream = ChunkExpCache->ChunkExpStream;
  SeekToByte(&BrickExpsStream, BrickExpOffset);
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  const i8 NBitPlanes = idx2_BitSizeOf(u64);
  /* gather the streams (for the different bit planes) */
  auto& Streams = D->Streams;
  Clear(&Streams);
  idx2_InclusiveFor (u32, Block, 0, LastBlock)
  { // zfp block loop
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
    u64 BlockUInts[4 * 4 * 4] = {};
    buffer_t BufUInts(BlockUInts, Prod(BlockDims3));
    bool CodedInNextIter =
      D->Subband == 0 && D->Level + 1 < Idx2.NLevels && BlockDims3 == Idx2.BlockDims3;
    if (CodedInNextIter)
      continue; // CodedInNextIter j/ust means that this block belongs to the LLL subband?
    // we read the exponent for the block
    i16 EMax = SizeOf(Idx2.DType) > 4
                 ? (i16)Read(&BrickExpsStream, 16) - traits<f64>::ExpBias
                 : (i16)Read(&BrickExpsStream, traits<f32>::ExpBits) - traits<f32>::ExpBias;
    i8 N = 0;
    i8 EndBitPlane = Min(i8(BitSizeOf(Idx2.DType)), NBitPlanes);
    int NBitPlanesDecoded = Exponent(Accuracy) - 6 - EMax + 1;
    i8 NBps = 0;
    idx2_InclusiveForBackward (i8, Bp, NBitPlanes - 1, NBitPlanes - EndBitPlane)
    { // bit plane loop
      i16 RealBp = Bp + EMax;
      if (NBitPlanes - 6 > RealBp - Exponent(Accuracy) + 1)
        break; // this bit plane is not needed to satisfy the input accuracy
      auto StreamIt = Lookup(&Streams, RealBp);
      bitstream* Stream = nullptr;
      if (!StreamIt)
      { // first block in the brick
        auto ReadChunkResult = ReadChunk(Idx2, D, Brick, D->Level, D->Subband, RealBp);
        if (!ReadChunkResult)
          return Error(ReadChunkResult);

        const chunk_cache* ChunkCache = Value(ReadChunkResult);
        auto BrickIt = BinarySearch(idx2_Range(ChunkCache->Bricks), Brick);
        idx2_Assert(BrickIt != End(ChunkCache->Bricks));
        idx2_Assert(*BrickIt == Brick);
        i64 BrickInChunk = BrickIt - Begin(ChunkCache->Bricks);
        idx2_Assert(BrickInChunk < Size(ChunkCache->BrickSizes));
        i64 BrickOffset = BrickInChunk == 0 ? 0 : ChunkCache->BrickSizes[BrickInChunk - 1];
        BrickOffset += Size(ChunkCache->ChunkStream);
        Insert(&StreamIt, RealBp, ChunkCache->ChunkStream);
        Stream = StreamIt.Val;
        SeekToByte(Stream,
                   BrickOffset); // seek to the correct byte offset of the brick in the chunk
      }
      else
      {
        Stream = StreamIt.Val;
      }
      /* zfp decode */
      ++NBps;
      //      timer Timer; StartTimer(&Timer);
      auto SizeBegin = BitSize(*Stream);
      if (NBitPlanesDecoded <= 8)
        Decode(BlockUInts, NVals, Bp, N, Stream); // use AVX2
      else
        DecodeTest(&BlockUInts[NBitPlanes - 1 - Bp],
                   NVals,
                   N,
                   Stream); // delay the transpose of bits to later
                            //      DecodeTime_ += Seconds(ElapsedTime(&Timer));
      auto SizeEnd = BitSize(*Stream);
      D->BytesDecoded_ += SizeEnd - SizeBegin;
    }                       // end bit plane loop

    if (NBitPlanesDecoded > 8)
    {
      //      timer Timer; StartTimer(&Timer);
      TransposeRecursive(BlockUInts, NBps); // transpose using the recursive algorithm
                                            //      DecodeTime_ += Seconds(ElapsedTime(&Timer));
    }
    /* do inverse zfp transform but only if any bit plane is decoded */
    if (NBps > 0)
    {
      InverseShuffle(BlockUInts, (i64*)BlockFloats, NDims);
      InverseZfp((i64*)BlockFloats, NDims);
      Dequantize(EMax, Prec, BufInts, &BufFloats);
      v3i S3;
      int J = 0;
      v3i From3 = From(SbGrid), Strd3 = Strd(SbGrid);
      timer DataTimer;
      StartTimer(&DataTimer);
      idx2_BeginFor3 (S3, v3i(0), BlockDims3, v3i(1))
      { // sample loop
        idx2_Assert(D3 + S3 < SbDims3);
        BVol->At<f64>(From3, Strd3, D3 + S3) = BlockFloats[J++];
      }
      idx2_EndFor3; // end sample loop
      D->DataMovementTime_ += ElapsedTime(&DataTimer);
    }
  }

  return idx2_Error(idx2_err_code::NoError);
}


static error<idx2_err_code>
DecodeBrick(const idx2_file& Idx2, const params& P, decode_data* D, f64 Accuracy)
{
  i8 Level = D->Level;
  u64 Brick = D->Brick[Level];
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
  auto BrickIt = Lookup(&D->BrickPool, GetBrickKey(Level, Brick));
  idx2_Assert(BrickIt);
  volume& BVol = BrickIt.Val->Vol;

  /* construct a list of subbands to decode */
  idx2_Assert(Size(Idx2.Subbands) <= 8);
  //idx2_For (u8, Sb, 0, 8)
  //{
  //  if (!BitSet(Mask, Sb))
  //    continue;
  //  idx2_For (u8, S, 0, 8)
  //    if ((Sb | S) <= Sb)
  //      DecodeSbMask = SetBit(DecodeSbMask, S);
  //} // end subband loop

  /* recursively decode the brick, one subband at a time */
  idx2_For (i8, Sb, 0, (i8)Size(Idx2.Subbands))
  {
    if (!BitSet(Idx2.DecodeSubbandMasks[Level], Sb))
      continue;
    const subband& S = Idx2.Subbands[Sb];
    v3i SbDimsNonExt3 = idx2_NonExtDims(Dims(S.Grid));
    i8 NextLevel = Level + 1;
    if (Sb == 0 && NextLevel < Idx2.NLevels)
    { // need to decode the parent brick first
      /* find and decode the parent */
      v3i Brick3 = D->Bricks3[D->Level];
      v3i PBrick3 = (D->Bricks3[NextLevel] = Brick3 / Idx2.GroupBrick3);
      u64 PBrick = (D->Brick[NextLevel] = GetLinearBrick(Idx2, NextLevel, PBrick3));
      u64 PKey = GetBrickKey(NextLevel, PBrick);
      auto PbIt = Lookup(&D->BrickPool, PKey);
      idx2_Assert(PbIt);
      // TODO: problem: here we will need access to D->LinearChunkInFile/D->LinearBrickInChunk for
      // the parent, which won't be computed correctly by the outside code, so for now we have to
      // stick to decoding from higher level down
      /* copy data from the parent's to my buffer */
      if (PbIt.Val->NChildren == 0)
      {
        v3i From3 = (Brick3 / Idx2.GroupBrick3) * Idx2.GroupBrick3;
        v3i NChildren3 = Dims(Crop(extent(From3, Idx2.GroupBrick3), extent(Idx2.NBricks3[Level])));
        PbIt.Val->NChildrenMax = (i8)Prod(NChildren3);
      }
      ++PbIt.Val->NChildren;
      v3i LocalBrickPos3 = Brick3 % Idx2.GroupBrick3;
      grid SbGridNonExt = S.Grid;
      SetDims(&SbGridNonExt, SbDimsNonExt3);
      extent ToGrid(LocalBrickPos3 * SbDimsNonExt3, SbDimsNonExt3);
      CopyExtentGrid<f64, f64>(ToGrid, PbIt.Val->Vol, SbGridNonExt, &BVol);
      if (PbIt.Val->NChildren == PbIt.Val->NChildrenMax)
      { // last child
        Dealloc(&PbIt.Val->Vol);
        Delete(&D->BrickPool, PKey);
      }
    }
    D->Subband = Sb;
    if (Sb == 0 || BitSet(Idx2.DecodeSubbandMasks[Level], Sb))
    { // NOTE: the check for Sb == 0 prevents the output volume from having blocking artifacts
      if (Idx2.Version == v2i(1, 0))
        idx2_PropagateIfError(DecodeSubband(Idx2, D, Accuracy, S.Grid, &BVol));
    }
  } // end subband loop
  // TODO: inverse transform only to the necessary level
  if (!P.WaveletOnly)
  {
    if (Level + 1 < Idx2.NLevels)
      InverseCdf53(Idx2.BrickDimsExt3, D->Level, Idx2.Subbands, Idx2.Td, &BVol, false);
    else
      InverseCdf53(Idx2.BrickDimsExt3, D->Level, Idx2.Subbands, Idx2.Td, &BVol, true);
  }

  return idx2_Error(err_code::NoError);
}


/* TODO: dealloc chunks after we are done with them */
error<idx2_err_code>
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf)
{
  timer DecodeTimer;
  StartTimer(&DecodeTimer);
  // TODO: we should add a --effective-mask
  grid OutGrid = GetGrid(Idx2, P.DecodeExtent);
  //printf("output grid = " idx2_PrStrGrid "\n", idx2_PrGrid(OutGrid));
  mmap_volume OutVol;
  volume OutVolMem;
  idx2_CleanUp(if (P.OutMode == params::out_mode::WriteToFile) { Unmap(&OutVol); });

  if (P.OutMode == params::out_mode::WriteToFile)
  {
    metadata Met;
    memcpy(Met.Name, Idx2.Name, sizeof(Met.Name));
    memcpy(Met.Field, Idx2.Field, sizeof(Met.Field));
    Met.Dims3 = Dims(OutGrid);
    Met.DType = Idx2.DType;
    //  printf("zfp decode time = %f\n", DecodeTime_);
    cstr OutFile = P.OutFile ? idx2_PrintScratch("%s/%s", P.OutDir, P.OutFile)
                             : idx2_PrintScratch("%s/%s-accuracy-%f.raw", P.OutDir, ToRawFileName(Met), P.DecodeAccuracy);
    //    idx2_RAII(mmap_volume, OutVol, (void)OutVol, Unmap(&OutVol));
    MapVolume(OutFile, Met.Dims3, Met.DType, &OutVol, map_mode::Write);
    printf("writing output volume to %s\n", OutFile);
  }
  else if (P.OutMode == params::out_mode::KeepInMemory)
  {
    OutVolMem.Buffer = *OutBuf;
    SetDims(&OutVolMem, Dims(OutGrid));
    OutVolMem.Type = Idx2.DType;
  }

  const int BrickBytes = Prod(Idx2.BrickDimsExt3) * sizeof(f64);
  BrickAlloc_ = free_list_allocator(BrickBytes);
  // TODO: move the decode_data into idx2_file itself
  //idx2_RAII(decode_data, D, Init(&D, &BrickAlloc_));
  idx2_RAII(decode_data, D, Init(&D, &Mallocator()));
  //  D.QualityLevel = Dw->GetQuality();
  f64 Accuracy = Max(Idx2.Accuracy, P.DecodeAccuracy);
  //  i64 CountZeroes = 0;

  idx2_InclusiveForBackward (i8, Level, Idx2.NLevels - 1, 0)
  {
    if (Idx2.DecodeSubbandMasks[Level] == 0)
      break;

    extent Ext = P.DecodeExtent;                  // this is in unit of samples
    v3i B3, Bf3, Bl3, C3, Cf3, Cl3, F3, Ff3, Fl3; // Brick dimensions, brick first, brick last
    B3 = Idx2.BrickDims3 * Pow(Idx2.GroupBrick3, Level);
    C3 = Idx2.BricksPerChunk3s[Level] * B3;
    F3 = C3 * Idx2.ChunksPerFile3s[Level];

    Bf3 = From(Ext) / B3;
    Bl3 = Last(Ext) / B3;
    Cf3 = From(Ext) / C3;
    Cl3 = Last(Ext) / C3;
    Ff3 = From(Ext) / F3;
    Fl3 = Last(Ext) / F3;

    extent ExtentInBricks(Bf3, Bl3 - Bf3 + 1);
    extent ExtentInChunks(Cf3, Cl3 - Cf3 + 1);
    extent ExtentInFiles(Ff3, Fl3 - Ff3 + 1);

    extent VolExt(Idx2.Dims3);
    v3i Vbf3, Vbl3, Vcf3, Vcl3, Vff3, Vfl3; // VolBrickFirst, VolBrickLast
    Vbf3 = From(VolExt) / B3;
    Vbl3 = Last(VolExt) / B3;
    Vcf3 = From(VolExt) / C3;
    Vcl3 = Last(VolExt) / C3;
    Vff3 = From(VolExt) / F3;
    Vfl3 = Last(VolExt) / F3;

    extent VolExtentInBricks(Vbf3, Vbl3 - Vbf3 + 1);
    extent VolExtentInChunks(Vcf3, Vcl3 - Vcf3 + 1);
    extent VolExtentInFiles(Vff3, Vfl3 - Vff3 + 1);

    idx2_FileTraverse(
      //      u64 FileAddr = FileTop.Address;
      //      idx2_Assert(FileAddr == GetLinearFile(Idx2, Level, FileTop.FileFrom3));
      idx2_ChunkTraverse(
        //        u64 ChunkAddr = (FileAddr * Idx2.ChunksPerFiles[Level]) + ChunkTop.Address;
        //        idx2_Assert(ChunkAddr == GetLinearChunk(Idx2, Level, ChunkTop.ChunkFrom3));
        D.ChunkInFile = ChunkTop.ChunkInFile;
        idx2_BrickTraverse(
          D.BrickInChunk = Top.BrickInChunk;
          //          u64 BrickAddr = (ChunkAddr * Idx2.BricksPerChunks[Level]) + Top.Address;
          //          idx2_Assert(BrickAddr == GetLinearBrick(Idx2, Level, Top.BrickFrom3));
          brick_volume BVol;
          Resize(&BVol.Vol, Idx2.BrickDimsExt3, dtype::float64, D.Alloc);
          // TODO: for progressive decompression, copy the data from BrickTable to BrickVol
          Fill(idx2_Range(f64, BVol.Vol), 0.0);
          D.Level = Level;
          D.Bricks3[Level] = Top.BrickFrom3;
          D.Brick[Level] = GetLinearBrick(Idx2, Level, Top.BrickFrom3);
          //printf("level = %d brick = %llu\n", Level, D.Brick[Level]);
          u64 BrickKey = GetBrickKey(Level, D.Brick[Level]);
          Insert(&D.BrickPool, BrickKey, BVol);
          idx2_PropagateIfError(DecodeBrick(Idx2, P, &D, Accuracy));
          // Copy the samples out to the output buffer (or file)
          if (Level == 0 || Idx2.DecodeSubbandMasks[Level - 1] == 0)
          {
            grid BrickGrid(
              Top.BrickFrom3 * B3,
              Idx2.BrickDims3,
              v3i(1 << Level)); // TODO: the 1 << level is only true for 1 transform pass per level
            grid OutBrickGrid = Crop(OutGrid, BrickGrid);
            grid BrickGridLocal = Relative(OutBrickGrid, BrickGrid);
            auto OutputVol = P.OutMode == params::out_mode::WriteToFile ? &OutVol.Vol : &OutVolMem;
            auto CopyFunc = OutputVol->Type == dtype::float32 ? (CopyGridGrid<f64, f32>)
                                                              : (CopyGridGrid<f64, f64>);
            CopyFunc(BrickGridLocal, BVol.Vol, Relative(OutBrickGrid, OutGrid), OutputVol);
            Dealloc(&BVol.Vol);
            Delete(&D.BrickPool, BrickKey); // TODO: also delete the parent bricks once we are done
          },
          64,
          Idx2.BricksOrderInChunk[Level],
          ChunkTop.ChunkFrom3 * Idx2.BricksPerChunk3s[Level],
          Idx2.BricksPerChunk3s[Level],
          ExtentInBricks,
          VolExtentInBricks);
        ,
        64,
        Idx2.ChunksOrderInFile[Level],
        FileTop.FileFrom3 * Idx2.ChunksPerFile3s[Level],
        Idx2.ChunksPerFile3s[Level],
        ExtentInChunks,
        VolExtentInChunks);
      , 64, Idx2.FilesOrder[Level], v3i(0), Idx2.NFiles3[Level], ExtentInFiles, VolExtentInFiles);
  } // end level loop
    //  printf("count zeroes        = %lld\n", CountZeroes);
  printf("total decode time   = %f\n", Seconds(ElapsedTime(&DecodeTimer)));
  printf("io time             = %f\n", Seconds(D.DecodeIOTime_));
  printf("data movement time  = %f\n", Seconds(D.DataMovementTime_));
  printf("exp   bytes read    = %" PRIi64 "\n", D.BytesExps_);
  printf("data  bytes read    = %" PRIi64 "\n", D.BytesData_);
  printf("total bytes read    = %" PRIi64 "\n", D.BytesExps_ + D.BytesData_);
  printf("total bytes decoded = %" PRIi64 "\n", D.BytesDecoded_ / 8);

  return idx2_Error(err_code::NoError);
}


void
DecompressChunk(bitstream* ChunkStream, chunk_cache* ChunkCache, u64 ChunkAddress, int L)
{
  (void)L;
  u64 Brk = ((ChunkAddress >> 18) & 0x3FFFFFFFFFFull);
  (void)Brk;
  InitRead(ChunkStream, ChunkStream->Stream);
  int NBricks = (int)ReadVarByte(ChunkStream);
  idx2_Assert(NBricks > 0);

  /* decompress and store the brick ids */
  u64 Brick = ReadVarByte(ChunkStream);
  Resize(&ChunkCache->Bricks, NBricks);
  ChunkCache->Bricks[0] = Brick;
  idx2_For (int, I, 1, NBricks)
  {
    Brick += ReadUnary(ChunkStream) + 1;
    ChunkCache->Bricks[I] = Brick;
    idx2_Assert(Brk == (Brick >> L));
  }
  Resize(&ChunkCache->BrickSizes, NBricks);

  /* decompress and store the brick sizes */
  i32 BrickSize = 0;
  SeekToNextByte(ChunkStream);
  idx2_ForEach (BrickSzIt, ChunkCache->BrickSizes)
    *BrickSzIt = BrickSize += (i32)ReadVarByte(ChunkStream);
  ChunkCache->ChunkStream = *ChunkStream;
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


/* ----------- VERSION 0: UNUSED ----------*/
/* NOTE: in v0.0, we only support reading the data from beginning to end on each iteration */
#if 0
error<idx2_err_code>
DecodeSubbandV0_0(const idx2_file& Idx2, decode_data* D, const grid& SbGrid, volume* BVol)
{
  u64 Brick = D->Brick[D->Level];
  v3i SbDims3 = Dims(SbGrid);
  v3i NBlocks3 = (SbDims3 + Idx2.BlockDims3 - 1) / Idx2.BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(Idx2, Brick, D->Level, 0, 0);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
  idx2_FSeek(Fp, D->Offsets[D->Level], SEEK_SET);
  /* first, read the block exponents */
  idx2_InclusiveFor (u32, Block, 0, LastBlock)
  { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    f64 BlockFloats[4 * 4 * 4] = {};
    buffer_t BufFloats(BlockFloats, Prod(Idx2.BlockDims3));
    v3i D3 = Z3 * Idx2.BlockDims3;
    v3i BlockDims3 = Min(Idx2.BlockDims3, SbDims3 - D3);
    bool CodedInNextIter =
      D->Subband == 0 && D->Level + 1 < Idx2.NLevels && BlockDims3 == Idx2.BlockDims3;
    if (CodedInNextIter)
      continue;
    ReadBuffer(Fp, &BufFloats);
    v3i S3;
    idx2_BeginFor3 (S3, v3i(0), BlockDims3, v3i(1))
    { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BVol->At<f64>(SbGrid, D3 + S3) = BlockFloats[Row(BlockDims3, S3)];
    }
    idx2_EndFor3; // end sample loop
  }
  D->Offsets[D->Level] = idx2_FTell(Fp);
  return idx2_Error(idx2_err_code::NoError);
}


error<idx2_err_code>
DecodeSubbandV0_1(const idx2_file& Idx2, decode_data* D, const grid& SbGrid, volume* BVol)
{
  u64 Brick = D->Brick[D->Level];
  v3i SbDims3 = Dims(SbGrid);
  const i8 NBitPlanes = idx2_BitSizeOf(f64);
  v3i NBlocks3 = (SbDims3 + Idx2.BlockDims3 - 1) / Idx2.BlockDims3;
  u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  file_id FileId = ConstructFilePathV0_0(Idx2, Brick, D->Level, 0, 0);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
  idx2_FSeek(Fp, D->Offsets[D->Level], SEEK_SET);
  int Sz = 0;
  ReadPOD(Fp, &Sz);
  Rewind(&D->BlockStream);
  GrowToAccomodate(&D->BlockStream, Max(Sz, 8 * 1024 * 1024));
  ReadBuffer(Fp, &D->BlockStream.Stream, Sz);
  InitRead(&D->BlockStream, D->BlockStream.Stream);
  /* first, read the block exponents */
  idx2_InclusiveFor (u32, Block, 0, LastBlock)
  { // zfp block loop
    v3i Z3(DecodeMorton3(Block));
    idx2_NextMorton(Block, Z3, NBlocks3);
    v3i D3 = Z3 * Idx2.BlockDims3;
    v3i BlockDims3 = Min(Idx2.BlockDims3, SbDims3 - D3);
    f64 BlockFloats[4 * 4 * 4] = {};
    buffer_t BufFloats(BlockFloats, Prod(BlockDims3));
    i64 BlockInts[4 * 4 * 4] = {};
    buffer_t BufInts(BlockInts, Prod(BlockDims3));
    u64 BlockUInts[4 * 4 * 4] = {};
    buffer_t BufUInts(BlockUInts, Prod(BlockDims3));
    bool CodedInNextIter =
      D->Subband == 0 && D->Level + 1 < Idx2.NLevels && BlockDims3 == Idx2.BlockDims3;
    if (CodedInNextIter)
      continue;
    int NDims = NumDims(BlockDims3);
    const int NVals = 1 << (2 * NDims);
    const int Prec = idx2_BitSizeOf(f64) - 1 - NDims;
    i16 EMax = i16(Read(&D->BlockStream, traits<f64>::ExpBits) - traits<f64>::ExpBias);
    i8 N = 0;
    idx2_InclusiveForBackward (i8, Bp, NBitPlanes - 1, 0)
    { // bit plane loop
      i16 RealBp = Bp + EMax;
      if (NBitPlanes - 6 > RealBp - Exponent(Idx2.Accuracy) + 1)
        break;
      Decode(BlockUInts, NVals, Bp, N, &D->BlockStream);
    }
    InverseShuffle(BlockUInts, BlockInts, NDims);
    InverseZfp(BlockInts, NDims);
    Dequantize(EMax, Prec, BufInts, &BufFloats);
    v3i S3;
    int J = 0;
    idx2_BeginFor3 (S3, v3i(0), BlockDims3, v3i(1))
    { // sample loop
      idx2_Assert(D3 + S3 < SbDims3);
      BVol->At<f64>(SbGrid, D3 + S3) = BlockFloats[J++];
    }
    idx2_EndFor3; // end sample loop
  }
  D->Offsets[D->Level] = idx2_FTell(Fp);

  return idx2_Error(idx2_err_code::NoError);
}
#endif

} // namespace idx2

