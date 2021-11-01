#ifndef _IDX2_LIB_H__
#define _IDX2_LIB_H__

#include "idx2_array.h"
#include "idx2_args.h"
#include "idx2_assert.h"
#include "idx2_bitops.h"
#include "idx2_circular_queue.h"
#include "idx2_common.h"
#include "idx2_dataset.h"
#include "idx2_data_types.h"
#include "idx2_debugbreak.h"
#include "idx2_enum.h"
#include "idx2_error.h"
#include "idx2_error_codes.h"
#include "idx2_expected.h"
#include "idx2_filesystem.h"
#include "idx2_file_utils.h"
#include "idx2_hashtable.h"
#include "idx2_io.h"
#include "idx2_linked_list.h"
#include "idx2_logger.h"
#include "idx2_macros.h"
#include "idx2_math.h"
#include "idx2_memory.h"
#include "idx2_memory_map.h"
#include "idx2_mutex.h"
#include "idx2_random.h"
#include "idx2_scopeguard.h"
#include "idx2_function.h"
#include "idx2_stacktrace.h"
#include "idx2_string.h"
#include "idx2_test.h"
#include "idx2_timer.h"
#include "idx2_utils.h"
#include "idx2_wavelet.h"
#include "idx2_v1.h"
#include "idx2_varint.h"
#include "idx2_volume.h"
#include "idx2_zfp.h"

namespace idx2 {
  error<idx2_file_err_code> Init(idx2_file* Idx2, const params& P);

  idx2::grid GetOutputGrid(const idx2_file& Idx2, const params& P);

  error<idx2_file_err_code> Decode(idx2_file* Idx2, const params& P, buffer* OutBuf);

  error<idx2_file_err_code> Destroy(idx2_file* Idx2);

}

//if you don't want to create a lib but just use a single merged header
#ifdef idx2_Implementation
#include "idx2_args.cpp"
#include "idx2_assert.cpp"
#include "idx2_dataset.cpp"
#include "idx2_filesystem.cpp"
#include "idx2_io.cpp"
#include "idx2_logger.cpp"
#include "idx2_math.cpp"
#include "idx2_memory.cpp"
#include "idx2_memory_map.cpp"
#include "idx2_stacktrace.cpp"
#include "idx2_string.cpp"
#include "idx2_utils.cpp"
#include "idx2_v1.cpp"
#include "idx2_v0.cpp"
#include "idx2_varint.cpp"
#include "idx2_volume.cpp"
#include "idx2_zfp.cpp"
#include "idx2_wavelet.cpp"
#include "idx2_lib.cpp"
#endif

#endif // _IDX2_LIB_H__