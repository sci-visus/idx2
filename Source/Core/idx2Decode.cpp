#include "idx2Decode.h"
#include "Array.h"
#include "BitStream.h"
#include "Expected.h"
#include "Function.h"
#include "HashTable.h"
#include "InputOutput.h"
#include "Memory.h"
#include "Timer.h"
#include "VarInt.h"
#include "Zfp.h"
#include "idx2Lookup.h"
#include "sexpr.h"
#include "zstd/zstd.h"


namespace idx2
{


void
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf);

static error<idx2_err_code>
ReadFileExponents(decode_data* D,
                  hash_table<u64, file_exp_cache>::iterator* FileExpCacheIt,
                  const file_id& FileId);

static expected<const chunk_exp_cache*, idx2_err_code>
ReadChunkExponents(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Level, i8 Subband);

static error<idx2_err_code>
ReadFileRdos(const idx2_file& Idx2,
             hash_table<u64, file_rdo_cache>::iterator* FileRdoCacheIt,
             const file_id& FileId);

static expected<const chunk_rdo_cache*, idx2_err_code>
ReadChunkRdos(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter);

static error<idx2_err_code>
ReadFile(decode_data* D, hash_table<u64, file_cache>::iterator* FileCacheIt, const file_id& FileId);

static expected<const chunk_cache*, idx2_err_code>
ReadChunk(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter, i8 Level, i16 BitPlane);

static error<idx2_err_code>
DecodeSubband(const idx2_file& Idx2,
              decode_data* D,
              f64 Accuracy,
              const grid& SbGrid,
              volume* BVol);

static void
DecodeBrick(const idx2_file& Idx2, const params& P, decode_data* D, u8 Mask, f64 Accuracy);

static void
DecompressChunk(bitstream* ChunkStream, chunk_cache* ChunkCache, u64 ChunkAddress, int L);


static idx2_Inline bool
IsEmpty(const chunk_exp_cache& ChunkExpCache)
{
  return Size(ChunkExpCache.BrickExpsStream.Stream) == 0;
}


static void
Dealloc(chunk_exp_cache* ChunkExpCache)
{
  Dealloc(&ChunkExpCache->BrickExpsStream);
}


static void
Dealloc(chunk_rdo_cache* ChunkRdoCache)
{
  Dealloc(&ChunkRdoCache->TruncationPoints);
}


static void
Dealloc(chunk_cache* ChunkCache)
{
  Dealloc(&ChunkCache->Bricks);
  Dealloc(&ChunkCache->BrickSzs);
  Dealloc(&ChunkCache->ChunkStream);
}


static void
Dealloc(file_exp_cache* FileExpCache)
{
  idx2_ForEach (ChunkExpCacheIt, FileExpCache->ChunkExpCaches)
    Dealloc(ChunkExpCacheIt);
  Dealloc(&FileExpCache->ChunkExpCaches);
  Dealloc(&FileExpCache->ChunkExpSzs);
}


static void
Dealloc(file_rdo_cache* FileRdoCache)
{
  idx2_ForEach (TileRdoCacheIt, FileRdoCache->TileRdoCaches)
  {
    Dealloc(TileRdoCacheIt);
  }
}


static void
Dealloc(file_cache* FileCache)
{
  Dealloc(&FileCache->ChunkSizes);
  idx2_ForEach (ChunkCacheIt, FileCache->ChunkCaches)
    Dealloc(ChunkCacheIt.Val);
  Dealloc(&FileCache->ChunkCaches);
}


static void
Init(file_cache_table* FileCacheTable)
{
  Init(&FileCacheTable->FileCaches, 8);
  Init(&FileCacheTable->FileExpCaches, 5);
  Init(&FileCacheTable->FileRdoCaches, 5);
}


