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
#include "idx2SparseBricks.h"
#include "sexpr.h"
#include "zstd/zstd.h"


namespace idx2
{


void
Init(decode_data* D, const idx2_file* Idx2, allocator* Alloc)
{
  Init(&D->BrickPool, Idx2);
  D->Alloc = Alloc ? Alloc : &BrickAlloc_;
  Init(&D->FileCacheTable);
#if VISUS_IDX2
  Init(&D->FileCache);
#endif
}


void
Dealloc(decode_data* D)
{
  D->Alloc->DeallocAll();
  Dealloc(&D->BrickPool);
  DeallocFileCacheTable(&D->FileCacheTable);
#if VISUS_IDX2
  Dealloc(&D->FileCache);
#endif
}


error<idx2_err_code>
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf);


// assume that Output has the right size already
// TODO: return an error
void
DecompressBufZstd(const buffer& Input, buffer* Output)
{
  unsigned long long const OutputSize = ZSTD_getFrameContentSize(Input.Data, Size(Input));
  idx2_Assert(Size(*Output) >= (i64)OutputSize);
  size_t const Result = ZSTD_decompress(Output->Data, OutputSize, Input.Data, Size(Input));
  if (Result != OutputSize)
  {
    fprintf(stderr, "Zstd decompression failed\n");
    exit(1);
  }
}


