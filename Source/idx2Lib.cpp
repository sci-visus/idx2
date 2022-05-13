#include "idx2Lib.h"
#include <stdio.h>

namespace idx2 {

error<idx2_err_code>
Init(idx2_file* Idx2, const params& P) 
{
  SetDir(Idx2, P.InDir);
  idx2_PropagateIfError(ReadMetaFile(Idx2, idx2_PrintScratch("%s", P.InputFile)));
  idx2_PropagateIfError(Finalize(Idx2));
  return idx2_Error(idx2_err_code::NoError);
}

idx2::grid
GetOutputGrid(const idx2_file& Idx2, const params& P) 
{
  u8 OutMask = P.DecodeLevel == P.OutputLevel ? P.DecodeMask : 128; // TODO: check this
  return GetGrid(P.DecodeExtent, P.OutputLevel, OutMask, Idx2.Subbands);
}

error<idx2_err_code>
Decode(idx2_file* Idx2, const params& P, buffer* OutBuf) 
{
  Decode(*Idx2, P, OutBuf);
  return idx2_Error(idx2_err_code::NoError);
}

error<idx2_err_code>
Destroy(idx2_file* Idx2) 
{
  Dealloc(Idx2);
  return idx2_Error(idx2_err_code::NoError);
}

void
Hello()
{
  printf("hello\n");
}

}
