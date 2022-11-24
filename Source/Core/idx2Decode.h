#pragma once

#include "HashTable.h"
#include "Volume.h"
#include "idx2Common.h"
#include "idx2Read.h"
#include "idx2SparseBricks.h"


namespace idx2
{


/* ---------------------- TYPES ----------------------*/

// By default, decode everything
struct decode_params
{
  bool Decode = false;
  f64 Accuracy = 0;
  u8 Mask = 0xFF;    // for now just a mask TODO: to generalize this we need an array of subbands
  i8 Precision = 64; // number of bit planes to decode
};


/* TODO: multithreading:
* TODO: make FileCacheTable threadsafe
* TODO: make BrickPool threadsafe
* TODO: make Streams threadsafe
* TODO: get rid of Level, Subband, Brick, Bricks3, BrickInChunk (move to a struct to pass around)
*/
struct decode_data
{
  allocator* Alloc = nullptr;
  file_cache_table FileCacheTable;
  brick_pool BrickPool;
  i8 Level  = 0; // current level being decoded
  i8 Subband = 0; // current subband being decoded
  stack_array<u64, idx2_file::MaxLevels> Brick; // current brick index being decoded on each level
  stack_array<v3i, idx2_file::MaxLevels> Bricks3; // current brick being decoded on each level
  //i32 ChunkInFile = 0;
  i32 BrickInChunk = 0;
  hash_table<i16, bitstream> Streams;
  //buffer CompressedChunkExps; // used in ReadChunkExponents
  //bitstream ChunkExpSizeStream; // used in ReadFileExponents
  //bitstream ChunkAddrsStream; // used in ReadFile and ReadFileExponents
  //bitstream ChunkSizeStream; // used in ReadFile

  u64 DecodeIOTime_ = 0;
  u64 BytesExps_ = 0;
  u64 BytesData_ = 0;
  u64 BytesDecoded_ = 0;
  u64 DataMovementTime_ = 0;
  i64 NSignificantBlocks = 0;
  i64 NInsignificantSubbands = 0;
};


/* ---------------------- FUNCTIONS ----------------------*/


void
DecompressChunk(bitstream* ChunkStream, chunk_cache* ChunkCache, u64 ChunkAddress, int L);

// TODO: return an error code?
error<idx2_err_code>
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf = nullptr);

void
DecompressBufZstd(const buffer& Input, buffer* Output);

void
DecompressBufZstd(const buffer& Input, bitstream* Output);

} // namespace idx2

