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


struct decode_state
{
  i8 Level;
  i8 Subband;
  u64 Brick;
  v3i Brick3;
  i32 BrickInChunk;
  hash_table<i16, bitstream>* Streams = nullptr;
};


struct decode_data
{
  allocator* Alloc = nullptr;
  file_cache_table FileCacheTable;
  brick_pool BrickPool;

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
Init(decode_data* D, const idx2_file* Idx2, allocator* Alloc = nullptr);

void
Dealloc(decode_data* D);

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

