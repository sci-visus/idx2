#pragma once

#include "Common.h"
#include "String.h"

namespace idx2
{

/* Only support the forward slash '/' separator. */
struct path
{
  constexpr static int NPartsMax = 64;
  stref Parts[NPartsMax] = {}; /* e.g. home, dir, file.txt */
  int NParts = 0;
  path();
  path(const stref& Str);
};

void
Init
(path* Path, const stref& Str);

/* Add a part to the end (e.g. "C:/Users" + "Meow" = "C:/Users/Meow"). */
void
Append
(path* Path, const stref& Part);

/* Get the file name from a path */
stref
GetFileName
(const stref& Path);

/* Remove the last part, e.g., removing the file name at the end of a path. */
stref
GetDirName
(const stref& Path);

cstr
ToString
(const path& Path);

/* Get the directory where the program is launched from. */
bool
IsRelative
(const stref& Path);

bool
CreateFullDir
(const stref& Path);

bool
DirExists
(const stref& Path);

void
RemoveDir
(cstr path);

stref
GetExtension
(const stref& Path);

} // namespace idx2