static void
Dealloc(file_cache_table* FileCacheTable)
{
  idx2_ForEach (FileCacheIt, FileCacheTable->FileCaches)
    Dealloc(FileCacheIt.Val);
  Dealloc(&FileCacheTable->FileCaches);
  idx2_ForEach (FileExpCacheIt, FileCacheTable->FileExpCaches)
    Dealloc(FileExpCacheIt.Val);
  Dealloc(&FileCacheTable->FileExpCaches);
  idx2_ForEach (FileRdoCacheIt, FileCacheTable->FileRdoCaches)
    Dealloc(FileRdoCacheIt.Val);
  Dealloc(&FileCacheTable->FileRdoCaches);
}


static void
Init(decode_data* D, allocator* Alloc = nullptr)
{
  Init(&D->BrickPool, 5);
  D->Alloc = Alloc ? Alloc : &BrickAlloc_;
  Init(&D->FcTable);
  Init(&D->Streams, 7);
  //  Reserve(&D->RequestedChunks, 64);
}


static void
Dealloc(decode_data* D)
{
  D->Alloc->DeallocAll();
  idx2_ForEach (BrickVolIt, D->BrickPool)
    Dealloc(&BrickVolIt.Val->Vol);
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


static void
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


static error<idx2_err_code>
ReadFileExponents(decode_data* D,
                  hash_table<u64, file_exp_cache>::iterator* FileExpCacheIt,
                  const file_id& FileId)
{
  timer IOTimer;
  StartTimer(&IOTimer);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
  idx2_FSeek(Fp, 0, SEEK_END);
  int S = 0; // total bytes of the encoded chunk sizes
  ReadBackwardPOD(Fp, &S);
  //  idx2_AbortIf(ChunkEMaxSzsSz > 0, "Invalid ChunkEMaxSzsSz from file %s\n",
  //  FileId.Name.ConstPtr); // TODO: we need better validity checking
  Rewind(&D->ChunkEMaxSzsStream);
  GrowToAccomodate(&D->ChunkEMaxSzsStream, S - Size(D->ChunkEMaxSzsStream));
  ReadBackwardBuffer(Fp, &D->ChunkEMaxSzsStream.Stream, S);
  D->BytesExps_ += sizeof(int) + S;
  D->DecodeIOTime_ += ElapsedTime(&IOTimer);
  InitRead(&D->ChunkEMaxSzsStream, D->ChunkEMaxSzsStream.Stream);
  file_exp_cache FileExpCache;
  Reserve(&FileExpCache.ChunkExpSzs, S);
  i32 CeSz = 0;
  while (Size(D->ChunkEMaxSzsStream) < S)
    PushBack(&FileExpCache.ChunkExpSzs, CeSz += (i32)ReadVarByte(&D->ChunkEMaxSzsStream));
  Resize(&FileExpCache.ChunkExpCaches, Size(FileExpCache.ChunkExpSzs));
  idx2_Assert(Size(D->ChunkEMaxSzsStream) == S);
  Insert(FileExpCacheIt, FileId.Id, FileExpCache);

  return idx2_Error(idx2_err_code::NoError);
}


static error<idx2_err_code>
ReadFileRdos(const idx2_file& Idx2,
             hash_table<u64, file_rdo_cache>::iterator* FileRdoCacheIt,
             const file_id& FileId)
{
  timer IOTimer;
  StartTimer(&IOTimer);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
  idx2_FSeek(Fp, 0, SEEK_END);
  int NumChunks = 0;
  i64 Sz = idx2_FTell(Fp) - sizeof(NumChunks);
  ReadBackwardPOD(Fp, &NumChunks);
  //BytesRdos_ += sizeof(NumChunks);
  file_rdo_cache FileRdoCache;
  Resize(&FileRdoCache.TileRdoCaches, NumChunks);
  idx2_RAII(buffer, CompresBuf, AllocBuf(&CompresBuf, Sz), DeallocBuf(&CompresBuf));
  ReadBackwardBuffer(Fp, &CompresBuf);
  //DecodeIOTime_ += ElapsedTime(&IOTimer);
  //BytesRdos_ += Size(CompresBuf);
  idx2_RAII(bitstream, Bs, );
  DecompressBufZstd(CompresBuf, &Bs);
  int Pos = 0;
  idx2_For (int, I, 0, Size(FileRdoCache.TileRdoCaches))
  {
    chunk_rdo_cache& TileRdoCache = FileRdoCache.TileRdoCaches[I];
    Resize(&TileRdoCache.TruncationPoints, Size(Idx2.RdoLevels) * Size(Idx2.Subbands));
    idx2_ForEach (It, TileRdoCache.TruncationPoints)
      *It = ((const i16*)Bs.Stream.Data)[Pos++];
  }
  Insert(FileRdoCacheIt, FileId.Id, FileRdoCache);
  return idx2_Error(idx2_err_code::NoError);
}

static expected<const chunk_rdo_cache*, idx2_err_code>
ReadChunkRdos(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter)
{
  file_id FileId = ConstructFilePathRdos(Idx2, Brick, Iter);
  auto FileRdoCacheIt = Lookup(&D->FcTable.FileRdoCaches, FileId.Id);
  if (!FileRdoCacheIt)
  {
    auto ReadFileOk = ReadFileRdos(Idx2, &FileRdoCacheIt, FileId);
    if (!ReadFileOk)
      idx2_PropagateError(ReadFileOk);
  }
  if (!FileRdoCacheIt)
    return idx2_Error(idx2_err_code::FileNotFound);
  file_rdo_cache* FileRdoCache = FileRdoCacheIt.Val;
  return &FileRdoCache->TileRdoCaches[D->ChunkInFile];
}

/* Given a brick address, open the file associated with the brick and cache its chunk information */
static error<idx2_err_code>
ReadFile(decode_data* D, hash_table<u64, file_cache>::iterator* FileCacheIt, const file_id& FileId)
{
  timer IOTimer;
  StartTimer(&IOTimer);
  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
  idx2_FSeek(Fp, 0, SEEK_END);
  int NChunks = 0;
  ReadBackwardPOD(Fp, &NChunks);
  // TODO: check if there are too many NChunks

  /* read and decompress chunk addresses */
  int IniChunkAddrsSz = NChunks * (int)sizeof(u64);
  int ChunkAddrsSz;
  ReadBackwardPOD(Fp, &ChunkAddrsSz);
  idx2_RAII(buffer,
            CpresChunkAddrs,
            AllocBuf(&CpresChunkAddrs, ChunkAddrsSz),
            DeallocBuf(&CpresChunkAddrs)); // TODO: move to decode_data
  ReadBackwardBuffer(Fp, &CpresChunkAddrs, ChunkAddrsSz);
  D->BytesData_ += ChunkAddrsSz;
  D->DecodeIOTime_ += ElapsedTime(&IOTimer);
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
  D->BytesData_ += ChunkSizesSz;
  D->DecodeIOTime_ += ElapsedTime(&IOTimer);
  InitRead(&D->ChunkSzsStream, D->ChunkSzsStream.Stream);

  /* parse the chunk addresses and cache in memory */
  file_cache FileCache;
  i64 AccumSize = 0;
  Init(&FileCache.ChunkCaches, 10);
  idx2_For (int, I, 0, NChunks)
  {
    i64 ChunkSize = ReadVarByte(&D->ChunkSzsStream);
    u64 ChunkAddr = *((u64*)D->ChunkAddrsStream.Stream.Data + I);
    chunk_cache ChunkCache;
    ChunkCache.ChunkPos = I;
    Insert(&FileCache.ChunkCaches, ChunkAddr, ChunkCache);
    PushBack(&FileCache.ChunkSizes, AccumSize += ChunkSize);
  }
  idx2_Assert(Size(D->ChunkSzsStream) == ChunkSizesSz);
  Insert(FileCacheIt, FileId.Id, FileCache);
  return idx2_Error(idx2_err_code::NoError);
}


/* Given a brick address, read the exponent chunk associated with the brick and cache it */
// TODO: remove the last two params (already stored in D)
static expected<const chunk_exp_cache*, idx2_err_code>
ReadChunkExponents(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Level, i8 Subband)
{
  file_id FileId = ConstructFilePathExponents(Idx2, Brick, Level, Subband);
  auto FileExpCacheIt = Lookup(&D->FcTable.FileExpCaches, FileId.Id);
  if (!FileExpCacheIt)
  {
    auto ReadFileOk = ReadFileExponents(D, &FileExpCacheIt, FileId);
    if (!ReadFileOk)
      idx2_PropagateError(ReadFileOk);
  }
  if (!FileExpCacheIt)
    return idx2_Error(idx2_err_code::FileNotFound);

  file_exp_cache* FileExpCache = FileExpCacheIt.Val;
  idx2_Assert(D->ChunkInFile < Size(FileExpCache->ChunkExpSzs));

  /* find the appropriate chunk */
  if (IsEmpty(FileExpCache->ChunkExpCaches[D->ChunkInFile]))
  {
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
    DecompressBufZstd(buffer{ D->CompressedChunkExps.Data, ChunkExpSize }, &ChunkExpStream);
    D->BytesExps_ += ChunkExpSize;
    D->DecodeIOTime_ += ElapsedTime(&IOTimer);
    InitRead(&ChunkExpStream, ChunkExpStream.Stream);
    FileExpCache->ChunkExpCaches[D->ChunkInFile] = ChunkExpCache;
  }
  return &FileExpCache->ChunkExpCaches[D->ChunkInFile];
}


/* Given a brick address, read the chunk associated with the brick and cache the chunk */
static expected<const chunk_cache*, idx2_err_code>
ReadChunk(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter, i8 Level, i16 BitPlane)
{
  file_id FileId = ConstructFilePath(Idx2, Brick, Iter, Level, BitPlane);
  auto FileCacheIt = Lookup(&D->FcTable.FileCaches, FileId.Id);
  if (!FileCacheIt)
  {
    auto ReadFileOk = ReadFile(D, &FileCacheIt, FileId);
    if (!ReadFileOk)
      idx2_PropagateError(ReadFileOk);
  }
  if (!FileCacheIt)
    return idx2_Error(idx2_err_code::FileNotFound);

  /* find the appropriate chunk */
  u64 ChunkAddress = GetChunkAddress(Idx2, Brick, Iter, Level, BitPlane);
  file_cache* FileCache = FileCacheIt.Val;
  decltype(FileCache->ChunkCaches)::iterator ChunkCacheIt;
  ChunkCacheIt = Lookup(&FileCache->ChunkCaches, ChunkAddress);
  if (!ChunkCacheIt)
    return idx2_Error(idx2_err_code::ChunkNotFound);
  chunk_cache* ChunkCache = ChunkCacheIt.Val;
  if (Size(ChunkCache->ChunkStream.Stream) == 0)
  {
    timer IOTimer;
    StartTimer(&IOTimer);
    idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
    i32 ChunkPos = ChunkCache->ChunkPos;
    i64 ChunkOffset = ChunkPos > 0 ? FileCache->ChunkSizes[ChunkPos - 1] : 0;
    i64 ChunkSize = FileCache->ChunkSizes[ChunkPos] - ChunkOffset;
    idx2_FSeek(Fp, ChunkOffset, SEEK_SET);
    bitstream ChunkStream;
    InitWrite(&ChunkStream,
              ChunkSize); // NOTE: not a memory leak since we will keep track of this in ChunkCache
    ReadBuffer(Fp, &ChunkStream.Stream);
    D->BytesData_ += Size(ChunkStream.Stream);
    D->DecodeIOTime_ += ElapsedTime(&IOTimer);
    DecompressChunk(&ChunkStream,
                    ChunkCache,
                    ChunkAddress,
                    Log2Ceil(Idx2.BricksPerChunks[Iter])); // TODO: check for error
    //    PushBack(&D->RequestedChunks, t2<u64, u64>{ChunkAddress, FileId.Id});
  }

  return ChunkCacheIt.Val;
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
  /* read the rdo information if present */
  int MinBitPlane = traits<i16>::Min;
  if (Size(Idx2.RdoLevels) > 0 && D->QualityLevel >= 0)
  {
    //    printf("reading rdo\n");
    auto ReadChunkRdoResult = ReadChunkRdos(Idx2, D, Brick, D->Level);
    if (!ReadChunkRdoResult)
      return Error(ReadChunkRdoResult);
    const chunk_rdo_cache* ChunkRdoCache = Value(ReadChunkRdoResult);
    int Ql = Min(D->QualityLevel, (int)Size(Idx2.RdoLevels) - 1);
    MinBitPlane = ChunkRdoCache->TruncationPoints[D->Subband * Size(Idx2.RdoLevels) + Ql];
  }

  if (MinBitPlane == traits<i16>::Max)
    return idx2_Error(idx2_err_code::NoError);
  int BlockCount = Prod(NBlocks3);
  if (D->Subband == 0 && D->Level + 1 < Idx2.NLevels)
    BlockCount -= Prod(SbDims3 / Idx2.BlockDims3);

  /* first, read the block exponents */
  auto ReadChunkExpResult = ReadChunkExponents(Idx2, D, Brick, D->Level, D->Subband);
  if (!ReadChunkExpResult)
    return Error(ReadChunkExpResult);

  const chunk_exp_cache* ChunkExpCache = Value(ReadChunkExpResult);
  i32 BrickExpOffset = (D->BrickInChunk * BlockCount) * (SizeOf(Idx2.DType) > 4 ? 2 : 1);
  bitstream BrickExpsStream = ChunkExpCache->BrickExpsStream;
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
      continue; // CodedInNextIter just means that this block belongs to the LLL subband?
    // we read the exponent for the block
    i16 EMax = SizeOf(Idx2.DType) > 4
                 ? (i16)Read(&BrickExpsStream, 16) - traits<f64>::ExpBias
                 : (i16)Read(&BrickExpsStream, traits<f32>::ExpBits) - traits<f32>::ExpBias;
    i8 N = 0;
    i8 EndBitPlane = Min(i8(BitSizeOf(Idx2.DType) + (24 + NDims)), NBitPlanes);
    int NBitPlanesDecoded = Exponent(Accuracy) - 6 - EMax + 1;
    i8 NBps = 0;
    idx2_InclusiveForBackward (i8, Bp, NBitPlanes - 1, NBitPlanes - EndBitPlane)
    { // bit plane loop
      i16 RealBp = Bp + EMax;
      if (NBitPlanes - 6 > RealBp - Exponent(Accuracy) + 1)
        break; // this bit plane is not needed to satisfy the input accuracy
      if (RealBp < MinBitPlane)
        break; // break due to rdo optimization
      auto StreamIt = Lookup(&Streams, RealBp);
      bitstream* Stream = nullptr;
      if (!StreamIt)
      { // first block in the brick
        auto ReadChunkResult = ReadChunk(Idx2, D, Brick, D->Level, D->Subband, RealBp);
        if (!ReadChunkResult)
        {
          idx2_Assert(false);
          return Error(ReadChunkResult);
        }
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
      if (NBitPlanesDecoded <= 8)
        Decode(BlockUInts, NVals, Bp, N, Stream); // use AVX2
      else
        DecodeTest(&BlockUInts[NBitPlanes - 1 - Bp],
                   NVals,
                   N,
                   Stream); // delay the transpose of bits to later
                            //      DecodeTime_ += Seconds(ElapsedTime(&Timer));
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


static void
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
        v3i NChildren3 = Dims(Crop(extent(From3, Idx2.GroupBrick3), extent(Idx2.NBricks3s[Level])));
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
        DecodeSubband(Idx2, D, Accuracy, S.Grid, &BVol);
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
}


/* TODO: dealloc chunks after we are done with them */
void
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf)
{
  timer DecodeTimer;
  StartTimer(&DecodeTimer);
  // TODO: we should add a --effective-mask
  grid OutGrid = GetGrid(Idx2, P.DecodeExtent);
  printf("output grid = " idx2_PrStrGrid "\n", idx2_PrGrid(OutGrid));
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
                             : idx2_PrintScratch("%s/%s", P.OutDir, ToRawFileName(Met));
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
          u64 BrickKey = GetBrickKey(Level, D.Brick[Level]);
          Insert(&D.BrickPool, BrickKey, BVol);
          DecodeBrick(Idx2, P, &D, Accuracy);
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
          Idx2.BrickOrderChunks[Level],
          ChunkTop.ChunkFrom3 * Idx2.BricksPerChunk3s[Level],
          Idx2.BricksPerChunk3s[Level],
          ExtentInBricks,
          VolExtentInBricks);
        ,
        64,
        Idx2.ChunkOrderFiles[Level],
        FileTop.FileFrom3 * Idx2.ChunksPerFile3s[Level],
        Idx2.ChunksPerFile3s[Level],
        ExtentInChunks,
        VolExtentInChunks);
      , 64, Idx2.FileOrders[Level], v3i(0), Idx2.NFiles3s[Level], ExtentInFiles, VolExtentInFiles);
  } // end level loop
    //  printf("count zeroes        = %lld\n", CountZeroes);
  printf("total decode time   = %f\n", Seconds(ElapsedTime(&DecodeTimer)));
  printf("io time             = %f\n", Seconds(D.DecodeIOTime_));
  printf("data movement time  = %f\n", Seconds(D.DataMovementTime_));
  printf("rdo   bytes read    = %" PRIi64 "\n", D.BytesRdos_);
  printf("exp   bytes read    = %" PRIi64 "\n", D.BytesExps_);
  printf("data  bytes read    = %" PRIi64 "\n", D.BytesData_);
  printf("total bytes read    = %" PRIi64 "\n", D.BytesRdos_ + D.BytesExps_ + D.BytesData_);
}


