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
#include "nd_volume.h"


namespace idx2
{


/*
The TransformTemplate is a string that looks like ':210210:210:210', where each number denotes a dimension.
The transform happens along the dimensions from right to left.
The ':' character denotes level boundary.
The number of ':' characters is the same as the number of levels (which is also Idx2.NLevels).
This is in contrast to v1, in which NLevels is always just 1.
*/
static transform_info_v2
ComputeTransformInfo(const template_view& TemplateView, const nd_size& Dims)
{
  transform_info_v2 TransformInfo;
  nd_size CurrentDims = Dims;
  nd_size ExtrapolatedDims = CurrentDims;
  nd_size CurrentSpacing(1); // spacing
  nd_grid G(Dims);
  i8 Pos = TemplateView.Size - 1;
  i8 NLevels = 0;
  while (Pos >= 0)
  {
    idx2_Assert(Pos >= 0);
    if (TemplateView[Pos--] == ':') // the character ':' (next level)
    {
      G.Spacing = CurrentSpacing;
      G.Dims = CurrentDims;
      ExtrapolatedDims = CurrentDims;
      ++NLevels;
    }
    else // one of 0, 1, 2
    {
      i8 D = TemplateView[Pos--] - '0';
      PushBack(&TransformInfo.Grids, G);
      PushBack(&TransformInfo.Axes, D);
      ExtrapolatedDims[D] = CurrentDims[D] + IsEven(CurrentDims[D]);
      G.Dims = ExtrapolatedDims;
      CurrentDims[D] = (ExtrapolatedDims[D] + 1) / 2;
      CurrentSpacing[D] *= 2;
    }
  }
  TransformInfo.BasisNorms = GetCdf53NormsFast<16>();
  TransformInfo.NLevels = NLevels;

  return TransformInfo;
}


template <typename t> void
FLiftCdf53(const nd_grid& Grid,
           const nd_size& StorageDims,
           i8 d,
           lift_option Option,
           nd_volume* Vol)
{
  // TODO: check alignment when allocating to ensure auto SIMD works
  // TODO: add openmp simd
  nd_size P = MakeFastestDimension(Grid.From   , d);
  nd_size  D = MakeFastestDimension(Grid.Dims   , d);
  nd_size  S = MakeFastestDimension(Grid.Spacing, d);
  nd_size  N = MakeFastestDimension(Vol->Dims   , d);
  nd_size  M = MakeFastestDimension(StorageDims , d);
  // now dimension d is effectively dimension 0
  if (D[0] <= 1)
    return;

  idx2_Assert(M[0] <= N[0]);
  idx2_Assert(IsPow2(S));
  idx2_Assert(IsEven(P[0]));
  idx2_Assert(P[0] + S[0] * (D[0] - 2) < M[0]);

  buffer_t<t> F(Vol->Buffer);
  auto X0 = Min(P[0] + S[0] * D[0], M[0]);       /* extrapolated position */
  auto X1 = Min(P[0] + S[0] * (D[0] - 1), M[0]); /* last position */
  auto X2 = P[0] + S[0] * (D[0] - 2);            /* second last position */
  auto X3 = P[0] + S[0] * (D[0] - 3);            /* third last position */
  bool SignalIsEven = IsEven(D[0]);
  ndOuterLoop(P, P + S * D, S, [&](const nd_size& ndI)
  {
    nd_size MinIdx = Min(ndI, M);
    nd_size SecondLastIdx = SetDimension(MinIdx, 0, X2);
    nd_size LastIdx       = SetDimension(MinIdx, 0, X1);
    nd_size ThirdLastIdx  = SetDimension(MinIdx, 0, X3);
    /* extrapolate if needed */
    if (SignalIsEven)
    {
      idx2_Assert(M[0] < N[0]);
      t A = F[LinearIndex(MinIdx, N)]; /* 2nd last (even) */
      t B = F[LinearIndex(MinIdx, N)]; /* last (odd) */
      /* store the extrapolated value at the boundary position */
      nd_size ExtrapolateIdx = SetDimension(MinIdx, 0, X0);
      F[LinearIndex(ExtrapolateIdx, N)] = 2 * B - A;
    }
    /* predict (excluding last odd position) */
    for (auto X = P[0] + S[0]; X < X2; X += 2 * S[0])
    {
      nd_size MiddleIdx = SetDimension(MinIdx, 0, X);
      nd_size LeftIdx   = SetDimension(MinIdx, 0, X - S[0]);
      nd_size RightIdx  = SetDimension(MinIdx, 0, X + S[0]);
      t& Val = F[LinearIndex(MiddleIdx, N)];
      Val -= (F[LinearIndex(LeftIdx, N)] + F[LinearIndex(RightIdx, N)]) / 2;
    }
    /* predict at the last odd position */
    if (!SignalIsEven)
    {
      t& Val = F[LinearIndex(SecondLastIdx, N)];
      Val -= (F[LinearIndex(LastIdx, N)] + F[LinearIndex(ThirdLastIdx, N)]) / 2;
    }
    else if (X1 < M[0])
    {
      nd_size LastIdx       = SetDimension(MinIdx, 0, X1);
      F[LinearIndex(LastIdx, N)] = 0;
    }
    /* update (excluding last odd position) */
    if (Option != lift_option::NoUpdate)
    {
      /* update excluding the last odd position */
      for (auto X = P[0] + S[0]; X < X2; X += 2 * S[0])
      {
        nd_size MiddleIdx = SetDimension(MinIdx, 0, X);
        nd_size LeftIdx   = SetDimension(MinIdx, 0, X - S[0]);
        nd_size RightIdx  = SetDimension(MinIdx, 0, X + S[0]);
        t Val = F[LinearIndex(MiddleIdx, N)];
        F[LinearIndex(LeftIdx, N)]  += Val / 4;
        F[LinearIndex(RightIdx, N)] += Val / 4;
      }
      /* update at the last odd position */
      if (!SignalIsEven)
      {
        t Val = F[LinearIndex(SecondLastIdx, N)];
        F[LinearIndex(ThirdLastIdx, N)] += Val / 4;
        if (Option == lift_option::Normal)
          F[LinearIndex(LastIdx, N)] += Val / 4;
        else if (Option == lift_option::PartialUpdateLast)
          F[LinearIndex(LastIdx, N)] = Val / 4;
      }
    }
  });
}


// TODO: this function does not make use of PartialUpdateLast
template <typename t> void
ILiftCdf53(const nd_grid& Grid,
           const nd_size& StorageDims,
           i8 d,
           lift_option Option,
           nd_volume* Vol)
{
  nd_size P = MakeFastestDimension(Grid.From, d);
  nd_size D = MakeFastestDimension(Grid.Dims, d);
  nd_size S = MakeFastestDimension(Grid.Spacing, d);
  nd_size N = MakeFastestDimension(Vol->Dims, d);
  nd_size M = MakeFastestDimension(StorageDims, d);
  // now dimension d is effectively dimension 0
  if (D[0] <= 1)
    return;

  idx2_Assert(M[0] <= N[0]);
  idx2_Assert(IsPow2(S));
  idx2_Assert(IsEven(P[0]));
  idx2_Assert(P[0] + S[0] * (D[0] - 2) < M[0]);

  buffer_t<t> F(Vol->Buffer);
  auto X0 = Min(P[0] + S[0] * D[0], M[0]);       /* extrapolated position */
  auto X1 = Min(P[0] + S[0] * (D[0] - 1), M[0]); /* last position */
  auto X2 = P[0] + S[0] * (D[0] - 2);            /* second last position */
  auto X3 = P[0] + S[0] * (D[0] - 3);            /* third last position */
  bool SignalIsEven = IsEven(D[0]);
  ndOuterLoop(P, P + S * D, S, [&](const nd_size& ndI)
  {
    nd_size MinIdx = Min(ndI, M);
    nd_size SecondLastIdx = SetDimension(MinIdx, 0, X2);
    nd_size LastIdx       = SetDimension(MinIdx, 0, X1);
    nd_size ThirdLastIdx  = SetDimension(MinIdx, 0, X3);
    /* inverse update (excluding last odd position) */
    if (Option != lift_option::NoUpdate)
    {
      for (auto X = P[0] + S[0]; X < X2; X += 2 * S[0])
      {
        nd_size MiddleIdx = SetDimension(MinIdx, 0, X);
        nd_size LeftIdx   = SetDimension(MinIdx, 0, X - S[0]);
        nd_size RightIdx  = SetDimension(MinIdx, 0, X + S[0]);
        t Val = F[LinearIndex(MiddleIdx, N)];
        F[LinearIndex(LeftIdx, N)] -= Val / 4;
        F[LinearIndex(RightIdx, N)] -= Val / 4;
      }
      if (!SignalIsEven)
      { /* no extrapolation, inverse update at the last odd position */
        t Val = F[LinearIndex(SecondLastIdx, N)];
        F[LinearIndex(ThirdLastIdx, N)] -= Val / 4;
        if (Option == lift_option::Normal)
          F[LinearIndex(LastIdx, N)] -= Val / 4;
      }
      else
      { /* extrapolation, need to "fix" the last position (odd) */
        nd_size ExtrapolateIdx = SetDimension(MinIdx, 0, X0);
        t A = F[LinearIndex(ExtrapolateIdx, N)];
        t B = F[LinearIndex(SecondLastIdx, N)];
        F[LinearIndex(LastIdx, N)] = (A + B) / 2;
      }
    }
    /* inverse predict (excluding last odd position) */
    for (auto X = P[0] + S[0]; X < X2; X += 2 * S[0])
    {
      nd_size MiddleIdx = SetDimension(MinIdx, 0, X);
      nd_size LeftIdx   = SetDimension(MinIdx, 0, X - S[0]);
      nd_size RightIdx  = SetDimension(MinIdx, 0, X + S[0]);
      t& Val = F[LinearIndex(MiddleIdx, N)];
      Val += (F[LinearIndex(LeftIdx, N)] + F[LinearIndex(RightIdx, N)]) / 2;
    }
    if (!SignalIsEven)
    { /* no extrapolation, inverse predict at the last odd position */
      t& Val = F[LinearIndex(SecondLastIdx, N)];
      Val += (F[LinearIndex(LastIdx, N)] + F[LinearIndex(ThirdLastIdx, N)]) / 2;
    }
  });
}


array<grid>
ComputeTransformGrids(const v3i& Dims3, const template_view& TemplateView, const i8* DimensionMap)
{
  array<grid> TransformGrids;

  v3i CurrentDims3 = Dims3;
  v3i ExtrapolatedDims3 = CurrentDims3;
  v3i CurrentSpacing3(1); // spacing
  grid G(Dims3);
  i8 Pos = TemplateView.Size - 1;
  while (Pos >= 0)
  {
    if (TemplateView[Pos--] == ':') // the character ':' (next level)
    {
      SetSpacing(&G, CurrentSpacing3);
      SetDims(&G, CurrentDims3);
      ExtrapolatedDims3 = CurrentDims3;
    }
    else
    {
      i8 D = DimensionMap[TemplateView[Pos--]];
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
ForwardCdf53(const nd_size& StorageDims,
             const array<subband>& Subbands,
             const array<nd_grid>& TransformGrids,
             const template_view& TemplateView,
             const i8* DimsMap,
             nd_volume* Vol,
             bool CoarsestLevel)
{
  idx2_Assert(Vol->Type == dtype::float64);
  idx2_Assert(Size(TemplateView) == Size(TransformGrids));

  idx2_For (i8, I, 0, Size(TransformGrids))
  {
    i8 D = DimsMap[TemplateView[I]];
    FLiftCdf53<f64>(TransformGrids[I], StorageDims, D, lift_option::Normal, Vol);
  }

  /* Optionally normalize the coefficients */
  idx2_For (i8, Sb, 0, Size(Subbands))
  {
    // TODO NEXT
    //if (Sb == 0 && !CoarsestLevel)
    //  continue; // do not normalize subband 0
    //subband& S = Subbands[Sb];
    //auto ItEnd = End<f64>(S.LocalGrid, *Vol);
    //for (auto It = Begin<f64>(S.LocalGrid, *Vol); It != ItEnd; ++It)
    //  *It *= S.Norm;
  }
}


/* The reason we need to know if the input is on the coarsest level is because we do not want
to normalize subband 0 otherwise */
void
InverseCdf53(const nd_size& StorageDims,
             const array<subband>& Subbands,
             const array<nd_grid>& TransformGrids,
             const template_view& TemplateView,
             const i8* DimsMap,
             nd_volume* Vol,
             bool CoarsestLevel)
{
  /* inverse normalize if required */
  idx2_Assert(Vol->Type == dtype::float64);
  idx2_Assert(Size(TemplateView) == Size(TransformGrids));

  idx2_For (i8, Sb, 0, Size(Subbands))
  {
    // TODO NEXT
    //if (Sb == 0 && !CoarsestLevel)
    //  continue; // do not normalize subband 0
    //subband& S = Subbands[Sb];
    //f64 W = 1.0 / S.Norm;
    //auto ItEnd = End<f64>(S.LocalGrid, *Vol);
    //for (auto It = Begin<f64>(S.LocalGrid, *Vol); It != ItEnd; ++It)
    //  *It *= W;
  }

  /* perform the inverse transform */
  idx2_InclusiveForBackward (i8, I, i8(Size(TransformGrids) - 1), 0)
  {
    int D = DimsMap[TemplateView[I]];
    ILiftCdf53<f64>(TransformGrids[I], StorageDims, D, lift_option::Normal, Vol);
  }
}

/*---------------------------------------------------------------------------------------------
Build subbands for a particular level.
The Template here is understood to be only part of a larger template.
In string form, a Template is made from 4 characters: x,y,z, and :
x, y, and z denotes the axis where the transform happens, while : denotes where the next
level begins (any subsequent transform will be done on the coarsest level subband only).
---------------------------------------------------------------------------------------------*/
array<subband>
BuildLevelSubbands(const template_view& TemplateView,
                   const nd_size& Dims,
                   const nd_size& Spacing)
{
  idx2_Assert(IsPow2(Spacing));

  array<subband> Subbands;

  const auto& WavNorms = GetCdf53NormsFast<MaxNumLevels_>();
  nd_size LogSpacing = Log2Floor(Spacing);

  /* we use a queue to produce subbands by breadth-first subdivision */
  circular_queue<subband, 128> Queue;
  PushBack(&Queue, subband{ nd_grid(Dims), nd_grid(nd_size(0), Dims, Spacing), v6<i8>(0), 0 });
  i8 Pos = Size(TemplateView) - 1;
  while (Pos >= 0)
  {
    i8 D = TemplateView[Pos];
    i8 Sz = i8(Size(Queue));
    for (i8 I = 0; I < Sz; ++I)
    {
      nd_grid_split LocalGridSplits = nd_SplitAlternate(Queue[0].LocalGrid, dimension(D));
      nd_grid_split GlobalGridSplits = nd_SplitAlternate(Queue[0].GlobalGrid, dimension(D));
      v6<i8> NextLowHigh = Queue[0].LowHigh;
      idx2_Assert(NextLowHigh[D] == 0);
      NextLowHigh[D] = 1;
      PushBack(&Queue, subband{ LocalGridSplits.First, GlobalGridSplits.First, Queue[0].LowHigh, 0 });
      PushBack(&Queue, subband{ LocalGridSplits.Second, GlobalGridSplits.Second, NextLowHigh, 0 });
      PopFront(&Queue);
    }
    --Pos;
  }

  /* compute the basis function norms and move the subbands from the queue to the output array */
  i8 Sz = i8(Size(Queue));
  for (i8 I = Sz - 1; I >= 0; --I)
  {
    v3d Weights(1);
    for (i8 Pos = 0; Pos < Size(TemplateView); ++Pos)
    {
      i8 D = TemplateView[Pos];
      if (Queue[I].LowHigh[D] == 0)
        Weights[D] = WavNorms.Scaling[LogSpacing[D]];
      else
        Weights[D] = WavNorms.Wavelet[LogSpacing[D]];
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