// TODO: return an error
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
static expected<bool, idx2_err_code>
DecodeSubband(const idx2_file& Idx2,
              decode_data* D,
              decode_state Ds,
              f64 Tolerance, // TODO: move to decode_state
              const grid& SbGrid, // TODO: move to decode_state
              hash_table<i16, bitstream> *StreamsPtr,
              brick_volume* BrickVol) // TODO: move to decode_states
{
  //u64 Brick = Ds.Brick;
  //v3i SbDims3 = Dims(SbGrid);
  //v3i NBlocks3 = (SbDims3 + Idx2.BlockDims3 - 1) / Idx2.BlockDims3;
  //int BlockCount = Prod(NBlocks3);
  //// the following subtracts blocks on subband 0 that are coded in the next level
  //// meaning, we only code subband-0 blocks that are on the boundary (extrapolated)
  //if (Ds.Subband == 0 && Ds.Level + 1 < Idx2.NLevels)
  //  BlockCount -= Prod(SbDims3 / Idx2.BlockDims3);

  ///* first, read the block exponents */
  //auto ReadChunkExpResult = ReadChunkExponents(Idx2, D, Brick, Ds.Level, Ds.Subband);
  //if (!ReadChunkExpResult)
  //  return Error(ReadChunkExpResult);

  //const chunk_exp_cache* ChunkExpCache = Value(ReadChunkExpResult);
  //i32 BrickExpOffset = (Ds.BrickInChunk * BlockCount) * (SizeOf(Idx2.DType) > 4 ? 2 : 1);
  //bitstream BrickExpsStream = ChunkExpCache->ChunkExpStream;
  //SeekToByte(&BrickExpsStream, BrickExpOffset);
  //u32 LastBlock = EncodeMorton3(v3<u32>(NBlocks3 - 1));
  //const i8 NBitPlanes = idx2_BitSizeOf(u64);
  //Clear(StreamsPtr);

  //bool SubbandSignificant = false; // whether there is any significant block on this subband
  //idx2_InclusiveFor (u32, Block, 0, LastBlock)
  //{ // zfp block loop
  //  v3i Z3(DecodeMorton3(Block));
  //  idx2_NextMorton(Block, Z3, NBlocks3);
  //  v3i D3 = Z3 * Idx2.BlockDims3;
  //  //if (Ds.Level ==0 && Ds.Subband == 0)
  //  //  printf("D3 " idx2_PrStrV3i " Spacing " idx2_PrStrV3i "\n", idx2_PrV3i(D3), idx2_PrV3i(Idx2.DecodeSubbandSpacings[Ds.Level][Ds.Subband]));
  //  bool BypassDecode = (D3 % Idx2.Subbands[Ds.Level].Spacings[Ds.Subband]) != 0;
  //  v3i BlockDims3 = Min(Idx2.BlockDims3, SbDims3 - D3);
  //  bool CodedInNextLevel =
  //    Ds.Subband == 0 && Ds.Level + 1 < Idx2.NLevels && BlockDims3 == Idx2.BlockDims3;
  //  if (CodedInNextLevel)
  //    continue;

  //  const int NDims = NumDims(BlockDims3);
  //  const int NVals = 1 << (2 * NDims);
  //  const int Prec = NBitPlanes - 1 - NDims;
  //  f64 BlockFloats[4 * 4 * 4];
  //  buffer_t BufFloats(BlockFloats, NVals);
  //  buffer_t BufInts((i64*)BlockFloats, NVals);
  //  u64 BlockUInts[4 * 4 * 4] = {};
  //  buffer_t BufUInts(BlockUInts, Prod(BlockDims3));

  //  // we read the exponent for the block
  //  i16 EMax = SizeOf(Idx2.DType) > 4
  //               ? (i16)Read(&BrickExpsStream, 16) - traits<f64>::ExpBias
  //               : (i16)Read(&BrickExpsStream, traits<f32>::ExpBits) - traits<f32>::ExpBias;
  //  i8 N = 0;
  //  i8 EndBitPlane = Min(i8(BitSizeOf(Idx2.DType)), NBitPlanes);
  //  int NBitPlanesDecoded = Exponent(Tolerance) - 6 - EMax + 1;
  //  i8 NBps = 0;
  //  int Bpc = Idx2.BitPlanesPerChunk;
  //  idx2_InclusiveForBackward (i8, Bp, NBitPlanes - 1, NBitPlanes - EndBitPlane)
  //  { // bit plane loop
  //    i16 RealBp = Bp + EMax;
  //    i16 BpKey = (RealBp + BitPlaneKeyBias_) / Bpc; // make it so that the BpKey is positive
  //    // TODO: always decode extra 6 bit planes?
  //    bool TooHighPrecision = NBitPlanes - 6 > RealBp - Exponent(Tolerance) + 1;
  //    if (TooHighPrecision)
  //    {
  //      if ((RealBp + BitPlaneKeyBias_) % Bpc == 0) // make sure we encode full "block" of BpKey
  //        break;
  //    }

  //    auto StreamIt = Lookup(*StreamsPtr, BpKey);
  //    bitstream* Stream = nullptr;
  //    if (!StreamIt)
  //    { // first block in the brick
  //      auto ReadChunkResult = ReadChunk(Idx2, D, Brick, Ds.Level, Ds.Subband, BpKey);
  //      if (!ReadChunkResult)
  //        //return Error(ReadChunkResult);
  //        break;

  //      const chunk_cache* ChunkCache = Value(ReadChunkResult);
  //      auto BrickIt = BinarySearch(idx2_Range(ChunkCache->Bricks), Brick);
  //      idx2_Assert(BrickIt != End(ChunkCache->Bricks));
  //      idx2_Assert(*BrickIt == Brick);
  //      i64 BrickInChunk = BrickIt - Begin(ChunkCache->Bricks);
  //      idx2_Assert(BrickInChunk < Size(ChunkCache->BrickOffsets));
  //      i64 BrickOffset = ChunkCache->BrickOffsets[BrickInChunk];
  //      // TODO: this addition is to bypass the part of the chunk stream that stores the brick offsets
  //      // but this is only correct if the first brick to decode is also first in the chunk stream
  //      //BrickOffset += Size(ChunkCache->ChunkStream);
  //      Insert(&StreamIt, BpKey, ChunkCache->ChunkStream);
  //      Stream = StreamIt.Val;
  //      // seek to the correct byte offset of the brick in the chunk
  //      //printf("stream end %llu  brick offset %llu\n", Size(Stream->Stream), BrickOffset);
  //      SeekToByte(Stream, BrickOffset);
  //    }
  //    else // if the stream already exists
  //    {
  //      Stream = StreamIt.Val;
  //    }
  //    /* zfp decode */
  //    if (!TooHighPrecision)
  //      ++NBps;
  //    auto SizeBegin = BitSize(*Stream);
  //    if (NBitPlanesDecoded <= 8)
  //      Decode(BlockUInts, NVals, Bp, N, Stream, BypassDecode); // use AVX2
  //    else // delay the transpose of bits to later
  //      DecodeTest(&BlockUInts[NBitPlanes - 1 - Bp], NVals, N, Stream);
  //    auto SizeEnd = BitSize(*Stream);
  //    D->BytesDecoded_ += SizeEnd - SizeBegin;
  //  } // end bit plane loop

  //  /* do inverse zfp transform but only if any bit plane is decoded */
  //  if (NBps > 0 && !BypassDecode)
  //  {
  //    if (NBitPlanesDecoded > 8)
  //      TransposeRecursive(BlockUInts, NBps);

  //    // if the subband is not 0 or if this is the last level, we count this block
  //    // as significant, otherwise it is not significant
  //    bool CurrBlockSignificant = (Ds.Subband > 0 || Ds.Level + 1 == Idx2.NLevels);
  //    SubbandSignificant = SubbandSignificant || CurrBlockSignificant;
  //    ++D->NSignificantBlocks;
  //    InverseShuffle(BlockUInts, (i64*)BlockFloats, NDims);
  //    InverseZfp((i64*)BlockFloats, NDims);
  //    Dequantize(EMax, Prec, BufInts, &BufFloats);
  //    v3i S3;
  //    int J = 0;
  //    v3i From3 = From(SbGrid), Spacing3 = Spacing(SbGrid);
  //    timer DataTimer;
  //    StartTimer(&DataTimer);
  //    volume& BVol = BrickVol->Vol;
  //    idx2_BeginFor3 (S3, v3i(0), BlockDims3, v3i(1))
  //    { // sample loop
  //      idx2_Assert(D3 + S3 < SbDims3);
  //      BVol.At<f64>(From3, Spacing3, D3 + S3) = BlockFloats[J++];
  //    }
  //    idx2_EndFor3; // end sample loop
  //    D->DataMovementTime_ += ElapsedTime(&DataTimer);
  //  }
  //}
  //D->NInsignificantSubbands += (SubbandSignificant == false);
  ////printf("%d\n", AnyBlockDecoded);

  //return SubbandSignificant;
  return false;
}