static void
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
  Resize(&ChunkCache->BrickSzs, NBricks);

  /* decompress and store the brick sizes */
  i32 BrickSize = 0;
  SeekToNextByte(ChunkStream);
  idx2_ForEach (BrickSzIt, ChunkCache->BrickSzs)
    *BrickSzIt = BrickSize += (i32)ReadVarByte(ChunkStream);
  ChunkCache->ChunkStream = *ChunkStream;
}


// TODO: return error type
error<idx2_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName)
{
  buffer Buf;
  idx2_CleanUp(DeallocBuf(&Buf));
  idx2_PropagateIfError(ReadFile(FileName, &Buf));
  SExprResult Result = ParseSExpr((cstr)Buf.Data, Size(Buf), nullptr);
  if (Result.type == SE_SYNTAX_ERROR)
  {
    fprintf(stderr, "Error(%d): %s.\n", Result.syntaxError.lineNumber, Result.syntaxError.message);
    return idx2_Error(idx2_err_code::SyntaxError);
  }
  else
  {
    SExpr* Data = (SExpr*)malloc(sizeof(SExpr) * Result.count);
    idx2_CleanUp(free(Data));
    array<SExpr*> Stack;
    Reserve(&Stack, Result.count);
    idx2_CleanUp(Dealloc(&Stack));
    // This time we supply the pool
    SExprPool Pool = { Result.count, Data };
    Result = ParseSExpr((cstr)Buf.Data, Size(Buf), &Pool);
    // result.expr contains the successfully parsed SExpr
    //    printf("parse .idx2 file successfully\n");
    PushBack(&Stack, Result.expr);
    bool GotId = false;
    SExpr* LastExpr = nullptr;
    while (Size(Stack) > 0)
    {
      SExpr* Expr = Back(Stack);
      PopBack(&Stack);
      if (Expr->next)
        PushBack(&Stack, Expr->next);
      if (GotId)
      {
        if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "version"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Version[0] = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_INT);
          Idx2->Version[1] = Expr->i;
          //          printf("Version = %d.%d\n", Idx2->Version[0], Idx2->Version[1]);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "name"))
        {
          idx2_Assert(Expr->type == SE_STRING);
          snprintf(Idx2->Name, Expr->s.len + 1, "%s", (cstr)Buf.Data + Expr->s.start);
          //          printf("Name = %s\n", Idx2->Name);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "field"))
        {
          idx2_Assert(Expr->type == SE_STRING);
          snprintf(Idx2->Field, Expr->s.len + 1, "%s", (cstr)Buf.Data + Expr->s.start);
          //          printf("Field = %s\n", Idx2->Field);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "dimensions"))
        {
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
        }
        if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "accuracy"))
        {
          idx2_Assert(Expr->type == SE_FLOAT);
          Idx2->Accuracy = Expr->f;
          //          printf("Accuracy = %.17g\n", Idx2->Accuracy);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "data-type"))
        {
          idx2_Assert(Expr->type == SE_STRING);
          Idx2->DType = StringTo<dtype>()(stref((cstr)Buf.Data + Expr->s.start, Expr->s.len));
          //          printf("Data type = %.*s\n", ToString(Idx2->DType).Size,
          //          ToString(Idx2->DType).ConstPtr);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "min-max"))
        {
          idx2_Assert(Expr->type == SE_FLOAT || Expr->type == SE_INT);
          Idx2->ValueRange.Min = Expr->i;
          idx2_Assert(Expr->next);
          Expr = Expr->next;
          idx2_Assert(Expr->type == SE_FLOAT || Expr->type == SE_INT);
          Idx2->ValueRange.Max = Expr->i;
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "brick-size"))
        {
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
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "transform-order"))
        {
          idx2_Assert(Expr->type == SE_STRING);
          Idx2->TformOrder =
            EncodeTransformOrder(stref((cstr)Buf.Data + Expr->s.start, Expr->s.len));
          char TransformOrder[128];
          DecodeTransformOrder(Idx2->TformOrder, TransformOrder);
          //          printf("Transform order = %s\n", TransformOrder);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "num-levels"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->NLevels = i8(Expr->i);
          //          printf("Num levels = %d\n", Idx2->NLevels);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "bricks-per-tile"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->BricksPerChunkIn = Expr->i;
          //          printf("Bricks per chunk = %d\n", Idx2->BricksPerChunks[0]);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "tiles-per-file"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->ChunksPerFileIn = Expr->i;
          //          printf("Chunks per file = %d\n", Idx2->ChunksPerFiles[0]);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "files-per-directory"))
        {
          idx2_Assert(Expr->type == SE_INT);
          Idx2->FilesPerDir = Expr->i;
          //          printf("Files per directory = %d\n", Idx2->FilesPerDir);
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "group-levels"))
        {
          idx2_Assert(Expr->type == SE_BOOL);
          Idx2->GroupLevels = Expr->i;
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "group-sub-levels"))
        {
          idx2_Assert(Expr->type == SE_BOOL);
          Idx2->GroupSubLevels = Expr->i;
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "group-bit-planes"))
        {
          idx2_Assert(Expr->type == SE_BOOL);
          Idx2->GroupBitPlanes = Expr->i;
        }
        else if (SExprStringEqual((cstr)Buf.Data, &(LastExpr->s), "quality-levels"))
        {
          int NumQualityLevels = Expr->i;
          Resize(&Idx2->QualityLevelsIn, NumQualityLevels);
          idx2_For (int, I, 0, NumQualityLevels)
          {
            Idx2->QualityLevelsIn[I] = Expr->i;
            Expr = Expr->next;
          }
        }
      }
      if (Expr->type == SE_ID)
      {
        LastExpr = Expr;
        GotId = true;
      }
      else if (Expr->type == SE_LIST)
      {
        PushBack(&Stack, Expr->head);
        GotId = false;
      }
      else
      {
        GotId = false;
      }
    }
  }
  return idx2_Error(idx2_err_code::NoError);
}


