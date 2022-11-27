#pragma once

#include "HashTable.h"
#include "Volume.h"
#include "idx2Common.h"
#include "idx2Read.h"
#include "idx2SparseBricks.h"
#include <atomic>
#include <condition_variable>
#include <mutex>


namespace idx2
{


/* ---------------------- TYPES ----------------------*/

struct decode_state
{
  i8 Level;
  i8 Subband;
  u64 Brick;
  v3i Brick3;
  i32 BrickInChunk;
};


struct decode_data
{
  allocator* Alloc = nullptr;
  file_cache_table FileCacheTable;
  brick_pool BrickPool;
  std::mutex FileCacheMutex;
  std::mutex BrickPoolMutex;
  std::mutex Mutex;
  std::condition_variable AllTasksDone;
  i64 NTasks = 0;

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

