#include "BitOps.h"
#include "idx2SparseBricks.h"
#include "idx2Common.h"
#include "idx2Lookup.h"

namespace idx2
{


void
Init(brick_pool* Bp, const idx2_file* Idx2)
{
  Bp->Idx2 = Idx2;
  i64 NFinestBricks = GetLinearBrick(*Idx2, 0, Idx2->NBricks3[0] - 1);
  AllocBuf(&Bp->ResolutionStream.Stream, (NFinestBricks * 4 + 7) / 8);
  ZeroBuf(&Bp->ResolutionStream.Stream);
  InitWrite(&Bp->ResolutionStream, Bp->ResolutionStream.Stream);
  Init(&Bp->BrickTable, 10);
}


void
Dealloc(brick_pool* Bp)
{
  Dealloc(&Bp->ResolutionStream);
  idx2_ForEach (It, Bp->BrickTable)
    Dealloc(&It.Val->Vol);
  Dealloc(&Bp->BrickTable);
}


struct stack_item
{
  v3i Brick3 = v3i(0);
  i8 Level = 0;
  i8 ResolutionToSet = 0;
};


/* Print the percentage of significant bricks on each level */
void
PrintStatistics(const brick_pool* Bp)
{
  const idx2_file* Idx2 = Bp->Idx2;
  i64 Count[idx2_file::MaxLevels] = {};

  idx2_ForEach (BIt, Bp->BrickTable)
  {
    i8 Level = GetLevelFromBrickKey(*BIt.Key);
    ++Count[Level];
  }
  idx2_For (i8, L, 0, Idx2->NLevels)
  {
    i64 NBricks = Prod<i64>(Idx2->NBricks3[L]);
    f64 Percent = f64(Count[L]) * 100 / f64(NBricks);
    printf("level %d: %lld out of %lld bricks significant (%f percent)\n", L, Count[L], NBricks, Percent);
  }
}


// we traverse the brick hierarchy in depth-first order
// if a brick has the AnySignificantChildren flag set, we set the resolution of all its
// descendants to its level (in brick_pool::Resolution)
// and then move to its neighbor on the same level
// TODO: the below isn't quite correct, we need to set the resolution not only when
// the current brick is the first to be significant
void
ComputeBrickResolution(brick_pool* Bp)
{
  const idx2_file* Idx2 = Bp->Idx2;
  stack_item Stack[idx2_file::MaxLevels * 8];
  i8 LastIndex = 0;
  /* push all bricks at the coarsest level */
  v3i CurrCoarsestBrick = v3i(0);
  stack_item& First = Stack[LastIndex++];
  First.Brick3 = CurrCoarsestBrick;
  First.Level = First.ResolutionToSet = Idx2->NLevels - 1;
  int Count = 0;
  while (LastIndex >= 0)
  {
    // pop the stack
    stack_item Current = Stack[LastIndex--];
    // get the current brick
    u64 BrickIndex = GetLinearBrick(*Idx2, Current.Level, Current.Brick3);
    u64 BrickKey = GetBrickKey(Current.Level, BrickIndex);
    auto BrickIt = Lookup(&Bp->BrickTable, BrickKey);

    if (BrickIt)
    {
      idx2_Assert(BrickIt.Val->Significant);
      Current.ResolutionToSet = Current.Level;
    }
    // push the children if not at the finest level
    if (Current.Level > 0)
    {
      i8 NextLevel = Current.Level - 1;
      for (int I = 0; I < 8; ++I)
      {
        int X = BitSet(I, 0);
        int Y = BitSet(I, 1);
        int Z = BitSet(I, 2);
        v3i ChildBrick3 = Current.Brick3 * Idx2->GroupBrick3 + v3i(X, Y, Z);
        if (ChildBrick3 < Idx2->NBricks3[NextLevel])
        {
          stack_item& Child = Stack[++LastIndex];
          Child.Brick3 = ChildBrick3;
          Child.Level = Current.Level - 1;
          Child.ResolutionToSet = Current.ResolutionToSet;
        }
      }
    }
    else // finest level, set the level if necessary
    {
      bitstream* Bs = &Bp->ResolutionStream;
      SeekToBit(Bs, BrickIndex * 4); // we use 4 bits to indicate the resolution of the brick
      Write(Bs, Current.ResolutionToSet, 4);
      //printf("level = %d\n", Current.ResolutionToSet);
    }

    /* push the next brick at the coarsest resolution if stack is empty */
    if (LastIndex < 0)
    {
      ++CurrCoarsestBrick.X;
      if (CurrCoarsestBrick.X >= Idx2->NBricks3[Idx2->NLevels - 1].X)
      {
        CurrCoarsestBrick.X = 0;
        ++CurrCoarsestBrick.Y;
        if (CurrCoarsestBrick.Y >= Idx2->NBricks3[Idx2->NLevels - 1].Y)
        {
          CurrCoarsestBrick.Y = 0;
          ++CurrCoarsestBrick.Z;
        }
      }
      if (CurrCoarsestBrick < Idx2->NBricks3[Idx2->NLevels - 1])
      {
        stack_item& Next = Stack[++LastIndex];
        Next.Brick3 = CurrCoarsestBrick;
        Next.Level = Next.ResolutionToSet = Idx2->NLevels - 1;
      }
    }
  }
}


grid
PointQuery(const brick_pool& Bp, const v3i& P3)
{ return grid(); }


f64
Interpolate(const v3i& P3, const grid& Grid)
{
  return 0;
}


} // namespace idx2

