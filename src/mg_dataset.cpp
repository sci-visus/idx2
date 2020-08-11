#include <ctype.h>
#include <stdio.h>
#include "mg_common.h"
#include "mg_dataset.h"
#include "mg_error.h"
#include "mg_filesystem.h"
#include "mg_io.h"
#include "mg_scopeguard.h"
#include "mg_stacktrace.h"
#include "mg_string.h"

namespace mg {

cstr
ToRawFileName(const metadata& Meta) {
  printer Pr(Meta.String, sizeof(Meta.String));
  mg_Print(&Pr, "%s-", Meta.Name);
  mg_Print(&Pr, "%s-", Meta.Field);
  mg_Print(&Pr, "[%d-%d-%d]-", Meta.Dims3.X, Meta.Dims3.Y, Meta.Dims3.Z);
  stref TypeStr = ToString(Meta.DType);
  mg_Print(&Pr, "%.*s.raw", TypeStr.Size, TypeStr.Ptr);
  return Meta.String;
}

cstr
ToString(const metadata& Meta) {
  printer Pr(Meta.String, sizeof(Meta.String));
  mg_Print(&Pr, "file = %s\n", Meta.File);
  mg_Print(&Pr, "name = %s\n", Meta.Name);
  mg_Print(&Pr, "field = %s\n", Meta.Field);
  mg_Print(&Pr, "dimensions = %d %d %d\n", Meta.Dims3.X, Meta.Dims3.Y, Meta.Dims3.Z);
  stref TypeStr = ToString(Meta.DType);
  mg_Print(&Pr, "data type = %.*s", TypeStr.Size, TypeStr.Ptr);
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
    stref FileStr = mg_StRef(Meta->File);
    Copy(FilePath, &FileStr);
    return mg_Error(err_code::NoError);
  }
  return mg_Error(err_code::ParseFailed);
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
  mg_CleanUp(0, DeallocBuf(&Buf));
  stref Str((cstr)Buf.Data, (int)Buf.Bytes);
  tokenizer TkLine(Str, "\r\n");
  for (stref Line = Next(&TkLine); Line; Line = Next(&TkLine)) {
    tokenizer TkEq(Line, "=");
    stref Attr = Trim(Next(&TkEq));
    stref Value = Trim(Next(&TkEq));
    if (!Attr || !Value)
      return mg_Error(err_code::ParseFailed, "File %s", FileName);

    if (Attr == "file") {
      path Path(GetDirName(FileName));
      Append(&Path, Trim(Value));
      stref FileStr = mg_StRef(Meta->File);
      Copy(ToString(Path), &FileStr);
    } else if (Attr == "name") {
      stref NameStr = mg_StRef(Meta->Name);
      Copy(Trim(Value), &NameStr);
    } else if (Attr == "field") {
      stref FieldStr = mg_StRef(Meta->Field);
      Copy(Trim(Value), &FieldStr);
    } else if (Attr == "dimensions") {
      tokenizer TkSpace(Value, " ");
      int D = 0;
      for (stref Dim = Next(&TkSpace); Dim && D < 4; Dim = Next(&TkSpace), ++D) {
        if (!ToInt(Dim, &Meta->Dims3[D]))
          return mg_Error(err_code::ParseFailed, "File %s", FileName);
      }
      if (D >= 4) return mg_Error(err_code::DimensionsTooMany, "File %s", FileName);
      if (D <= 2) Meta->Dims3[2] = 1;
      if (D <= 1) Meta->Dims3[1] = 1;
    } else if (Attr == "type") {
      if ((Meta->DType = StringTo<dtype>()(Value)) == dtype::__Invalid__)
        return mg_Error(err_code::TypeNotSupported, "File %s", FileName);
    } else {
      return mg_Error(err_code::AttributeNotFound, "File %s", FileName);
    }
  }
  return mg_Error(err_code::NoError);
}

} // namespace mg
