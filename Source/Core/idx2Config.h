#pragma once


#include "Common.h"


namespace idx2
{

static constexpr i8  MaxBitsPerLevel_ = 3;
static constexpr i8  MaxNumSubbandsPerLevel_ = 1 << MaxBitsPerLevel_;
static constexpr i8  MaxNumLevels_ = 16;
static constexpr i8  MaxNumDimensions_ = nd_size::Size();
static constexpr i8  MaxNameLength_ = 64;
static constexpr i16 MaxNumFields_ = 1024;
static constexpr i8  MaxTemplateLength_ = 64;

}
