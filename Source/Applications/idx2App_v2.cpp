#include "../idx2_v2.h"


using namespace idx2;


static void
DoEncode(int Argc, cstr* Argv, cstr InputFile)
{
  idx2_RAII(idx2_file_v2, Idx2);

  /* Parse the dimensions */
  if (!OptVal(Argc, Argv, "--dimensions", &Idx2.Dimensions))
    idx2_Exit("Provide --dimensions, followed by x 384 y 384 z 256, for example\n");

  if (!OptVal(Argc, Argv, "--template", &Idx2.IdxTemplate.Full))
    idx2_Exit("Provide indexing --template, e.g., zyzyxzyxzyxzyx|zyx:zyx:zyx:zyx\n");

  array<stref> Fields;
  if (OptVal(Argc, Argv, "--fields", &Fields))
  {
    dimension_info Dim;
    {
      Dim.Names = Fields;
      Dim.ShortName = 'f'; // TODO: hardcoding
    }
    PushBack(&Idx2.Dimensions, Dim);
  }

  idx2_ExitIfError(Finalize(&Idx2));
}


static void
DoDecode(int Argc, cstr* Argv, cstr InputFile)
{
}


int
main(int Argc, cstr* Argv)
{
  SetHandleAbortSignals();

  CheckForUnsupportedOpt(Argc, Argv,
                         "--encode "
                         "--decode "
                         "--dimensions "
                         "--template "
                         "--fields ");

  /* Parse the action (--encode or --decode) */
  cstr InputFile = nullptr;
  if (OptVal(Argc, Argv, "--encode", &InputFile))
    DoEncode(Argc, Argv, InputFile);
  else if (OptVal(Argc, Argv, "--decode", &InputFile))
    DoDecode(Argc, Argv, InputFile);
  else
    idx2_Exit("Provide either --encode or --decode, followed by a file name\n");

  return 0;
}