// TODO NEXT: check that this function is still correct (specifically the parts related to Brick3 and BrickInfo)
static error<idx2_err_code>
DecodeBrick(const idx2_file& Idx2, const params& P, decode_data* D, decode_state Ds, f64 Tolerance)
{
  //i8 Level = Ds.Level;
  //u64 Brick = Ds.Brick;
  ////  printf("level %d brick " idx2_PrStrV3i " %llu\n", Iter, idx2_PrV3i(D->Bricks3[Iter]), Brick);
  //auto BrickIt = Lookup(D->BrickPool.BrickTable, GetBrickKey(Level, Brick));
  //idx2_Assert(BrickIt);
  //volume& BVol = BrickIt.Val->Vol;

  //using stream_cache = hash_table<i16, bitstream>;
  //idx2_RAII(stream_cache, Streams, Init(&Streams, 7), Dealloc(&Streams));
  //idx2_For (i8, Sb, 0, (i8)Size(Idx2.Subbands[Level].PowOf2Plus1))
  //{
  //  if (!BitSet(Idx2.Subbands[Level].DecodeMasks, Sb))
  //    continue;

  //  const subband& SubbandPowerOfTwoPlusOne = Idx2.Subbands[Level].PowOf2Plus1[Sb];
  //  const subband& SubbandPowerOfTwo = Idx2.Subbands[Level].PowOf2Plus1[Sb];
  //  v3i SbDimsNonExt3 = Dims(SubbandPowerOfTwoPlusOne.LocalGrid);
  //  i8 NextLevel = Level + 1;

  //  /* we first copy data from the parent brick to the current brick if subband is 0 */
  //  if (Sb == 0 && NextLevel < Idx2.NLevels)
  //  { // need to decode the parent brick first
  //    /* find and decode the parent */
  //    v3i Brick3 = Ds.Brick3;
  //    //v3i PBrick3 = (D->Bricks3[NextLevel] = Brick3 / Idx2.GroupBrick3);
  //    //u64 PBrick = (D->Brick[NextLevel] = GetLinearBrick(Idx2, NextLevel, PBrick3));
  //    v3i PBrick3 = Brick3 / Idx2.BrickInfo[Level].Group;
  //    u64 PBrick = GetLinearBrick(Idx2, NextLevel, PBrick3);
  //    // TODO: move the following outside the subband loop
  //    u64 PKey = GetBrickKey(NextLevel, PBrick);
  //    auto PbIt = Lookup(D->BrickPool.BrickTable, PKey);
  //    idx2_Assert(PbIt);
  //    // TODO: problem: here we will need access to D->LinearChunkInFile/D->LinearBrickInChunk for
  //    // the parent, which won't be computed correctly by the outside code, so for now we have to
  //    // stick to decoding from higher level down
  //    /* copy data from the parent's to my buffer */
  //    //if (PbIt.Val->NChildrenDecoded == 0)
  //    //{
  //    //  v3i From3 = (Brick3 / Idx2.GroupBrick3) * Idx2.GroupBrick3;
  //    //  v3i NChildren3 = Dims(Crop(extent(From3, Idx2.GroupBrick3), extent(Idx2.NBricks3[Level])));
  //    //  PbIt.Val->NChildrenMax = (i8)Prod(NChildren3);
  //    //}
  //    //++PbIt.Val->NChildrenDecoded;
  //    v3i First3 = (Brick3 / Idx2.BrickInfo[Level].Group) * Idx2.BrickInfo[Level].Group;
  //    extent E = Crop(extent(First3, Idx2.BrickInfo[Level].Group), extent(Idx2.BrickInfo[Level].NBricks));
  //    v3i Last3 = Last(E);
  //    /* copy data from the parent to the current brick for subband 0 */
  //    v3i LocalBrickPos3 = Brick3 % Idx2.BrickInfo[Level].Group;
  //    grid SbGridNonExt = SubbandPowerOfTwoPlusOne.LocalGrid; // TODO NEXT: check this logic
  //    SetDims(&SbGridNonExt, SbDimsNonExt3);
  //    extent PGrid(LocalBrickPos3 * SbDimsNonExt3, SbDimsNonExt3); // parent grid
  //    CopyExtentGrid<f64, f64>(PGrid, PbIt.Val->Vol, SbGridNonExt, &BVol);
  //    if (Last3 == Brick3)
  //    { // last child
  //      bool DeleteBrick = true;
  //      if (P.OutMode == params::out_mode::HashMap)
  //        DeleteBrick = !PbIt.Val->Significant;
  //      if (DeleteBrick)
  //      {
  //        Dealloc(PbIt.Val);
  //        Delete(&D->BrickPool.BrickTable, PKey);
  //      }
  //    }
  //  }

  //  /* now we decode the subband */
  //  Ds.Subband = Sb;
  //  auto Result = DecodeSubband(Idx2, D, Ds, Tolerance, SubbandPowerOfTwoPlusOne.LocalGrid, &Streams, BrickIt.Val);
  //  if (Result)
  //    BrickIt.Val->Significant = BrickIt.Val->Significant || Value(Result);
  //  else
  //    return Error(Result);
  //} // end subband loop

  //bool CoarsestLevel = Level + 1 == Idx2.NLevels;
  //// TODO NEXT
  ////InverseCdf53(Idx2.BrickDimsExt3, Ds.Level, Idx2.Subbands, Idx2.TransformDetails, &BVol, CoarsestLevel);

  ////printf("%d\n", AnySubbandDecoded);
  return idx2_Error(idx2_err_code::NoError);
}


