#pragma once

#include "Array.h"
#include "BitStream.h"
#include "HashTable.h"
#include "Memory.h"
#include "idx2Common.h"

namespace idx2 {

/* ---------------------- TYPES ----------------------*/

struct block_sig {
  u32 Block = 0;
  i16 BitPlane = 0;
};

struct chunk_meta_info {
  array<u64> Addrs; // iteration, level, bit plane, chunk id
  bitstream Sizes; // TODO: do we need to init this?
};

// Each channel corresponds to one (iteration, subband, bit plane) tuple
struct channel {
  /* brick-related streams, to be reset once per chunk */
  bitstream BrickDeltasStream; // store data for many bricks
  bitstream BrickSzsStream; // store data for many bricks
  bitstream BrickStream; // store data for many bricks
  /* block-related streams, to be reset once per brick */
  bitstream BlockStream; // store data for many blocks
  u64 LastChunk = 0; // current chunk
  u64 LastBrick = 0;
  i32 NBricks = 0;
};

// Each sub-channel corresponds to one (iteration, subband) tuple
struct sub_channel {
  bitstream BlockEMaxesStream;
  bitstream BrickEMaxesStream; // at the end of each brick we copy from BlockEMaxesStream to here
  u64 LastChunk = 0;
  u64 LastBrick = 0;
};

struct sub_channel_ptr {
  i8 Iteration = 0;
  i8 Level = 0;
  sub_channel* ChunkEMaxesPtr = nullptr;
  idx2_Inline bool operator<(const sub_channel_ptr& Other) const {
    if (Iteration == Other.Iteration) return Level < Other.Level;
    return Iteration < Other.Iteration;
  }
};

struct rdo_chunk {
  u64 Address;
  i64 Length;
  f64 Lambda;
  idx2_Inline bool operator<(const rdo_chunk& Other) const { return Address > Other.Address; }
};

/* We use this to pass data between different stages of the encoder */
struct encode_data {
  allocator* Alloc = nullptr;
  hash_table<u64, brick_volume> BrickPool;
  hash_table<u32, channel> Channels; // each corresponds to (bit plane, iteration, level)
  hash_table<u16, sub_channel> SubChannels; // only consider level and iteration
  i8 Iter = 0;
  i8 Level = 0;
  stack_array<u64, idx2_file::MaxLevels> Brick;
  stack_array<v3i, idx2_file::MaxLevels> Bricks3;
  hash_table<u64, chunk_meta_info> ChunkMeta; // map from file address to chunk info
  hash_table<u64, bitstream> ChunkEMaxesMeta; // map from file address to a stream of chunk emax sizes
  bitstream CpresEMaxes;
  bitstream CpresChunkAddrs;
  bitstream ChunkStream;
  /* block emaxes related */
  bitstream ChunkEMaxesStream;
  array<block_sig> BlockSigs;
  array<i16> EMaxes;
  bitstream BlockStream; // only used by v0.1
  array<t2<u32, channel*>> SortedChannels;
  array<rdo_chunk> ChunkRDOs; // list of chunks and their sizes, sorted by bit plane
  hash_table<u64, u32> ChunkRDOLengths;
};

/*
By default, copy brick data from a volume to a local brick buffer.
Can be extended polymorphically to provide other ways of copying.
*/
struct brick_copier
{
  const volume* Volume = nullptr;

  brick_copier() {};
  brick_copier(const volume* InputVolume);

  virtual v2d // {Min, Max} values of brick
  Copy(const extent& ExtentGlobal, const extent& ExtentLocal, brick_volume* Brick);
};


/* FUNCTIONS
--------------------------------------------------------------------------------------------*/

void
WriteMetaFile(const idx2_file& Idx2, cstr FileName);

/* Encode a whole volume, assuming the volume is available  */
error<idx2_err_code>
Encode(idx2_file* Idx2, const params& P, brick_copier& Copier);

/* Encode a brick. Use this when the input data is not in the form of a big volume. */
error<idx2_err_code>
EncodeBrick(idx2_file* Idx2, const params& P, const v3i& BrickPos3);


/* INLINE FUNCTIONS
--------------------------------------------------------------------------------------------*/

idx2_Inline void
Init(channel* C)
{
  InitWrite(&C->BrickStream, 16384);
  InitWrite(&C->BrickDeltasStream, 32);
  InitWrite(&C->BrickSzsStream, 256);
  InitWrite(&C->BlockStream, 256);
}


idx2_Inline void
Dealloc(channel* C)
{
  Dealloc(&C->BrickDeltasStream);
  Dealloc(&C->BrickSzsStream);
  Dealloc(&C->BrickStream);
  Dealloc(&C->BlockStream);
}


idx2_Inline void
Init(sub_channel* Sc)
{
  InitWrite(&Sc->BlockEMaxesStream, 64);
  InitWrite(&Sc->BrickEMaxesStream, 8192);
}


idx2_Inline void
Dealloc(sub_channel* Sc)
{
  Dealloc(&Sc->BlockEMaxesStream);
  Dealloc(&Sc->BrickEMaxesStream);
}


idx2_Inline void
Dealloc(chunk_meta_info* Cm)
{
  Dealloc(&Cm->Addrs);
  Dealloc(&Cm->Sizes);
}


} // namespace idx2

