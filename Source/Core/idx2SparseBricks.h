#pragma once


#include "BitStream.h"
#include "HashTable.h"
#include "Volume.h"

namespace idx2
{


// TODO: allow the bricks to have different resolutions depending on the subbands decoded
struct brick_volume
{
  volume Vol;
  extent ExtentLocal; // dimensions of the brick // TODO: we do not need full extent, just dims v3i
  extent ExtentGlobal; // global extent of the brick
  i8 NChildrenDecoded = 0;
  i8 NChildrenReturned = 0;
  i8 NChildrenMax = 0;
  bool Significant = false; // if any (non 0) subband is decoded for this brick
  bool AnySignificantChildren = false;
};


using brick_table = hash_table<u64, brick_volume>;

struct decode_data;
struct idx2_file;

struct brick_pool
{
  brick_table BrickTable;
  // We use 4 bits for each brick at the finest resolution to specify the finest resolution
  // at the spatial location of the brick (this depends on how much the refinement is at that
  // location)
  bitstream ResolutionStream;
  const idx2_file* Idx2 = nullptr;
  //v3i Dims3 = v3i(0);
  //v3i BrickDims3 = v3i(0); // dimensions of bricks, should be powers of 2
  //i8 NLevels = 0; // number of levels, level 0 is the finest, NLevels - 1 is the coarsest
};


void
Init(brick_pool* Bp, idx2_file* Idx2, decode_data* D);


void
Dealloc(brick_pool* Bp);


/* For all finest-resolution bricks, output the actual resolution */
void
ComputeBrickResolution(brick_pool* Bp);


/* Given a position, return the grid encompassing the point */
grid
PointQuery(const brick_pool& Bp, const v3i& P3);


/* Interpolate to get the value at a point */
f64
Interpolate(const v3i& P3, const grid& Grid);


idx2_Inline i64
Size(const brick_volume& B)
{
  return Prod(Dims(B.Vol)) * SizeOf(B.Vol.Type);
}


} // namespace idx2

