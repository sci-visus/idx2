#pragma once


#include "HashTable.h"
#include "Volume.h"

namespace idx2
{


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


} // namespace idx2

