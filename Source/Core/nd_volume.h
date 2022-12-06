#pragma once

#include "Common.h"
#include "DataTypes.h"
#include "Error.h"
#include "Macros.h"
#include "Memory.h"
#include "MemoryMap.h"


namespace idx2
{


struct nd_extent
{
  nd_index From = nd_index(0);
  nd_size Dims = nd_size(0);
  nd_extent();
  explicit nd_extent(const nd_size& Dims);
  nd_extent(const nd_index& From, const nd_size& Dims);
  operator bool() const;
};


struct nd_grid : public nd_extent
{
  nd_size Spacing = nd_size(0);
  nd_grid();
  explicit nd_grid(const nd_size& Dims);
  nd_grid(const nd_index& From, const nd_size& Dims);
  nd_grid(const nd_index& From, const nd_size& Dims, const nd_size& Spacing);
  explicit nd_grid(const nd_extent& Ext);
  operator bool() const;
};


} // namespace idx2



#include "Assert.h"
#include "BitOps.h"
#include "Math.h"


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
nd_extent::nd_extent(const nd_index& From, const nd_size& Dims)
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
nd_grid::nd_grid(const nd_index& From, const nd_size& Dims)
  : nd_extent(From, Dims)
  , Spacing(1)
{
}


idx2_Inline
nd_grid::nd_grid(const nd_index& From, const nd_size& Dims, const nd_size& Spacing)
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

