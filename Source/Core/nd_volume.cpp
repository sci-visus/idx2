#include "Volume.h"
#include "Assert.h"
#include "InputOutput.h"
#include "Math.h"
#include "MemoryMap.h"
#include "ScopeGuard.h"
#include "nd_volume.h"


namespace idx2
{


nd_grid_split
nd_sSplit(const nd_grid& Grid, dimension D, int N)
{
  nd_size Dims = idx2::Dims(Grid);
  idx2_Assert(N <= Dims[D] && N >= 0);
  nd_grid_split GridSplit{ Grid, Grid };
  nd_size Mask = Dims;
  Mask[D] = N;
  SetDims(&GridSplit.First, Mask);
  Mask[D] = Dims[D] - N;
  SetDims(&GridSplit.Second, Mask);
  nd_size From = idx2::From(Grid);
  From[D] += N * Spacing(Grid)[D];
  SetFrom(&GridSplit.Second, From);
  return GridSplit;
}


nd_grid_split
nd_SplitAlternate(const nd_grid& Grid, dimension D)
{
  nd_size Dims = idx2::Dims(Grid);
  nd_size Spacing = idx2::Spacing(Grid);
  nd_size From = idx2::From(Grid);
  From[D] += Spacing[D];
  nd_grid_split GridSplit{ Grid, Grid };
  nd_size Mask = Dims;
  Mask[D] = (Dims[D] + 1) >> 1;
  SetDims(&GridSplit.First, Mask);
  nd_size NewSpacing = Spacing;
  NewSpacing[D] <<= 1;
  SetSpacing(&GridSplit.First, NewSpacing);
  Mask[D] = Dims[D] - Mask[D];
  SetDims(&GridSplit.Second, Mask);
  SetSpacing(&GridSplit.Second, NewSpacing);
  SetFrom(&GridSplit.Second, From);
  return GridSplit;
}


} // namespace idx2