// TODO NEXT
error<idx2_err_code>
BrickDecodeTask(const idx2_file& Idx2, const extent& Extent, i8 Level, const traverse_item& BrickTop)
{
  ////          u64 BrickAddr = (ChunkAddr * Idx2.BricksPerChunks[Level]) + Top.Address;
  ////          idx2_Assert(BrickAddr == GetLinearBrick(Idx2, Level, Top.BrickFrom3));
  //Ds.BrickInChunk = Top.BrickInChunk;
  //brick_volume BVol;
  //Resize(&BVol.Vol, Idx2.BrickDimsExt3, dtype::float64, D.Alloc);
  //// TODO: for progressive decompression, copy the data from BrickTable to BrickVol
  //Fill(idx2_Range(f64, BVol.Vol), 0.0); // TODO: use memset
  //Ds.Level = Level;
  //Ds.Brick3 = Top.BrickFrom3;
  //Ds.Brick = GetLinearBrick(Idx2, Level, Top.BrickFrom3);
  ////printf("level = %d brick = %llu\n", Level, D.Brick[Level]);
  //u64 BrickKey = GetBrickKey(Level, Ds.Brick);
  //auto BrickIt = Insert(&D.BrickPool.BrickTable, BrickKey, BVol);
  //// TODO: pass the brick iterator into the DecodeBrick function to avoid one extra lookup
  ///* --------------- Decode the brick --------------- */
  //idx2_PropagateIfError(DecodeBrick(Idx2, P, &D, Ds, Tolerance));
  //// Copy the samples out to the output buffer (or file)
  //// The Idx2.DecodeSubbandMasks[Level - 1] == 0 means that no subbands on the next level
  //// will be decoded, so we can now just copy the result out
  ///* -------------------- Copy wavelet inverse transform samples to the output --------------- */
  //if (Level == 0 || Idx2.DecodeMasks[Level - 1] == 0)
  //{// TODO: the 1 << level is only true for 1 transform pass per level
  //  grid BrickGrid(Top.BrickFrom3 * B3, Idx2.BrickDims3, v3i(1 << Level));
  //  grid OutBrickGrid = Crop(OutGrid, BrickGrid);
  //  grid BrickGridLocal = Relative(OutBrickGrid, BrickGrid);
  //  if (P.OutMode == params::out_mode::RegularGridFile ||
  //      P.OutMode == params::out_mode::RegularGridMem)
  //  {
  //    auto OutputVol = P.OutMode == params::out_mode::RegularGridFile ? &OutVol.Vol : &OutVolMem;
  //    auto CopyFunc = OutputVol->Type == dtype::float32 ? (CopyGridGrid<f64, f32>)
  //                                                      : (CopyGridGrid<f64, f64>);
  //    CopyFunc(BrickGridLocal, BVol.Vol, Relative(OutBrickGrid, OutGrid), OutputVol);
  //    Dealloc(&BVol);
  //    Delete(&D.BrickPool.BrickTable, BrickKey);
  //  }
  //  else if (P.OutMode == params::out_mode::HashMap)
  //  {
  //    //printf("deleting\n");
  //    if (!BrickIt.Val->Significant)
  //    {
  //      Dealloc(&BVol);
  //      // TODO: can we delete straight from the iterator?
  //      Delete(&D.BrickPool.BrickTable, BrickKey);
  //    }
  //  }
  //}

  return idx2_Error(idx2_err_code::NoError)
}


