#include "Wavelet.h"
#include "Algorithm.h"
#include "Array.h"
#include "Assert.h"
#include "BitOps.h"
#include "Common.h"
#include "DataTypes.h"
#include "Function.h"
#include "Logger.h"
#include "Math.h"
#include "Memory.h"
#include "ScopeGuard.h"
#include "CircularQueue.h"


namespace idx2
{


transform_info
ComputeTransformDetails(const v3i& Dims3, stref Template)
{
  transform_info TransformInfo;
  v3i CurrentDims3 = Dims3;
  v3i ExtrapolatedDims3 = CurrentDims3;
  v3i CurrentSpacing3(1); // spacing
  grid G(Dims3);
  i8 Pos = Template.Size - 1;
  i8 NLevels = 0;
  while (Pos >= 0)
  {
    idx2_Assert(Pos >= 0);
    if (Template[Pos--] == ':') // the character ':' (next level)
    {
      SetStrd(&G, CurrentSpacing3);
      SetDims(&G, CurrentDims3);
      ExtrapolatedDims3 = CurrentDims3;
      ++NLevels;
    }
    else // one of 0, 1, 2
    {
      i8 D = Template[Pos--] - '0';
      PushBack(&TransformInfo.Grids, G);
      PushBack(&TransformInfo.Axes, D);
      ExtrapolatedDims3[D] = CurrentDims3[D] + IsEven(CurrentDims3[D]);
      SetDims(&G, ExtrapolatedDims3);
      CurrentDims3[D] = (ExtrapolatedDims3[D] + 1) / 2;
      CurrentSpacing3[D] *= 2;
    }
  }
  // TODO NEXT
  //TransformInfo.BasisNorms = GetCdf53NormsFast<16>();
  TransformInfo.NLevels = NLevels;

  return TransformInfo;
}


void
Dealloc(transform_info* TInfo)
{
  Dealloc(&TInfo->Grids);
  Dealloc(&TInfo->Axes);
}

void
ForwardCdf53(const v3i& M3,
             i8 Level,
             const array<subband>& Subbands,
             const transform_info& Transform,
             volume* Vol,
             bool CoarsestLevel)
{
  idx2_Assert(Vol->Type == dtype::float64);

  idx2_For (int, I, 0, Size(Transform.Grids))
  {
    int D = Transform.Axes[I];
    if (D == 0)
      FLiftCdf53X<f64>(Transform.Grids[I], M3, lift_option::Normal, Vol);
    else if (D == 1)
      FLiftCdf53Y<f64>(Transform.Grids[I], M3, lift_option::Normal, Vol);
    else if (D == 2)
      FLiftCdf53Z<f64>(Transform.Grids[I], M3, lift_option::Normal, Vol);
  }

  /* Optionally normalize the coefficients */
  for (int I = 0; I < Size(Subbands); ++I)
  {
    if (I == 0 && !CoarsestLevel)
      continue; // do not normalize subband 0

    subband& S = Subbands[I];
    // TODO NEXT
    f64 Wx = M3.X == 1
               ? 1
               : (S.LowHigh3.X == 0 ? Transform.BasisNorms.ScalNorms[Level + S.LowHigh3.X - 1]
                                    : Transform.BasisNorms.WaveNorms[Level + S.LowHigh3.X]);
    f64 Wy = M3.Y == 1
               ? 1
               : (S.LowHigh3.Y == 0 ? Transform.BasisNorms.ScalNorms[Level + S.LowHigh3.Y - 1]
                                    : Transform.BasisNorms.WaveNorms[Level + S.LowHigh3.Y]);
    f64 Wz = M3.Z == 1
               ? 1
               : (S.LowHigh3.Z == 0 ? Transform.BasisNorms.ScalNorms[Level + S.LowHigh3.Z - 1]
                                    : Transform.BasisNorms.WaveNorms[Level + S.LowHigh3.Z]);
    f64 W = Wx * Wy * Wz;

    auto ItEnd = End<f64>(S.Grid, *Vol);
    for (auto It = Begin<f64>(S.Grid, *Vol); It != ItEnd; ++It)
      *It *= W;
  }
}


/* The reason we need to know if the input is on the coarsest level is because we do not want
to normalize subband 0 otherwise */
void
InverseCdf53(const v3i& M3,
             i8 Level,
             const array<subband>& Subbands,
             const transform_info& Transform,
             volume* Vol,
             bool CoarsestLevel)
{
  /* inverse normalize if required */
  idx2_Assert(IsFloatingPoint(Vol->Type));

  for (int I = 0; I < Size(Subbands); ++I)
  {
    if (I == 0 && !CoarsestLevel)
      continue; // do not normalize subband 0
    // TODO NEXT
    subband& S = Subbands[I];
    f64 Wx = M3.X == 1 ? 1
               : (S.LowHigh3.X == 0 ? Transform.BasisNorms.ScalNorms[Level + S.LowHigh3.X - 1]
                                    : Transform.BasisNorms.WaveNorms[Level + S.LowHigh3.X]);
    f64 Wy = M3.Y == 1 ? 1
               : (S.LowHigh3.Y == 0 ? Transform.BasisNorms.ScalNorms[Level + S.LowHigh3.Y - 1]
                                    : Transform.BasisNorms.WaveNorms[Level + S.LowHigh3.Y]);
    f64 Wz = M3.Z == 1 ? 1
               : (S.LowHigh3.Z == 0 ? Transform.BasisNorms.ScalNorms[Level + S.LowHigh3.Z - 1]
                                    : Transform.BasisNorms.WaveNorms[Level + S.LowHigh3.Z]);
    f64 W = 1.0 / (Wx * Wy * Wz);

    auto ItEnd = End<f64>(S.Grid, *Vol);
    for (auto It = Begin<f64>(S.Grid, *Vol); It != ItEnd; ++It)
      *It *= W;
  }

  /* perform the inverse transform */
  for (int I = Size(Transform.Grids) - 1; I >= 0; --I)
  {
    int D = Transform.Axes[I];
    if (D == 0)
      ILiftCdf53X<f64>(Transform.Grids[I], M3, lift_option::Normal, Vol);
    else if (D == 1)
      ILiftCdf53Y<f64>(Transform.Grids[I], M3, lift_option::Normal, Vol);
    else if (D == 2)
      ILiftCdf53Z<f64>(Transform.Grids[I], M3, lift_option::Normal, Vol);
  }
}


/*
In string form, a TransformOrder is made from 4 characters: X,Y,Z, and +
X, Y, and Z denotes the axis where the transform happens, while + denotes where the next
level begins (any subsequent transform will be done on the coarsest level subband only). */
/* Build subbands for a particular level.
The Template here is understood to be only part of a larger template. */
void
BuildSubbandsForOneLevel(const v3i& N3, stref Template, array<subband>* Subbands)
{
  Clear(Subbands);
  // we use a queue to keep track of all the subbands that have been created
  circular_queue<subband, 256> Queue;
  PushBack(&Queue, subband{ grid(N3), v3<i8>(0) });
  v3<i8> MaxLevel3(0);
  stack_array<grid, 32> Grids;
  i8 Pos = Size(Template) - 1;
  while (Pos >= 0)
  {
    if (Template[Pos] == ':') // next level
    {
      // push everything in the queue to the output
      i16 Sz = Size(Queue);
      for (i16 I = Sz - 1; I >= 1; --I)
      {
        PushBack(Subbands, Queue[I]);
        PopBack(&Queue);
      }
    }
    else // transform along one of the dimensions
    {
      int D = Template[Pos] - 'X';
      i16 Sz = Size(Queue);
      for (i16 I = 0; I < Sz; ++I)
      {
        const grid& G = Queue[0].Grid;
        grid_split Gs = SplitAlternate(G, dimension(D));
        v3<i8> NextLowHigh3 = Queue[0].LowHigh3;
        idx2_Assert(NextLowHigh3[D] == 0);
        NextLowHigh3[D] = 1;
        PushBack(&Queue, subband{ Gs.First, Queue[0].LowHigh3 });
        PushBack(&Queue, subband{ Gs.Second, NextLowHigh3 });
        PopFront(&Queue);
      }
    }
  }

  if (Size(Queue) > 0)
    PushBack(Subbands, Queue[0]);

  Reverse(Begin(*Subbands), End(*Subbands));
}


grid
MergeSubbandGrids(const grid& Sb1, const grid& Sb2)
{
  v3i Off3 = Abs(From(Sb2) - From(Sb1));
  v3i Strd3 = Min(Strd(Sb1), Strd(Sb2)) * Equals(Off3, v3i(0)) + Off3;
  // TODO: works in case of subbands but not in general
  v3i Dims3 = Dims(Sb1) + NotEquals(From(Sb1), From(Sb2)) * Dims(Sb2);
  v3i From3 = Min(From(Sb2), From(Sb1));
  return grid(From3, Dims3, Strd3);
}


} // namespace idx2

