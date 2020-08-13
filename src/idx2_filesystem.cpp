#include <string.h>
#include "idx2_assert.h"
#include "idx2_algorithm.h"
#include "idx2_filesystem.h"
#include "idx2_io.h"

#if defined(_WIN32)
  #include <direct.h>
  #include <io.h>
  #include <Windows.h>
  #define GetCurrentDir _getcwd
  #define MkDir(Dir) _mkdir(Dir)
  #define Access(Dir) _access(Dir, 0)
#elif defined(__linux__) || defined(__APPLE__)
  #include <sys/stat.h>
  #include <unistd.h>
  #define GetCurrentDir getcwd
  #define MkDir(Dir) mkdir(Dir, 0733)
  #define Access(Dir) access(Dir, F_OK)
#endif

namespace idx2 {

path::
path() = default;

path::
path(const stref& Str) { Init(this, Str); }

void
Init(path* Path, const stref& Str) {
  Path->Parts[0] = Str;
  Path->NParts = 1;
}

void Append(path* Path, const stref& Part) {
  idx2_Assert(Path->NParts < Path->NPartsMax, "too many path parts");
  Path->Parts[Path->NParts++] = Part;
}

stref
GetFileName(const stref& Path) {
  idx2_Assert(!Contains(Path, '\\'));
  cstr LastSlash = FindLast(RevBegin(Path), RevEnd(Path), '/');
  if (LastSlash != RevEnd(Path))
    return SubString(Path, int(LastSlash - Begin(Path) + 1),
                     Path.Size - int(LastSlash - Begin(Path)));
  return Path;
}

stref
GetDirName(const stref& Path) {
  idx2_Assert(!Contains(Path, '\\'));
  cstr LastSlash = FindLast(RevBegin(Path), RevEnd(Path), '/');
  if (LastSlash != RevEnd(Path))
    return SubString(Path, 0, int(LastSlash - Begin(Path)));
  return Path;
}

cstr
ToString(const path& Path) {
  printer Pr(ScratchBuf, sizeof(ScratchBuf));
  for (int I = 0; I < Path.NParts; ++I) {
    idx2_Print(&Pr, "%.*s", Path.Parts[I].Size, Path.Parts[I].Ptr);
    if (I + 1 < Path.NParts)
      idx2_Print(&Pr, "/");
  }
  return ScratchBuf;
}

bool
IsRelative(const stref& Path) {
  stref& PathR = const_cast<stref&>(Path);
  if (PathR.Size > 0 && PathR[0] == '/')  // e.g. /usr/local
    return false;
  if (PathR.Size > 2 && PathR[1] == ':' && PathR[2] == '/')  // e.g. C:/Users
    return false;
  return true;
}

bool
CreateFullDir(const stref& Path) {
  cstr PathCopy = ToString(Path);
  int Error = 0;
  str P = (str)PathCopy;
  for (P = (str)strchr(PathCopy, '/'); P; P = (str)strchr(P + 1, '/')) {
    *P = '\0';
    Error = MkDir(PathCopy);
    *P = '/';
  }
  Error = MkDir(PathCopy);
  return (Error == 0);
}

bool
DirExists(const stref& Path) {
  cstr PathCopy = ToString(Path);
  return Access(PathCopy) == 0;
}

} // namespace idx2

#undef GetCurrentDir
#undef MkDir
#undef Access
