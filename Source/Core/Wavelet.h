#pragma once

#include "Array.h"
#include "Common.h"
#include "Volume.h"
#include "nd_volume.h"


namespace idx2
{


template <int N> struct wavelet_basis_norms
{
  stack_array<f64, N> Scaling;
  stack_array<f64, N> Wavelet;
};


struct transform_info_v2
{
  wavelet_basis_norms<16> BasisNorms;
  array<nd_grid> Grids;
  array<i8> Axes;
  i8 NLevels;
};


template <int N> const wavelet_basis_norms<N>&
GetCdf53NormsFast()
{
  static wavelet_basis_norms<N> Result;
  f64 Num1 = 3, Num2 = 23;
  for (int I = 0; I < N; ++I)
  {
    Result.Scaling[I] = sqrt(Num1 / (1 << (I + 1)));
    Num1 = Num1 * 4 - 1;
    Result.Wavelet[I] = sqrt(Num2 / (1 << (I + 5)));
    Num2 = Num2 * 4 - 33;
  }
  return Result;
}


struct subband
{
  nd_grid LocalGrid;
  nd_grid GlobalGrid;
  v6<i8> LowHigh;
  f64 Norm; // l2 norm of the wavelet basis function
};


/*
New set of lifting functions. We assume the volume where we want to transform
to happen (M) is contained within a bigger volume (Vol). When Dims(Grid) is even,
extrapolation will happen, in a way that the last (odd) wavelet coefficient is 0.
We assume the storage at index M3.(x/y/z) is available to store the extrapolated
values if necessary. We could always store the extrapolated value at the correct
position, but storing it at M3.(x/y/z) allows us to avoid having to actually use
extra storage (which are mostly used to store 0 wavelet coefficients).
*/
enum lift_option
{
  Normal,
  PartialUpdateLast,
  NoUpdateLast,
  NoUpdate
};


array<grid>
ComputeTransformGrids(const v3i& Dims3, stref Template, const i8* DimensionMap);


void
ForwardCdf53(const v3i& M3,
             const array<subband>& Subbands,
             const array<grid>& TransformGrids,
             stref Template,
             const i8* DimensionMap,
             volume* Vol,
             bool CoarsestLevel);

void
InverseCdf53(const v3i& M3,
             i8 Level,
             const array<subband>& Subbands,
             const array<grid>& TransformGrids,
             stref Template,
             const i8* DimsMap,
             volume* Vol,
             bool CoarsestLevel);


array<subband>
BuildLevelSubbands(stref Template,
                   const i8* DimensionMap,
                   const nd_size& Dims,
                   const nd_size& Spacing);

} // namespace idx2

