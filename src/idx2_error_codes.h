#pragma once

#include "idx2_enum.h"

#define idx2_CommonErrs\
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

idx2_Enum(err_code, int, idx2_CommonErrs)
