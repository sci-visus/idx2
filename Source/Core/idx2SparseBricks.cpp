#include "idx2SparseBricks.h"
#include "idx2Common.h"

namespace idx2
{


struct index_key
{
  u64 LinearBrick;
  u32 BitStreamKey; // key consisting of bit plane, level, and sub-level
  idx2_Inline bool operator==(const index_key& Other) const
  {
    return LinearBrick == Other.LinearBrick && BitStreamKey == Other.BitStreamKey;
  }
};


struct brick_index
{
  u64 LinearBrick = 0;
  u64 Offset = 0;
};


idx2_Inline u64
Hash(const index_key& IdxKey)
{
  return (IdxKey.LinearBrick + 1) * (1 + IdxKey.BitStreamKey);
}


template <typename t> void
Dealloc(brick_table<t>* BrickTable)
{
  idx2_ForEach (BrickIt, BrickTable->Bricks)
    BrickTable->Alloc->Dealloc(BrickIt.Val->Samples);
  Dealloc(&BrickTable->Bricks);
  idx2_ForEach (BlockSig, BrickTable->BlockSigs)
    Dealloc(BlockSig);
}


/* Upscale a single brick to a given resolution level */
// TODO: upscale across levels
template <typename t> static void
UpscaleBrick(const grid& Grid,
             int TformOrder,
             const brick<t>& Brick,
             int Level,
             const grid& OutGrid,
             volume* OutBrickVol)
{
  idx2_Assert(Level >= Brick.Level);
  idx2_Assert(OutBrickVol->Type == dtype::float64);
  v3i Dims3 = Dims(Grid);
  volume BrickVol(buffer((byte*)Brick.Samples, Prod(Dims3) * sizeof(f64)), Dims3, dtype::float64);
  if (Level > Brick.Level)
    *OutBrickVol = 0;
  Copy(Relative(Grid, Grid), BrickVol, Relative(Grid, OutGrid), OutBrickVol);
  if (Level > Brick.Level)
  {
    InverseCdf53(
      Dims(*OutBrickVol), Dims(*OutBrickVol), Level - Brick.Level, TformOrder, OutBrickVol, true);
  }
}


/* Flatten a brick table. the function allocates memory for its output. */
// TODO: upscale across levels
template <typename t> static void
FlattenBrickTable(const array<grid>& LevelGrids,
                  int TformOrder,
                  const brick_table<t>& BrickTable,
                  volume* VolOut)
{
  idx2_Assert(Size(BrickTable.Bricks) > 0);
  /* determine the maximum level of all bricks in the table */
  int MaxLevel = 0;
  auto ItEnd = End(BrickTable.Bricks);
  for (auto It = Begin(BrickTable.Bricks); It != ItEnd; ++It)
  {
    int Iteration = *(It.Key) & 0xF;
    idx2_Assert(Iteration == 0); // TODO: for now we only support one level
    MaxLevel = Max(MaxLevel, (int)It.Val->Level);
  }
  /* allocate memory for VolOut */
  v3i DimsExt3 = Dims(LevelGrids[MaxLevel]);
  v3i Dims3 = idx2_NonExtDims(DimsExt3);
  idx2_Assert(IsPow2(Dims3.X) && IsPow2(Dims3.Y) && IsPow2(Dims3.Z));
  auto It = Begin(BrickTable.Bricks);
  extent Ext(DecodeMorton3(*(It.Key) >> 4));
  for (++It; It != ItEnd; ++It)
  {
    v3i P3 = DecodeMorton3(*(It.Key) >> 4);
    Ext = BoundingBox(Ext, extent(P3 * Dims3, Dims3));
  }
  Resize(VolOut, Dims(Ext));
  /* upscale every brick */
  volume BrickVol(DimsExt3, dtype::float64);
  idx2_CleanUp(Dealloc(&BrickVol));
  for (auto It = Begin(BrickTable.Bricks); It != ItEnd; ++It)
  {
    v3i P3 = DecodeMorton3(*(It.Key) >> 4);
    UpscaleBrick(
      LevelGrids[It.Val->Level], TformOrder, *(It.Val), MaxLevel, LevelGrids[MaxLevel], &BrickVol);
    Copy(extent(Dims3), BrickVol, extent(P3 * Dims3, Dims3), VolOut);
  }
}


} // namespace idx2

