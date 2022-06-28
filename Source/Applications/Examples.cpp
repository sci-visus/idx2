//#define idx2_Implementation
//#include "../idx2.hpp"
#include "../idx2Lib.h"
#include "stdio.h"


idx2::error<idx2::idx2_err_code>
Decode1()
{
  idx2::params P;
  P.InputFile = "MIRANDA/VISCOSITY.idx2"; // name of data set and field
  P.InDir = ".";                          // the directory containing the InputFile
  idx2::idx2_file Idx2;
  idx2_CleanUp(Dealloc(&Idx2)); // clean up Idx2 automatically in case of error
  idx2_PropagateIfError(Init(&Idx2, P));

  P.DownsamplingFactor3 = idx2::v3i(1, 1, 1); // Downsample x by 2^1, y by 2^1, z by 2^1
  P.DecodeAccuracy = 0.001;
  P.DecodeExtent = idx2::extent(Idx2.Dims3); // get the whole volume
  // P.DecodeExtent = idx2::extent(idx2::v3i(10, 20, 30), idx2::v3i(100, 140, 160)); // get a portion of the volume
  // portion of the whole volume
  idx2::grid OutGrid = idx2::GetOutputGrid(Idx2, P);

  idx2::buffer OutBuf;               // buffer to store the output
  idx2_CleanUp(DeallocBuf(&OutBuf)); // deallocate OutBuf automatically in case of error
  idx2::AllocBuf(&OutBuf, idx2::Prod<idx2::i64>(idx2::Dims(OutGrid)) * idx2::SizeOf(Idx2.DType));
  idx2_PropagateIfError(idx2::Decode(&Idx2, P, &OutBuf));

  // uncomment the following lines to write the output to a file
  // FILE* Fp = fopen("out.raw", "wb");
  // idx2_CleanUp(if (Fp) fclose(Fp));
  // fwrite(OutBuf.Data, OutBuf.Bytes, 1, Fp);

  return idx2_Error(idx2::idx2_err_code::NoError);
}


int
main()
{
  auto Ok = Decode1();
  if (!Ok)
  {
    fprintf(stderr, "%s\n", ToString(Ok));
    return 1;
  }

  return 0;
}
