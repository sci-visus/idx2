#pragma once

#include "HashTable.h"
#include "Volume.h"
#include "idx2Common.h"


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


template <typename t> struct brick
{
  t* Samples = nullptr; // TODO: data should stay compressed
  u8 LevelMask = 0;     // TODO: need to change if we support more than one transform pass per brick
  //  stack_array<array<u8>, 8> BlockSigs; // TODO: to support more than one transform pass per
  //  brick, we need a dynamic array
  // friend v3i Dims(const brick<t>& Brick, const array<grid>& LevelGrids);
  // friend t& At(const brick<t>& Brick, array<grid>& LevelGrids, const v3i& P3);
};


template <typename t> struct brick_table
{
  hash_table<u64, brick<t>> Bricks; // hash from BrickKey to Brick
  allocator* Alloc = &Mallocator();
  // TODO: let Enc->Alloc follow this allocator
};


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


struct decode_data
{
  allocator* Alloc = nullptr;
  file_cache_table FcTable;
  hash_table<u64, brick_volume> BrickPool;
  i8 Level  = 0; // current level being decoded
  i8 Subband = 0; // current subband being decoded
  stack_array<u64, idx2_file::MaxLevels> Brick;
  stack_array<v3i, idx2_file::MaxLevels> Bricks3;
  i32 ChunkInFile = 0;
  i32 BrickInChunk = 0;
  stack_array<u64, idx2_file::MaxLevels> Offsets = { {} }; // used by v0.0 only
  bitstream BlockStream;                                   // used only by v0.1
  hash_table<i16, bitstream> Streams;
  buffer CompressedChunkExps;
  bitstream ChunkEMaxSzsStream;
  bitstream ChunkAddrsStream;
  bitstream ChunkSzsStream;
  //  array<t2<u64, u64>> RequestedChunks; // is cleared after each tile
  int QualityLevel = -1;
  int EffIter = 0;
  u64 LastTile = 0;
};


/* ---------------------- FUNCTIONS ----------------------*/

error<idx2_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName);


template <typename t> void
GetBrick(brick_table<t>* BrickTable, i8 Iter, u64 Brick)
{
  // auto
  (void)BrickTable;
  (void)Iter;
  (void)Brick;
}


template <typename t> void
Dealloc(brick_table<t>* BrickTable);


template <typename t> idx2_Inline v3i
Dims(const brick<t>& Brick, const array<grid>& LevelGrids)
{
  return Dims(LevelGrids[Brick.Level]);
}


template <typename t> idx2_Inline t&
At(const brick<t>& Brick, array<grid>& LevelGrids, const v3i& P3)
{
  v3i D3 = Dims(LevelGrids[Brick.Level]);
  idx2_Assert(P3 < D3);
  idx2_Assert(D3 == Dims(Brick));
  i64 Idx = Row(D3, P3);
  return const_cast<t&>(Brick.Samples[Idx]);
}


template <typename t> void
Dealloc(brick<t>* Brick)
{
  free(Brick->Samples);
} // TODO: check this


idx2_Inline i64
Size(const chunk_exp_cache& ChunkExpCache)
{
  return Size(ChunkExpCache.BrickExpsStream.Stream);
}


idx2_Inline i64
Size(const chunk_rdo_cache& ChunkRdoCache)
{
  return Size(ChunkRdoCache.TruncationPoints) * sizeof(i16);
}


idx2_Inline i64
Size(const chunk_cache& C)
{
  return Size(C.Bricks) * sizeof(u64) + Size(C.BrickSzs) * sizeof(i32) + sizeof(C.ChunkPos) +
         Size(C.ChunkStream.Stream);
}


idx2_Inline i64
Size(const file_exp_cache& F)
{
  i64 Result = 0;
  idx2_ForEach (It, F.ChunkExpCaches)
    Result += Size(*It);
  Result += Size(F.ChunkExpSzs) * sizeof(i32);
  return Result;
}


idx2_Inline i64
Size(const file_rdo_cache& F)
{
  i64 Result = 0;
  idx2_ForEach (It, F.TileRdoCaches)
    Result += Size(*It);
  return Result;
}


idx2_Inline i64
Size(const file_cache& F)
{
  i64 Result = 0;
  Result += Size(F.ChunkSizes) * sizeof(i64);
  idx2_ForEach (It, F.ChunkCaches)
    Result += Size(*It.Val);
  return Result;
}


idx2_Inline i64
Size(const file_cache_table& F)
{
  i64 Result = 0;
  idx2_ForEach (It, F.FileCaches)
    Result += Size(*It.Val);
  idx2_ForEach (It, F.FileExpCaches)
    Result += Size(*It.Val);
  idx2_ForEach (It, F.FileRdoCaches)
    Result += Size(*It.Val);
  return Result;
}


idx2_Inline i64
SizeBrickPool(const decode_data& D)
{
  i64 Result = 0;
  idx2_ForEach (It, D.BrickPool)
    Result += Size(*It.Val);
  return Result;
}


// TODO: return an error code?
void
Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf = nullptr);


} // namespace idx2
