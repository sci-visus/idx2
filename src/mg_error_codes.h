#pragma once

#include "mg_enum.h"

#define mg_CommonErrs\
  NoError, UnknownError,\
  SizeZero, SizeTooSmall, SizeMismatched,\
  DimensionMismatched, DimensionsTooMany,\
  AttributeNotFound,\
  OptionNotSupported,\
  TypeNotSupported,\
  FileCreateFailed, FileReadFailed, FileWriteFailed, FileOpenFailed,\
  FileCloseFailed, FileSeekFailed, FileTellFailed,\
  ParseFailed,\
  OutOfMemory

mg_Enum(err_code, int, mg_CommonErrs)
