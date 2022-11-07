#pragma once


#include "Expected.h"
#include "HashTable.h"
#include "idx2Common.h"


namespace idx2
{


// TODO: merge this with the below
struct chunk_exp_cache
{
  bitstream BrickExpsStream;
};


struct chunk_cache
{
  i32 ChunkPos; // chunk position in the file
  array<u64> Bricks;
  array<i32> BrickSzs;
  bitstream ChunkStream;
};


struct file_cache
{
  array<i64> ChunkSizes;                    // TODO: 32-bit to store chunk sizes?
  array<chunk_exp_cache> ChunkExpCaches;
  array<i32> ChunkExpSizes;
  hash_table<u64, chunk_cache> ChunkCaches; // [chunk address] -> chunk cache
};


// [file address] -> file cache
using file_cache_table = hash_table<u64, file_cache>;


struct decode_data;


void
DeallocFileCacheTable(file_cache_table* FileCacheTable);


idx2_Inline i64
Size(const chunk_exp_cache& ChunkExpCache)
{
  return Size(ChunkExpCache.BrickExpsStream.Stream);
}


idx2_Inline i64
Size(const chunk_cache& C)
{
  return Size(C.Bricks) * sizeof(u64) + Size(C.BrickSzs) * sizeof(i32) + sizeof(C.ChunkPos) +
         Size(C.ChunkStream.Stream);
}


idx2_Inline i64
Size(const file_cache& F)
{
  /* bit plane chunks */
  i64 Result = 0;
  Result += Size(F.ChunkSizes) * sizeof(i64);
  idx2_ForEach (It, F.ChunkCaches)
    Result += Size(*It.Val);

  /* exponent chunks */
  idx2_ForEach (It, F.ChunkExpCaches)
    Result += Size(*It);
  Result += Size(F.ChunkExpSizes) * sizeof(i32);
  return Result;
}


idx2_Inline i64
Size(const file_cache_table& F)
{
  i64 Result = 0;
  idx2_ForEach (It, F)
    Result += Size(*It.Val);
//  idx2_ForEach (It, F.FileExpCaches)
//    Result += Size(*It.Val);
//  idx2_ForEach (It, F.FileRdoCaches)
//    Result += Size(*It.Val);
  return Result;
}


expected<const chunk_exp_cache*, idx2_err_code>
ReadChunkExponents(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Level, i8 Subband);


expected<const chunk_cache*, idx2_err_code>
ReadChunk(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter, i8 Level, i16 BitPlane);


} // namespace idx2