/* TODO: dealloc chunks after we are done with them */
error<idx2_err_code>
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf)
{
  //timer DecodeTimer;
  //StartTimer(&DecodeTimer);
  //// TODO: we should add a --effective-mask
  //grid OutGrid = GetGrid(Idx2, P.DecodeExtent);
  //mmap_volume OutVol;
  //volume OutVolMem;
  //idx2_CleanUp(if (P.OutMode == params::out_mode::RegularGridFile) { Unmap(&OutVol); });

  //if (P.OutMode == params::out_mode::RegularGridFile)
  //{
  //  metadata Met;
  //  memcpy(Met.Name, Idx2.Name, sizeof(Met.Name));
  //  Met.Dims3 = Dims(OutGrid);
  //  Met.DType = Idx2.DType;
  //  //  printf("zfp decode time = %f\n", DecodeTime_);
  //  cstr OutFile = P.OutFile ? idx2_PrintScratch("%s/%s", P.OutDir, P.OutFile)
  //                           : idx2_PrintScratch("%s/%s-tolerance-%f.raw", P.OutDir, ToRawFileName(Met), P.DecodeTolerance);
  //  //    idx2_RAII(mmap_volume, OutVol, (void)OutVol, Unmap(&OutVol));
  //  MapVolume(OutFile, Met.Dims3, Met.DType, &OutVol, map_mode::Write);
  //  printf("writing output volume to %s\n", OutFile);
  //}
  //else if (P.OutMode == params::out_mode::RegularGridMem)
  //{
  //  OutVolMem.Buffer = *OutBuf;
  //  SetDims(&OutVolMem, Dims(OutGrid));
  //  OutVolMem.Type = Idx2.DType;
  //}

  //// TODO: make the free list allocator threadsafe
  ////BrickAlloc_ = free_list_allocator(BrickBytes);
  //// TODO: move the decode_data into idx2_file itself
  ////idx2_RAII(decode_data, D, Init(&D, &BrickAlloc_));
  //idx2_RAII(decode_data, D, Init(&D, &Idx2, &Mallocator())); // for now the allocator seems not a bottleneck
  ////  D.QualityLevel = Dw->GetQuality();
  //f64 Tolerance = Max(Idx2.Tolerance, P.DecodeTolerance);
  ////  i64 CountZeroes = 0;

  //decode_state Ds;
  //idx2_InclusiveForBackward (i8, Level, Idx2.NLevels - 1, 0)
  //{
  //  if (Idx2.Subbands[Level].DecodeMasks == 0)
  //    break;

  //  // TODO NEXT
  //} // end level loop

  //if (P.OutMode == params::out_mode::HashMap)
  //{
  //  PrintStatistics(&D.BrickPool);
  //  //ComputeBrickResolution(&D.BrickPool); // TODO NEXT
  //  WriteBricks(&D.BrickPool, "bricks");
  //}

  //printf("total decode time   = %f\n", Seconds(ElapsedTime(&DecodeTimer)));
  //printf("io time             = %f\n", Seconds(D.DecodeIOTime_.load()));
  //printf("data movement time  = %f\n", Seconds(D.DataMovementTime_.load()));
  //printf("exp   bytes read    = %" PRIi64 "\n", D.BytesExps_.load());
  //printf("data  bytes read    = %" PRIi64 "\n", D.BytesData_.load());
  //printf("total bytes read    = %" PRIi64 "\n", D.BytesExps_.load() + D.BytesData_.load());
  //printf("total bytes decoded = %" PRIi64 "\n", D.BytesDecoded_.load() / 8);
  //printf("final size of brick hashmap = %" PRIi64 "\n", Size(D.BrickPool.BrickTable));
  //printf("number of significant blocks = %" PRIi64 "\n", D.NSignificantBlocks.load());
  //printf("number of insignificant subbands = %" PRIi64 "\n", D.NInsignificantSubbands.load());

  return idx2_Error(err_code::NoError);
}

/*
* The structure of a chunk:
* A
* B
* C
* D
*
* A : varint = number of bricks in the chunk
* B : buffer = (unary encoded) delta stream that encodes which bricks are present
* C : buffer = size (in bytes) of each brick
* D : buffer = compressed data for each brick
*/
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

  Resize(&ChunkCache->BrickOffsets, NBricks);
  /* decompress and store the brick sizes */
  i32 BrickSize = 0;
  SeekToNextByte(ChunkStream);
  ChunkCache->BrickOffsets[0] = 0;
  idx2_For (int, I, 1, Size(ChunkCache->BrickOffsets))
  {
    ChunkCache->BrickOffsets[I] = (BrickSize += (i32)ReadVarByte(ChunkStream));
  }
  ReadVarByte(ChunkStream); // the size of the last brick (ignored here)
  idx2_ForEach (BrickSzIt, ChunkCache->BrickOffsets)
    *BrickSzIt += (i32)Size(*ChunkStream);
  ChunkCache->ChunkStream = *ChunkStream;
}


} // namespace idx2

