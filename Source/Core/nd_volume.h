#pragma once

#include "Common.h"
#include "DataTypes.h"
#include "Error.h"
#include "Memory.h"
#include "MemoryMap.h"

/* -------------------------------- TYPES -------------------------------- */

namespace idx2
{


struct nd_extent
{
  nd_size From = nd_size(0);
  nd_size Dims = nd_size(0);

  nd_extent();
  explicit nd_extent(const nd_size& Dims);
  nd_extent(const nd_size& From, const nd_size& Dims);
  operator bool() const;
};


struct nd_grid : public nd_extent
{
  nd_size Spacing = nd_size(0);

  nd_grid();
  explicit nd_grid(const nd_size& Dims);
  nd_grid(const nd_size& From, const nd_size& Dims);
  nd_grid(const nd_size& From, const nd_size& Dims, const nd_size& Spacing);
  explicit nd_grid(const nd_extent& Ext);
  operator bool() const;
};


struct nd_volume
{
  buffer Buffer = {};
  nd_size Dims = nd_size(0);
  dtype Type = dtype::__Invalid__;

  nd_volume();
  nd_volume(const buffer& Buf, const nd_size& Dims, dtype Type);
  nd_volume(const nd_size& Dims, dtype Type, allocator* Alloc = &Mallocator());
};


} // namespace idx2



#include "Assert.h"
#include "BitOps.h"
#include "Math.h"


/* -------------------------------- METHODS -------------------------------- */

namespace idx2
{

idx2_Inline
nd_extent::nd_extent() = default;


idx2_Inline
nd_extent::nd_extent(const nd_size& Dims)
  : From(0)
  , Dims(Dims)
{
}


idx2_Inline
nd_extent::nd_extent(const nd_size& From, const nd_size& Dims)
  : From(From)
  , Dims(Dims)
{
}


idx2_Inline
nd_extent::operator bool() const
{
  return Dims > 0;
}


idx2_Inline
nd_grid::nd_grid() = default;


idx2_Inline
nd_grid::nd_grid(const nd_size& Dims)
  : nd_extent(Dims)
  , Spacing(1)
{
}


idx2_Inline
nd_grid::nd_grid(const nd_size& From, const nd_size& Dims)
  : nd_extent(From, Dims)
  , Spacing(1)
{
}


idx2_Inline
nd_grid::nd_grid(const nd_size& From, const nd_size& Dims, const nd_size& Spacing)
  : nd_extent(From, Dims)
  , Spacing(Spacing)
{
}


idx2_Inline
nd_grid::nd_grid(const nd_extent& Ext)
  : nd_extent(Ext)
  , Spacing(1)
{
}


idx2_Inline
nd_grid::operator bool() const
{
  return Dims > 0;
}


} // namespace idx2


/* -------------------------------- FREE FUNCTIONS -------------------------------- */

namespace idx2
{


idx2_Inline i64
LinearIndex(const nd_size& ndIdx, const nd_size& Dims)
{
  idx2_Assert(ndIdx < Dims);
  i64 Stride = 1;
  i64 LinearIdx = 0;
  idx2_For (i8, D, 0, Dims.Size())
  {
    LinearIdx += ndIdx[D] * Stride;
    Stride *= Dims[D];
  }
  return LinearIdx;
}


idx2_Inline nd_size
From(const nd_extent& Ext)
{
  return Ext.From;
}


idx2_Inline void
SetFrom(nd_extent* Ext, const nd_size& From)
{
  Ext->From = From;
}


idx2_Inline nd_size
Dims(const nd_extent& Ext)
{
  return Ext.Dims;
}


idx2_Inline void
SetDims(nd_extent* Ext, const nd_size& Dims)
{
  Ext->Dims = Dims;
}


idx2_Inline nd_size
Spacing(const nd_extent& Ext)
{
  (void)Ext;
  return nd_size(1);
}


idx2_Inline nd_size
To(const nd_extent& Ext)
{
  return Ext.From + Ext.Dims;
}


idx2_Inline nd_size
First(const nd_extent& Ext)
{
  return From(Ext);
}


idx2_Inline nd_size
Last(const nd_extent& Ext)
{
  return To(Ext) - 1;
}


idx2_Inline nd_size
From(const nd_grid& Grid)
{
  return Grid.From;
}


idx2_Inline void
SetFrom(nd_grid* Grid, const nd_size& From)
{
  Grid->From = From;
}


idx2_Inline nd_size
Dims(const nd_grid& Grid)
{
  return Grid.Dims;
}


idx2_Inline void
SetDims(nd_grid* Grid, const nd_size& Dims)
{
  Grid->Dims = Dims;
}


idx2_Inline nd_size
Spacing(const nd_grid& Grid)
{
  return Grid.Spacing;
}


idx2_Inline void
SetSpacing(nd_grid* Grid, const nd_size& Spacing)
{
  Grid->Spacing = Spacing;
}


idx2_Inline nd_size
To(const nd_grid& Grid)
{
  return From(Grid) + Dims(Grid) * Spacing(Grid);
}


idx2_Inline nd_size
First(const nd_grid& Grid)
{
  return From(Grid);
}


idx2_Inline nd_size
Last(const nd_grid& Grid)
{
  return To(Grid) - Spacing(Grid);
}


template <typename t1, typename t2> t1
nd_Crop(const t1& Grid1, const t2& Grid2)
{
  nd_size Spacing = idx2::Spacing(Grid1);
  nd_size Grid1First = idx2::First(Grid1);
  nd_size First = Max(Grid1First, idx2::First(Grid2));
  nd_size Last = Min(idx2::Last(Grid1), idx2::Last(Grid2));
  First = ((First - Grid1First + Spacing - 1) / Spacing) * Spacing + Grid1First;
  Last = ((Last - Grid1First) / Spacing) * Spacing + Grid1First;
  t1 OutGrid = Grid1;
  OutGrid.From = First;
  OutGrid.Dims = First <= Last ? (Last - First) / Spacing + 1 : nd_size(0);
  return OutGrid;
}


struct nd_grid_split
{
  nd_grid First, Second;
};

nd_grid_split
nd_Split(const nd_grid& Grid, dimension D, int N);

nd_grid_split
nd_SplitAlternate(const nd_grid& Grid, dimension D);


} // namespace idx2

