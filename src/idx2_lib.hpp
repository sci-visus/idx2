#pragma once

#include "idx2_all.h"

namespace idx2 {

error<idx2_file_err_code>
Decode(idx2_file* Idx2, const params& P, buffer* OutBuf);

}

#ifdef idx2_Implementation

#include "idx2_all.cpp"

namespace idx2 {

/* Need P.InDir, P.InputFile, P.DecodeMask, P.DecodeUpToLevel, P.DecodeAccuracy */
error<idx2_file_err_code>
Decode(idx2_file* Idx2, const params& P, buffer* OutBuf) {
  SetDir(Idx2, P.InDir);
  idx2_PropagateIfError(ReadMetaFile(Idx2, idx2_PrintScratch("%s", P.InputFile)));
  idx2_PropagateIfError(Finalize(Idx2));
  decode_all Dw;
  Dw.Init(*Idx2);
  Dw.SetExtent(P.DecodeExtent);
  Dw.SetMask(P.DecodeMask);
  Dw.SetIteration(P.DecodeUpToLevel);
  Dw.SetAccuracy(P.DecodeAccuracy);
  Decode(*Idx2, P, &Dw);

  return idx2_file_err_code::NoError;
}

} // end namespace idx2

#endif // idx2_Implementation
