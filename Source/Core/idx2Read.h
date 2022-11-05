#pragma once


#include "Expected.h"
#include "HashTable.h"
#include "idx2Common.h"


namespace idx2
{


struct chunk_exp_cache
{
  bitstream BrickExpsStream;
};


struct chunk_rdo_cache
{
  array<i16> TruncationPoints;
};


struct chunk_cache
{
  i32 ChunkPos; // chunk position in the file
  array<u64> Bricks;
  array<i32> BrickSzs;
  bitstream ChunkStream;
};


struct file_exp_cache
{
  array<chunk_exp_cache> ChunkExpCaches;
  array<i32> ChunkExpSzs;
};


struct file_rdo_cache
{
  array<chunk_rdo_cache> TileRdoCaches;
};


struct file_cache
{
  array<i64> ChunkSizes;                    // TODO: 32-bit to store chunk sizes?
  hash_table<u64, chunk_cache> ChunkCaches; // [chunk address] -> chunk cache
};


struct file_cache_table
{
  hash_table<u64, file_cache> FileCaches;        // [file address] -> file cache
  hash_table<u64, file_exp_cache> FileExpCaches; // [file exp address] -> file exp cache
  hash_table<u64, file_rdo_cache> FileRdoCaches; // [file rdo address] -> file rdo cache
};


struct decode_data;


void
Init(file_cache_table* FileCacheTable);

void
Dealloc(file_cache_table* FileCacheTable);


expected<const chunk_exp_cache*, idx2_err_code>
ReadChunkExponents(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Level, i8 Subband);

expected<const chunk_rdo_cache*, idx2_err_code>
ReadChunkRdos(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter);

expected<const chunk_cache*, idx2_err_code>
ReadChunk(const idx2_file& Idx2, decode_data* D, u64 Brick, i8 Iter, i8 Level, i16 BitPlane);


} // namespace idx2

