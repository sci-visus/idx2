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


array<grid>
ComputeTransformGrids(const v3i& Dims3, stref Template, const i8* DimensionMap)
{
  array<grid> TransformGrids;

  v3i CurrentDims3 = Dims3;
  v3i ExtrapolatedDims3 = CurrentDims3;
  v3i CurrentSpacing3(1); // spacing
  grid G(Dims3);
  i8 Pos = Template.Size - 1;
  while (Pos >= 0)
  {
    if (Template[Pos--] == ':') // the character ':' (next level)
    {
      SetSpacing(&G, CurrentSpacing3);
      SetDims(&G, CurrentDims3);
      ExtrapolatedDims3 = CurrentDims3;
    }
    else
    {
      i8 D = DimensionMap[Template[Pos--]];
      PushBack(&TransformGrids, G);
      ExtrapolatedDims3[D] = CurrentDims3[D] + IsEven(CurrentDims3[D]);
      SetDims(&G, ExtrapolatedDims3);
      CurrentDims3[D] = (ExtrapolatedDims3[D] + 1) / 2;
      CurrentSpacing3[D] *= 2;
    }
  }

  return TransformGrids;
}


void
ForwardCdf53(const v3i& M3,
             const array<subband>& Subbands,
             const array<grid>& TransformGrids,
             stref Template,
             const i8* DimensionMap,
             volume* Vol,
             bool CoarsestLevel)
{
  idx2_Assert(Vol->Type == dtype::float64);
  idx2_Assert(Size(Template) == Size(TransformGrids));

  idx2_For (i8, I, 0, Size(TransformGrids))
  {
    int D = DimensionMap[Template[I]];
    if (D == 0)
      FLiftCdf53X<f64>(TransformGrids[I], M3, lift_option::Normal, Vol);
    else if (D == 1)
      FLiftCdf53Y<f64>(TransformGrids[I], M3, lift_option::Normal, Vol);
    else if (D == 2)
      FLiftCdf53Z<f64>(TransformGrids[I], M3, lift_option::Normal, Vol);
  }

  /* Optionally normalize the coefficients */
  idx2_For (i8, Sb, 0, Size(Subbands))
  {
    if (Sb == 0 && !CoarsestLevel)
      continue; // do not normalize subband 0
    subband& S = Subbands[Sb];
    auto ItEnd = End<f64>(S.LocalGrid, *Vol);
    for (auto It = Begin<f64>(S.LocalGrid, *Vol); It != ItEnd; ++It)
      *It *= S.Norm;
  }
}


/* The reason we need to know if the input is on the coarsest level is because we do not want
to normalize subband 0 otherwise */
void
InverseCdf53(const v3i& M3,
             i8 Level,
             const array<subband>& Subbands,
             const array<grid>& TransformGrids,
             stref Template,
             const i8* DimensionMap,
             volume* Vol,
             bool CoarsestLevel)
{
  /* inverse normalize if required */
  idx2_Assert(Vol->Type == dtype::float64);
  idx2_Assert(Size(Template) == Size(TransformGrids));

  idx2_For (i8, Sb, 0, Size(Subbands))
  {
    if (Sb == 0 && !CoarsestLevel)
      continue; // do not normalize subband 0
    subband& S = Subbands[Sb];
    f64 W = 1.0 / S.Norm;
    auto ItEnd = End<f64>(S.LocalGrid, *Vol);
    for (auto It = Begin<f64>(S.LocalGrid, *Vol); It != ItEnd; ++It)
      *It *= W;
  }

  /* perform the inverse transform */
  idx2_InclusiveForBackward (i8, I, Size(TransformGrids) - 1, 0)
  {
    int D = DimensionMap[Template[I]];
    if (D == 0)
      ILiftCdf53X<f64>(TransformGrids[I], M3, lift_option::Normal, Vol);
    else if (D == 1)
      ILiftCdf53Y<f64>(TransformGrids[I], M3, lift_option::Normal, Vol);
    else if (D == 2)
      ILiftCdf53Z<f64>(TransformGrids[I], M3, lift_option::Normal, Vol);
  }
}

/* Build subbands for a particular level.
The Template here is understood to be only part of a larger template.
In string form, a Template is made from 4 characters: x,y,z, and :
x, y, and z denotes the axis where the transform happens, while : denotes where the next
level begins (any subsequent transform will be done on the coarsest level subband only). */
array<subband>
BuildLevelSubbands(stref Template, const i8* DimensionMap, const v3i& Dims3, const v3i& Spacing3)
{
  idx2_Assert(IsPow2(Spacing3));

  array<subband> Subbands;

  const auto& WavNorms = GetCdf53NormsFast<32>();
  v3i LogSpacing3 = Log2Floor(Spacing3);

  /* we use a queue to produce subbands by breadth-first subdivision */
  circular_queue<subband, 128> Queue;
  PushBack(&Queue, subband{ grid(Dims3), grid(v3i(0), Dims3, Spacing3), v3<i8>(0), 0 });
  i8 Pos = Size(Template) - 1;
  while (Pos >= 0)
  {
    i8 D = DimensionMap[Template[Pos]];
    i8 Sz = Size(Queue);
    for (i8 I = 0; I < Sz; ++I)
    {
      grid_split LocalGridSplits = SplitAlternate(Queue[0].LocalGrid, dimension(D));
      grid_split GlobalGridSplits = SplitAlternate(Queue[0].GlobalGrid, dimension(D));
      v3<i8> NextLowHigh3 = Queue[0].LowHigh3;
      idx2_Assert(NextLowHigh3[D] == 0);
      NextLowHigh3[D] = 1;
      PushBack(&Queue, subband{ LocalGridSplits.First, GlobalGridSplits.First, Queue[0].LowHigh3, 0 });
      PushBack(&Queue, subband{ LocalGridSplits.Second, GlobalGridSplits.Second, NextLowHigh3, 0 });
      PopFront(&Queue);
    }
    --Pos;
  }

  /* compute the basis function norms and move the subbands from the queue to the output array */
  i8 Sz = Size(Queue);
  for (i8 I = Sz - 1; I >= 0; --I)
  {
    v3d Weights(1);
    for (i8 Pos = 0; Pos < Size(Template); ++Pos)
    {
      i8 D = DimensionMap[Template[Pos]];
      if (Queue[I].LowHigh3[D] == 0)
        Weights[D] = WavNorms.Scaling[LogSpacing3[D]];
      else
        Weights[D] = WavNorms.Wavelet[LogSpacing3[D]];
    }
    Queue[I].Norm = Prod<f64>(Weights);
    PushBack(&Subbands, Queue[I]);
    PopBack(&Queue);
  }

  Reverse(Begin(Subbands), End(Subbands));

  return Subbands;
}


grid
MergeSubbandGrids(const grid& Sb1, const grid& Sb2)
{
  v3i Off3 = Abs(From(Sb2) - From(Sb1));
  v3i Spacing3 = Min(Spacing(Sb1), Spacing(Sb2)) * Equals(Off3, v3i(0)) + Off3;
  // TODO: works in case of subbands but not in general
  v3i Dims3 = Dims(Sb1) + NotEquals(From(Sb1), From(Sb2)) * Dims(Sb2);
  v3i From3 = Min(From(Sb2), From(Sb1));
  return grid(From3, Dims3, Spacing3);
}


} // namespace idx2

