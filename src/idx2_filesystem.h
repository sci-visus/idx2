#pragma once

#include "idx2_common.h"
#include "idx2_string.h"

namespace idx2 {

/* Only support the forward slash '/' separator. */
struct path {
  constexpr static int NPartsMax = 64;
  stref Parts[NPartsMax] = {}; /* e.g. home, dir, file.txt */
  int NParts = 0;
  path();
  path(const stref& Str);
};

void Init(path* Path, const stref& Str);
/* Add a part to the end (e.g. "C:/Users" + "Meow" = "C:/Users/Meow"). */
void Append(path* Path, const stref& Part);
/* Remove the last part, e.g., removing the file name at the end of a path. */
stref GetFileName(const stref& Path);
stref GetDirName(const stref& Path);
cstr ToString(const path& Path);
/* Get the directory where the program is launched from. */
bool IsRelative(const stref& Path);
bool CreateFullDir(const stref& Path);
bool DirExists(const stref& Path);

} // namespace idx2
