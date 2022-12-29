#include "idx2.h"
#include <stdio.h>


namespace idx2
{

error<err_code>
InitFromBuffer(idx2_file* Idx2, params& P, buffer& Buf)
{
  //// TODO NEXT
  ////SetDir(Idx2, P.InDir);
  ////SetDownsamplingFactor(Idx2, P.DownsamplingFactor3);
  //idx2_PropagateIfError(ReadMetaFileFromBuffer(Idx2, Buf));
  //idx2_PropagateIfError(Finalize(Idx2, &P));
  //if (Dims(P.DecodeExtent) == v3i(0)) // TODO: this could conflate with the user wanting to decode a
  //                                    // single sample (very unlikely though)
  //  P.DecodeExtent = extent(Idx2->Dims3);
  return idx2_Error(err_code::NoError);
}


error<err_code>
Init(idx2_file* Idx2, params& P)
{
  //// TODO NEXT
  ////SetDir(Idx2, P.InDir);
  ////SetDownsamplingFactor(Idx2, P.DownsamplingFactor3);
  //idx2_PropagateIfError(ReadMetaFile(Idx2, idx2_PrintScratch("%s", P.InputFile)));
  //idx2_PropagateIfError(Finalize(Idx2, &P));
  //if (Dims(P.DecodeExtent) == v3i(0)) // TODO: this could conflate with the user wanting to decode a single sample (very unlikely though)
  //  P.DecodeExtent = extent(Idx2->Dims3);
  return idx2_Error(err_code::NoError);
}


idx2::grid
GetOutputGrid(const idx2_file& Idx2, const params& P)
{
  //return GetGrid(Idx2, P.DecodeExtent);
  return grid();
}


error<err_code>
Decode(idx2_file* Idx2, params& P, buffer* OutBuf)
{
  Decode(*Idx2, P, OutBuf);
  return idx2_Error(err_code::NoError);
}


error<err_code>
Destroy(idx2_file* Idx2)
{
  Dealloc(Idx2);
  return idx2_Error(err_code::NoError);
}


} // namespace idx2
