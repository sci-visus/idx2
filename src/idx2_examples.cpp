#define idx2_Implementation // define this only once in your code before including idx2_lib.hpp
#include "idx2_lib.hpp"
#include "stdio.h"

idx2::error<idx2::idx2_file_err_code>
Decode1() {
  idx2::params P;
  P.InputFile = "MIRANDA/VISCOSITY.idx"; // name of data set and field
  P.InDir = "."; // the directory containing the InputFile
  idx2::idx2_file Idx2;
  idx2_CleanUp(Dealloc(&Idx2)); // clean up Idx2 automatically in case of error
  idx2_PropagateIfError(Init(&Idx2, P));

  P.OutputLevel = 0; // Level 0 is the finest, level 1 is half of level 0 in each dimension, etc
  P.DecodeLevel = P.OutputLevel; // most of the time we want this to be the same as OutputLevel
  P.DecodeMask = 128; // controls the exact sub-level to extract (by default is 128)
  // For example, if the volume is 256 x 256 x 256 and there are 2 iterations
  // Level 0, mask 128 = 256 x 256 x 256
  // Level 0, mask 64  = 256 x 256 x 128
  // Level 0, mask 32  = 256 x 128 x 256
  // Level 0, mask 16  = 128 x 256 x 256
  // Level 0, mask 8   = 256 x 128 x 128
  // Level 0, mask 4   = 128 x 256 x 128
  // Level 0, mask 2   = 128 x 128 x 256
  // Level 0, mask 1   = 128 x 128 x 128
  // Level 1, mask 128 = 128 x 128 x 128 (same resolution but slightly less accurate than Level 0, mask 1)
  // ...
  // Level 1, mask 1   =  64 x  64 x  64
  P.DecodeAccuracy = 0.001;
  P.DecodeExtent = idx2::extent(Idx2.Dims3); // get the whole volume
  // P.DecodeExtent = idx2::extent(idx2::v3i(10, 20, 30), idx2::v3i(100, 140, 160)); // get a portion of the whole volume
  idx2::grid OutGrid = idx2::GetOutputGrid(Idx2, P);

  idx2::buffer OutBuf; // buffer to store the output
  idx2_CleanUp(DeallocBuf(&OutBuf)); // deallocate OutBuf automatically in case of error
  idx2::AllocBuf(&OutBuf, idx2::Prod<idx2::i64>(idx2::Dims(OutGrid)) * idx2::SizeOf(Idx2.DType));
  idx2_PropagateIfError(idx2::Decode(&Idx2, P, &OutBuf));

  // uncomment the following lines to write the output to a file
  // FILE* Fp = fopen("out.raw", "wb");
  // idx2_CleanUp(if (Fp) fclose(Fp));
  // fwrite(OutBuf.Data, OutBuf.Bytes, 1, Fp);

  return idx2_Error(idx2::idx2_file_err_code::NoError);
}

int
main() {
  auto Ok = Decode1();
  if (!Ok) {
    fprintf(stderr, ToString(Ok));
    return 1;
  }

  return 0;
}
