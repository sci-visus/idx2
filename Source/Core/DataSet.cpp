#include "DataSet.h"
#include "Common.h"
#include "Error.h"
#include "FileSystem.h"
#include "Format.h"
#include "InputOutput.h"
#include "ScopeGuard.h"
#include "StackTrace.h"
#include "String.h"
#include <ctype.h>
#include <stdio.h>


namespace idx2
{


cstr
ToRawFileName(const metadata& Meta)
{
  printer Pr(Meta.String, sizeof(Meta.String));
  idx2_Print(&Pr, "%s-", Meta.Name);
  idx2_Print(&Pr, "%s-", Meta.Field);
  idx2_Print(&Pr, "[%d-%d-%d]-", Meta.Dims3.X, Meta.Dims3.Y, Meta.Dims3.Z);
  stref TypeStr = ToString(Meta.DType);
  idx2_Print(&Pr, "%.*s", TypeStr.Size, TypeStr.Ptr);

  return Meta.String;
}


cstr
ToString(const metadata& Meta)
{
  printer Pr(Meta.String, sizeof(Meta.String));
  idx2_Print(&Pr, "name = %s\n", Meta.Name);
  idx2_Print(&Pr, "field = %s\n", Meta.Field);
  idx2_Print(&Pr, "dimensions = %d %d %d\n", Meta.Dims3.X, Meta.Dims3.Y, Meta.Dims3.Z);
  stref TypeStr = ToString(Meta.DType);
  idx2_Print(&Pr, "data type = %.*s", TypeStr.Size, TypeStr.Ptr);

  return Meta.String;
}


} // namespace idx2