template <typename t> void
Dealloc(brick_table<t>* BrickTable)
{
  idx2_ForEach (BrickIt, BrickTable->Bricks)
    BrickTable->Alloc->Dealloc(BrickIt.Val->Samples);
  Dealloc(&BrickTable->Bricks);
  idx2_ForEach (BlockSig, BrickTable->BlockSigs)
    Dealloc(BlockSig);
}


struct index_key
{
  u64 LinearBrick;
  u32 BitStreamKey; // key consisting of bit plane, level, and sub-level
  idx2_Inline bool operator==(const index_key& Other) const
  {
    return LinearBrick == Other.LinearBrick && BitStreamKey == Other.BitStreamKey;
  }
};


struct brick_index
{
  u64 LinearBrick = 0;
  u64 Offset = 0;
};


idx2_Inline u64
Hash(const index_key& IdxKey)
{
  return (IdxKey.LinearBrick + 1) * (1 + IdxKey.BitStreamKey);
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


/* Upscale a single brick to a given resolution level */
// TODO: upscale across levels
template <typename t> static void
UpscaleBrick(const grid& Grid,
             int TformOrder,
             const brick<t>& Brick,
             int Level,
             const grid& OutGrid,
             volume* OutBrickVol)
{
  idx2_Assert(Level >= Brick.Level);
  idx2_Assert(OutBrickVol->Type == dtype::float64);
  v3i Dims3 = Dims(Grid);
  volume BrickVol(buffer((byte*)Brick.Samples, Prod(Dims3) * sizeof(f64)), Dims3, dtype::float64);
  if (Level > Brick.Level)
    *OutBrickVol = 0;
  Copy(Relative(Grid, Grid), BrickVol, Relative(Grid, OutGrid), OutBrickVol);
  if (Level > Brick.Level)
  {
    InverseCdf53(
      Dims(*OutBrickVol), Dims(*OutBrickVol), Level - Brick.Level, TformOrder, OutBrickVol, true);
  }
}


/* Flatten a brick table. the function allocates memory for its output. */
// TODO: upscale across levels
template <typename t> static void
FlattenBrickTable(const array<grid>& LevelGrids,
                  int TformOrder,
                  const brick_table<t>& BrickTable,
                  volume* VolOut)
{
  idx2_Assert(Size(BrickTable.Bricks) > 0);
  /* determine the maximum level of all bricks in the table */
  int MaxLevel = 0;
  auto ItEnd = End(BrickTable.Bricks);
  for (auto It = Begin(BrickTable.Bricks); It != ItEnd; ++It)
  {
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
  for (++It; It != ItEnd; ++It)
  {
    v3i P3 = DecodeMorton3(*(It.Key) >> 4);
    Ext = BoundingBox(Ext, extent(P3 * Dims3, Dims3));
  }
  Resize(VolOut, Dims(Ext));
  /* upscale every brick */
  volume BrickVol(DimsExt3, dtype::float64);
  idx2_CleanUp(Dealloc(&BrickVol));
  for (auto It = Begin(BrickTable.Bricks); It != ItEnd; ++It)
  {
    v3i P3 = DecodeMorton3(*(It.Key) >> 4);
    UpscaleBrick(
      LevelGrids[It.Val->Level], TformOrder, *(It.Val), MaxLevel, LevelGrids[MaxLevel], &BrickVol);
    Copy(extent(Dims3), BrickVol, extent(P3 * Dims3, Dims3), VolOut);
  }
}


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


} // namespace idx2

