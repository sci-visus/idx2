#pragma once

#include "Common.h"
#include "Array.h"
#include "BitStream.h"
#include "CircularQueue.h"
#include "DataSet.h"
#include "Error.h"
#include "HashTable.h"
#include "Wavelet.h"
#include "Volume.h"
#include "idx2Common.h"


namespace idx2 {





grid GetGrid(const extent& Ext, int Iter, u8 Mask, const array<subband>& Subbands);

} // namespace idx2
