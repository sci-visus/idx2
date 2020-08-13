#include <ctype.h>
#include <stdio.h>
#include "idx2_common.h"
#include "idx2_dataset.h"
#include "idx2_error.h"
#include "idx2_filesystem.h"
#include "idx2_io.h"
#include "idx2_scopeguard.h"
#include "idx2_stacktrace.h"
#include "idx2_string.h"

namespace idx2 {

cstr
ToRawFileName(const metadata& Meta) {
  printer Pr(Meta.String, sizeof(Meta.String));
  idx2_Print(&Pr, "%s-", Meta.Name);
  idx2_Print(&Pr, "%s-", Meta.Field);
  idx2_Print(&Pr, "[%d-%d-%d]-", Meta.Dims3.X, Meta.Dims3.Y, Meta.Dims3.Z);
  stref TypeStr = ToString(Meta.DType);
  idx2_Print(&Pr, "%.*s.raw", TypeStr.Size, TypeStr.Ptr);
  return Meta.String;
}

cstr
ToString(const metadata& Meta) {
  printer Pr(Meta.String, sizeof(Meta.String));
  idx2_Print(&Pr, "file = %s\n", Meta.File);
  idx2_Print(&Pr, "name = %s\n", Meta.Name);
  idx2_Print(&Pr, "field = %s\n", Meta.Field);
  idx2_Print(&Pr, "dimensions = %d %d %d\n", Meta.Dims3.X, Meta.Dims3.Y, Meta.Dims3.Z);
  stref TypeStr = ToString(Meta.DType);
  idx2_Print(&Pr, "data type = %.*s", TypeStr.Size, TypeStr.Ptr);
  return Meta.String;
}

/* MIRANDA-DENSITY-[96-96-96]-Float64.raw */
error<>
ParseMeta(stref FilePath, metadata* Meta) {
  stref FileName = GetFileName(FilePath);
  char Type[8];
  if (6 == sscanf(FileName.ConstPtr, "%[^-]-%[^-]-[%d-%d-%d]-%[^.]", Meta->Name,
                  Meta->Field, &Meta->Dims3.X, &Meta->Dims3.Y, &Meta->Dims3.Z, Type))
  {
    Type[0] = (char)tolower(Type[0]);
    Meta->DType = StringTo<dtype>()(stref(Type));
    stref FileStr = idx2_StRef(Meta->File);
    Copy(FilePath, &FileStr);
    return idx2_Error(err_code::NoError);
  }
  return idx2_Error(err_code::ParseFailed);
}

/*
file = MIRANDA-DENSITY-[96-96-96]-Float64.raw
name = MIRANDA
field = DATA
dimensions = 96 96 96
type = float64 */
error<>
ReadMeta(cstr FileName, metadata* Meta) {
  buffer Buf;
  error Ok = ReadFile(FileName, &Buf);
  if (Ok.Code != err_code::NoError)
    return Ok;
  idx2_CleanUp(0, DeallocBuf(&Buf));
  stref Str((cstr)Buf.Data, (int)Buf.Bytes);
  tokenizer TkLine(Str, "\r\n");
  for (stref Line = Next(&TkLine); Line; Line = Next(&TkLine)) {
    tokenizer TkEq(Line, "=");
    stref Attr = Trim(Next(&TkEq));
    stref Value = Trim(Next(&TkEq));
    if (!Attr || !Value)
      return idx2_Error(err_code::ParseFailed, "File %s", FileName);

    if (Attr == "file") {
      path Path(GetDirName(FileName));
      Append(&Path, Trim(Value));
      stref FileStr = idx2_StRef(Meta->File);
      Copy(ToString(Path), &FileStr);
    } else if (Attr == "name") {
      stref NameStr = idx2_StRef(Meta->Name);
      Copy(Trim(Value), &NameStr);
    } else if (Attr == "field") {
      stref FieldStr = idx2_StRef(Meta->Field);
      Copy(Trim(Value), &FieldStr);
    } else if (Attr == "dimensions") {
      tokenizer TkSpace(Value, " ");
      int D = 0;
      for (stref Dim = Next(&TkSpace); Dim && D < 4; Dim = Next(&TkSpace), ++D) {
        if (!ToInt(Dim, &Meta->Dims3[D]))
          return idx2_Error(err_code::ParseFailed, "File %s", FileName);
      }
      if (D >= 4) return idx2_Error(err_code::DimensionsTooMany, "File %s", FileName);
      if (D <= 2) Meta->Dims3[2] = 1;
      if (D <= 1) Meta->Dims3[1] = 1;
    } else if (Attr == "type") {
      if ((Meta->DType = StringTo<dtype>()(Value)) == dtype::__Invalid__)
        return idx2_Error(err_code::TypeNotSupported, "File %s", FileName);
    } else {
      return idx2_Error(err_code::AttributeNotFound, "File %s", FileName);
    }
  }
  return idx2_Error(err_code::NoError);
}

} // namespace idx2
