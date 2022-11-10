#include "InputOutput.h"
#include "BitStream.h"
#include "Error.h"
#include "Expected.h"
#include "Timer.h"
#include "VarInt.h"
#include "idx2Lookup.h"
#include "idx2Read.h"
#include "idx2Decode.h"


namespace idx2
{


static error<idx2_err_code>
ReadFileExponents(decode_data* D,
                  file_cache_table::iterator* FileExpCacheIt,
                  const file_id& FileId);


static error<idx2_err_code>
ReadFile(decode_data* D, file_cache_table::iterator* FileCacheIt, const file_id& FileId);


//static idx2_Inline bool
//IsEmpty(const chunk_exp_cache& ChunkExpCache)
//{
//  return Size(ChunkExpCache.BrickExpsStream.Stream) == 0;
//}


//static void
//Dealloc(chunk_exp_cache* ChunkExpCache)
//{
//  Dealloc(&ChunkExpCache->BrickExpsStream);
//}


static void
Dealloc(chunk_cache* ChunkCache)
{
  Dealloc(&ChunkCache->Bricks);
  Dealloc(&ChunkCache->BrickSzs);
  Dealloc(&ChunkCache->ChunkStream);
}


static void
Dealloc(file_cache* FileCache)
{
  Dealloc(&FileCache->ChunkOffsets);
  Dealloc(&FileCache->ChunkCaches);
  //Dealloc(&FileCache->ChunkExpCaches);
  Dealloc(&FileCache->ChunkExpOffsets);
}


//void
//InitFileCacheTable(file_cache_table* FileCacheTable)
//{
//  Init(&FileCacheTable->FileCaches, 8);
//  Init(&FileCacheTable->FileExpCaches, 5);
//  Init(&FileCacheTable->FileRdoCaches, 5);
//}


void
DeallocFileCacheTable(file_cache_table* FileCacheTable)
{
  idx2_ForEach (FileCacheIt, *FileCacheTable)
  {
    Dealloc(FileCacheIt.Val);
  }
  Dealloc(FileCacheTable);
}


/* Given a brick address, open the file associated with the brick and cache its chunk information */
static error<idx2_err_code>
ReadFile(decode_data* D, file_cache_table::iterator* FileCacheIt, const file_id& FileId)
{
  timer IOTimer;
  StartTimer(&IOTimer);

  if (*FileCacheIt && FileCacheIt->Val->DataCached)
  {
    return idx2_Error(idx2_err_code::NoError);
  }

  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
  idx2_ReturnErrorIf(!Fp, idx2::idx2_err_code::FileNotFound, "File: %s", FileId.Name.ConstPtr);
  idx2_FSeek(Fp, 0, SEEK_END);
  i64 FileSize = idx2_FTell(Fp);
  int S = 0; // total number of bytes used to store exponents info
  ReadBackwardPOD(Fp, &S);
  idx2_FSeek(Fp, (FileSize - S - sizeof(S)), SEEK_SET); // skip the exponents info at the end
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
  printf("read file %s\n", FileId.Name.ConstPtr);
  printf("num chunks = %d\n", NChunks);
  idx2_For (int, I, 0, NChunks)
  {
    i64 ChunkSize = ReadVarByte(&D->ChunkSzsStream); // TODO: use i32 for chunk size
    u64 ChunkAddr = *((u64*)D->ChunkAddrsStream.Stream.Data + I);
    chunk_cache ChunkCache;
    ChunkCache.ChunkPos = I;
    Insert(&FileCache.ChunkCaches, ChunkAddr, ChunkCache);
    printf("chunk %llu size = %lld\n", ChunkAddr, ChunkSize);
    PushBack(&FileCache.ChunkOffsets, AccumSize += ChunkSize);
  }
  idx2_Assert(Size(D->ChunkSzsStream) == ChunkSizesSz);
  if (FileId.Id == 2305843009213693952ull)
    int Stop = 0;

  if (!*FileCacheIt)
  {
    Insert(FileCacheIt, FileId.Id, FileCache);
  }
  else
  {
    FileCacheIt->Val->ChunkCaches = FileCache.ChunkCaches;
    FileCacheIt->Val->ChunkOffsets = FileCache.ChunkOffsets;
  }
  FileCacheIt->Val->DataCached = true;

  return idx2_Error(idx2_err_code::NoError);
}


/* Given a brick address, read the chunk associated with the brick and cache the chunk */
expected<const chunk_cache*, idx2_err_code>
ReadChunk(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter, i8 Level, i16 BitPlane)
{
  file_id FileId = ConstructFilePath(Idx2, Brick, Iter, Level, BitPlane);
  auto FileCacheIt = Lookup(&D->FileCacheTable, FileId.Id);
  idx2_PropagateIfError(ReadFile(D, &FileCacheIt, FileId));
  if (!FileCacheIt)
    return idx2_Error(idx2_err_code::FileNotFound, "File: %s\n", FileId.Name.ConstPtr);

  /* find the appropriate chunk */
  u64 ChunkAddress = GetChunkAddress(Idx2, Brick, Iter, Level, BitPlane);
  file_cache* FileCache = FileCacheIt.Val;
  decltype(FileCache->ChunkCaches)::iterator ChunkCacheIt;
  ChunkCacheIt = Lookup(&FileCache->ChunkCaches, ChunkAddress);
  if (!ChunkCacheIt)
    return idx2_Error(idx2_err_code::ChunkNotFound);
  chunk_cache* ChunkCache = ChunkCacheIt.Val;
  if (Size(ChunkCache->ChunkStream.Stream) == 0) // chunk has not been loaded
  {
    timer IOTimer;
    StartTimer(&IOTimer);
    idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
    if (!Fp)
      return idx2_Error(idx2_err_code::FileNotFound, "File: %s\n", FileId.Name.ConstPtr);
    i32 ChunkPos = ChunkCache->ChunkPos;
    i64 ChunkOffset = ChunkPos > 0 ? FileCache->ChunkOffsets[ChunkPos - 1] : 0;
    i64 ChunkSize = FileCache->ChunkOffsets[ChunkPos] - ChunkOffset;
    idx2_FSeek(Fp, ChunkOffset, SEEK_SET);
    bitstream ChunkStream;
  // NOTE: not a memory leak since we will keep track of this in ChunkCache
    InitWrite(&ChunkStream, ChunkSize);
    ReadBuffer(Fp, &ChunkStream.Stream);
    D->BytesData_ += Size(ChunkStream.Stream);
    D->DecodeIOTime_ += ElapsedTime(&IOTimer);
    DecompressChunk(&ChunkStream,
                    ChunkCache,
                    ChunkAddress,
                    Log2Ceil(Idx2.BricksPerChunk[Iter])); // TODO: check for error
  }

  return ChunkCacheIt.Val;
}


/* Read and decode the sizes of the compressed exponent chunks in a file */
static error<idx2_err_code>
ReadFileExponents(decode_data* D, file_cache_table::iterator* FileCacheIt, const file_id& FileId)
{
  timer IOTimer;
  StartTimer(&IOTimer);

  if (*FileCacheIt && FileCacheIt->Val->ExpCached)
  {
    return idx2_Error(idx2_err_code::NoError);
  }

  idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
  idx2_ReturnErrorIf(!Fp, idx2::idx2_err_code::FileNotFound, "File: %s", FileId.Name.ConstPtr);
  idx2_FSeek(Fp, 0, SEEK_END);
  i64 FileSize = idx2_FTell(Fp);
  if (FileId.Id == 2305843009213693952ull)
    int Stop = 0;
  int ExponentSize = 0; // total bytes of the encoded chunk sizes
  ReadBackwardPOD(Fp, &ExponentSize); // total size of the exponent info
  int S = 0; // size (in bytes) of the compressed exponent sizes
  ReadBackwardPOD(Fp, &S);
  Rewind(&D->ChunkEMaxSzsStream);
  GrowToAccomodate(&D->ChunkEMaxSzsStream, S - Size(D->ChunkEMaxSzsStream));
  // Read the emax sizes
  ReadBackwardBuffer(Fp, &D->ChunkEMaxSzsStream.Stream, S);
  D->BytesExps_ += sizeof(int) + S;
  D->DecodeIOTime_ += ElapsedTime(&IOTimer);
  InitRead(&D->ChunkEMaxSzsStream, D->ChunkEMaxSzsStream.Stream);

  file_cache FileCache;
  FileCache.ExponentBeginOffset = FileSize - ExponentSize - sizeof(ExponentSize);
  Reserve(&FileCache.ChunkExpOffsets, S);
  i32 CeSz = 0;
  // we compute a "prefix sum" of the sizes to get the offsets
  int NChunks = 0;
  while (Size(D->ChunkEMaxSzsStream) < S)
  {
    PushBack(&FileCache.ChunkExpOffsets, CeSz += (i32)ReadVarByte(&D->ChunkEMaxSzsStream));
    ++NChunks;
  }

  idx2_For (int, I, 0, NChunks)
  {
    chunk_cache ChunkCache;
    ChunkCache.ChunkPos = I;

  }
  //Resize(&FileCache.ChunkCaches, Size(FileCache.ChunkExpOffsets));
  idx2_Assert(Size(D->ChunkEMaxSzsStream) == S);

  if (!*FileCacheIt)
  {
    Insert(FileCacheIt, FileId.Id, FileCache);
  }
  else
  {
    FileCacheIt->Val->ExponentBeginOffset = FileCache.ExponentBeginOffset;
    FileCacheIt->Val->ChunkExpOffsets = FileCache.ChunkExpOffsets;
    FileCacheIt->Val->ChunkCaches = FileCache.ChunkCaches;
  }
  FileCacheIt->Val->ExpCached = true;

  return idx2_Error(idx2_err_code::NoError);
}


/* Given a brick address, read the exponent chunk associated with the brick and cache it */
// TODO: remove the last two params (already stored in D)
expected<const chunk_cache*, idx2_err_code>
ReadChunkExponents(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Level, i8 Subband)
{
  file_id FileId = ConstructFilePath(Idx2, Brick, Level, Subband, ExponentBitPlane_);
  auto FileCacheIt = Lookup(&D->FileCacheTable, FileId.Id);
  idx2_PropagateIfError(ReadFileExponents(D, &FileCacheIt, FileId));
  if (!FileCacheIt)
    return idx2_Error(idx2_err_code::FileNotFound, "File: %s\n", FileId.Name.ConstPtr);

  //file_cache* FileCache = FileCacheIt.Val;
  //FileCache->ExponentCached = true;
  //idx2_Assert(D->ChunkInFile < Size(FileExpCache->ChunkExpSzs));

  /* find the appropriate chunk */
  if (Level == 2 && Subband == 7)
  {
    int Stop = 0;
  }
  if (FileId.Id == 2305843009213693952ull && D->ChunkInFile == 0)
  {
    int Stoop = 0;
  }

  u64 ChunkAddress = GetChunkAddress(Idx2, Brick, Level, Subband, ExponentBitPlane_);
  file_cache* FileCache = FileCacheIt.Val;
  decltype(FileCache->ChunkCaches)::iterator ChunkCacheIt;
  ChunkCacheIt = Lookup(&FileCache->ChunkCaches, ChunkAddress);
  if (!ChunkCacheIt)
    return idx2_Error(idx2_err_code::ChunkNotFound);
  chunk_cache* ChunkCache = ChunkCacheIt.Val;
  if (Size(ChunkCache->ChunkStream.Stream) == 0) // chunk has not been loaded
  {
    timer IOTimer;
    StartTimer(&IOTimer);
    idx2_RAII(FILE*, Fp = fopen(FileId.Name.ConstPtr, "rb"), , if (Fp) fclose(Fp));
    if (!Fp)
      return idx2_Error(idx2_err_code::FileNotFound, "File: %s\n", FileId.Name.ConstPtr);
    i32 ChunkPos = ChunkCache->ChunkPos;
    i64 ChunkExpOffset = FileCache->ExponentBeginOffset;
    i32 ChunkExpSize = FileCache->ChunkExpOffsets[ChunkPos];
    if (ChunkPos > 0)
    {
      i32 PrevChunkOffset = FileCache->ChunkExpOffsets[ChunkPos - 1];
      ChunkExpOffset += PrevChunkOffset;
      ChunkExpSize -= PrevChunkOffset;
    }
    idx2_FSeek(Fp, ChunkExpOffset, SEEK_SET);
    bitstream& ChunkExpStream = ChunkCache->ChunkStream;
    // TODO: calculate the number of bricks in this chunk in a different way to verify correctness
    Resize(&D->CompressedChunkExps, ChunkExpSize);
    ReadBuffer(Fp, &D->CompressedChunkExps, ChunkExpSize);
    if (Level == 2 && Subband == 7)
    {
      int Stop = 0;
    }
    DecompressBufZstd(buffer{ D->CompressedChunkExps.Data, ChunkExpSize }, &ChunkExpStream);
    D->BytesDecoded_ += ChunkExpSize;
    D->BytesExps_ += ChunkExpSize;
    D->DecodeIOTime_ += ElapsedTime(&IOTimer);
    InitRead(&ChunkExpStream, ChunkExpStream.Stream);
  }

  return ChunkCacheIt.Val;
}


//expected<const chunk_rdo_cache*, idx2_err_code>
//ReadChunkRdos(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter)
//{
//  file_id FileId = ConstructFilePathRdos(Idx2, Brick, Iter);
//  auto FileRdoCacheIt = Lookup(&D->FcTable.FileRdoCaches, FileId.Id);
//  if (!FileRdoCacheIt)
//  {
//    auto ReadFileOk = ReadFileRdos(Idx2, &FileRdoCacheIt, FileId);
//    if (!ReadFileOk)
//      idx2_PropagateError(ReadFileOk);
//  }
//  if (!FileRdoCacheIt)
//    return idx2_Error(idx2_err_code::FileNotFound);
//  file_rdo_cache* FileRdoCache = FileRdoCacheIt.Val;
//  return &FileRdoCache->TileRdoCaches[D->ChunkInFile];
//}


} // namespace idx2

