#pragma once

#include "mg_common.h"

namespace mg {

u32 Murmur3_32(u8* Key, int Len, u32 Seed);

} // namespace mg
