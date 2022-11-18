#include "BitOps.h"
#include "idx2SparseBricks.h"
#include "idx2Common.h"
#include "idx2Lookup.h"

namespace idx2
{


void
Init(brick_pool* Bp, idx2_file* Idx2, decode_data* D)
{
  Bp->Idx2 = Idx2;
  i64 NFinestBricks = GetLinearBrick(*Idx2, 0, Idx2->NBricks3[0] - 1);
  AllocBuf(&Bp->ResolutionStream.Stream, (NFinestBricks * 4 + 7) / 8);
  InitWrite(&Bp->ResolutionStream, Bp->ResolutionStream.Stream);
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
  i8 Level = 0;
  v3i Brick3 = v3i(0);
};


// we traverse the brick hierarchy in depth-first order
// if a brick has the AnySignificantChildren flag set, we set the resolution of all its
// descendants to its level (in brick_pool::Resolution)
// and then move to its neighbor on the same level
void
ComputeBrickResolution(brick_pool* Bp)
{
  const idx2_file* Idx2 = Bp->Idx2;
  stack_item Stack[32];
  i8 LastIndex = 0;
  stack_item& First = Stack[LastIndex];
  First.Level = Idx2->NLevels - 1; // coarsest level
  First.Brick3 = v3i(0);
  bool NeedToSetResolution = false;
  i8 ResolutionToSet = -1; // if we get back to this level, reset (we have set all descendants)
  while (LastIndex >= 0)
  {
    // pop the stack
    stack_item Current = Stack[LastIndex--];
    // get the current brick
    u64 BrickIndex = GetLinearBrick(*Idx2, Current.Level, Current.Brick3);
    u64 BrickKey = GetBrickKey(Current.Level, BrickIndex);
    auto BrickIt = Lookup(&Bp->BrickTable, BrickKey);
    // we have traversed all descendants and reached a neighbor brick, reset this
    if (NeedToSetResolution && Current.Level == ResolutionToSet)
    {
      NeedToSetResolution = false;
      ResolutionToSet = -1;
    }
    // if the current brick is the first to be significant in its subtree, set its descendants'
    // level to its own
    if (!BrickIt.Val->AnySignificantChildren && BrickIt.Val->Significant)
    {
      NeedToSetResolution = true;
      ResolutionToSet = Current.Level;
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
          stack_item& Next = Stack[++LastIndex];
          Next.Level = Current.Level - 1;
          Next.Brick3 = ChildBrick3;
        }
      }
    }
    else // finest level, set the level if necessary
    {
      bitstream* Bs = &Bp->ResolutionStream;
      SeekToBit(Bs, BrickIndex * 4); // we use 4 bits to indicate the resolution of the brick
      Write(Bs, ResolutionToSet, 4);
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

