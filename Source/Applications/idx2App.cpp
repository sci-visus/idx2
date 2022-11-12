//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

//#define idx2_Implementation
//#include "../idx2.hpp"
#include "../idx2.h"


using namespace idx2;


/* Parse the decode options */
static void
ParseDecodeOptions(int Argc, cstr* Argv, params* P)
{
  idx2_ExitIf(!OptVal(Argc, Argv, "--decode", &P->InputFile), "Provide input file after --decode\n"); // TODO: we actually cannot detect if --decode is not followed by a file name
  v3i First3, Last3;
  if (OptExists(Argc, Argv, "--first") || OptExists(Argc, Argv, "--last"))
  {
    // Parse the first extent coordinates (--first)
    idx2_ExitIf(!OptVal(Argc, Argv, "--first", &First3),
                "Provide --first (the first sample of the box to decode)\n"
                "Example: --first 0 400 0\n");

    // Parse the last extent coordinates (--last)
    idx2_ExitIf(!OptVal(Argc, Argv, "--last", &Last3),
                "Provide --last (the last sample of the box to decode)\n"
                "Example: --first 919 655 719\n");
    P->DecodeExtent = extent(First3, Last3 - First3 + 1);
  }

  // Parse the downsampling factor
  idx2_ExitIf(!OptVal(Argc, Argv, "--downsampling", &P->DownsamplingFactor3), "Provide --downsampling (0 0 0 means full resolution, 1 1 2 means half X, half Y, quarter Z)\n");

  // Parse the decode accuracy (--accuracy)
  idx2_ExitIf(!OptVal(Argc, Argv, "--tolerance", &P->DecodeAccuracy),
              "Provide --tolerance\n"
              "Example: --tolerance 0.01\n");

  // Parse the input directory (--in_dir)
  OptVal(Argc, Argv, "--in_dir", &P->InDir);
  if (!OptExists(Argc, Argv, "--in_dir"))
  { // try to parse the input directory from the input file
    auto Parent = GetParentPath(P->InputFile);
    P->InDir = GetParentPath(Parent);
    if (P->InDir == Parent)
      P->InDir = "./";
  }
  //idx2_ExitIf(
  //  !OptVal(Argc, Argv, "--in_dir", &P->InDir),
  //  "Provide --in_dir (input directory)\n"
  //  "For example, if the input file is C:/Data/MIRANDA/DENSITY.idx2, the --in_dir is C:/Data\n");

}


/* Parse the metadata (name, field, dims, dtype) from the command line */
static void
ParseMetaData(int Argc, cstr* Argv, params* P)
{
  // Parse the name
  idx2_ExitIf(!OptVal(Argc, Argv, "--name", P->Meta.Name),
              "Provide --name\n"
              "Example: --name Miranda\n");

  // Parse the field name (--field)
  idx2_ExitIf(!OptVal(Argc, Argv, "--field", P->Meta.Field),
              "Provide --field\n"
              "Example: --field Density\n");

  // Parse the dimensions
  idx2_ExitIf(!OptVal(Argc, Argv, "--dims", &P->Meta.Dims3),
              "Provide --dims\n"
              "Example: --dims 384 384 256\n");

  // Parse the data type (--type)
  char DType[8];
  idx2_ExitIf(!OptVal(Argc, Argv, "--type", DType),
              "Provide --type (float32 or float64)\n"
              "Example: --type float32\n");
  P->Meta.DType = StringTo<dtype>()(stref(DType));
  idx2_ExitIf(P->Meta.DType != dtype::float32 && P->Meta.DType != dtype::float64,
              "Unsupported type\n");
}


/* If the files in P->InputFiles have different sizes, return false */
static bool
CheckFileSizes(params* P)
{
  i64 ConstantSize = 0;
  idx2_For (int, I, 0, Size(P->InputFiles))
  {
    i64 S = GetFileSize(stref(P->InputFiles[I].Arr));
    if (I == 0)
      ConstantSize = S;
    if (S != ConstantSize)
      return false;
  }

  int DTypeSize = SizeOf(P->Meta.DType);
  idx2_ExitIf(ConstantSize % DTypeSize != 0, "File size not multiple of dtype size\n");
  P->NSamplesInFile = ConstantSize / DTypeSize;

  return true;
}


/* Parse the options specific to encoding */
static void
ParseEncodeOptions(int Argc, cstr* Argv, params* P)
{
  idx2_ExitIf(!OptVal(Argc, Argv, "--encode", &P->InputFile), "Provide input file after --encode\n"); // TODO: we actually cannot currently detect if --encode is not followed by a file names
  // First, try to parse the metadata from the file name
  auto ParseOk = StrToMetaData(P->InputFile, &P->Meta);
  // if the previous parse fails, parse metadata from the command line
  if (!ParseOk)
  {
    ParseMetaData(Argc, Argv, P);

    // If the input file is a .txt file, read all the file names in the txt into an array
    if (GetExtension(P->InputFile) == idx2_StRef("txt"))
    {
      // Parse the file names
      idx2_RAII(FILE*, Fp = fopen(P->InputFile, "rb"), , if (Fp) fclose(Fp));
      ReadLines(Fp, &P->InputFiles);
      idx2_ExitIf(!CheckFileSizes(P), "Input files have to be of the same size\n");
    }
  }

  // Check the data type
  idx2_ExitIf(P->Meta.DType == dtype::__Invalid__, "Data type not supported\n");

  // Parse the brick dimensions (--brick_size)
  OptVal(Argc, Argv, "--brick_size", &P->BrickDims3);
  //idx2_ExitIf(!OptVal(Argc, Argv, "--brick_size", &P->BrickDims3),
  //            "Provide --brick_size\n"
  //            "Example: --brick_size 32 32 32\n");

  // Parse the number of levels (--num_levels)
  idx2_ExitIf(!OptVal(Argc, Argv, "--num_levels", &P->NLevels),
              "Provide --num_levels\n"
              "Example: --num_levels 2\n");

  // Parse the tolerance (--tolerance)
  idx2_ExitIf(!OptVal(Argc, Argv, "--tolerance", &P->Accuracy),
              "Provide --tolerance for absolute error tolerance\n"
              "Example: --tolerance 1e-9\n");

  OptVal(Argc, Argv, "--bricks_per_tile", &P->BricksPerChunk);
  OptVal(Argc, Argv, "--tiles_per_file", &P->ChunksPerFile);
  OptVal(Argc, Argv, "--files_per_dir", &P->FilesPerDir);
  //// Parse the number of bricks per tile (--bricks_per_tile)
  //idx2_ExitIf(!OptVal(Argc, Argv, "--bricks_per_tile", &P->BricksPerChunk),
  //            "Provide --bricks_per_tile\n"
  //            "Example: --bricks_per_tile 512\n");

  //// Parse the number of tiles per file (--tiles_per_file)
  //idx2_ExitIf(!OptVal(Argc, Argv, "--tiles_per_file", &P->ChunksPerFile),
  //            "Provide --tiles_per_file\n"
  //            "Example: --tiles_per_file 4096\n");

  //// Parse the number of files per directory (--files_per_dir)
  //idx2_ExitIf(!OptVal(Argc, Argv, "--files_per_dir", &P->FilesPerDir),
  //            "Provide --files_per_dir\n"
  //            "Example: --files_per_dir 4096\n");

  // Parse the optional version (--version)
  OptVal(Argc, Argv, "--version", &P->Version);
  // Parse the optional output directory (--out_dir)
  OptVal(Argc, Argv, "--out_dir", &P->OutDir);

  // Parse various grouping options (levels, bit planes, sub levels)
  // P->GroupLevels = OptExists(Argc, Argv, "--group_levels");
  // P->GroupBitPlanes = OptExists(Argc, Argv, "--group_bit_planes");
  // P->GroupSubLevels = OptExists(Argc, Argv, "--group_sub_levels");

  // Parse the optional strides and offset
  OptVal(Argc, Argv, "--strides", &P->Strides3);
  OptVal(Argc, Argv, "--offset", &P->Offset);

  // Parse the --llc (for NASA datasets)
  OptVal(Argc, Argv, "--llc", &P->LLC);
  if (P->LLC >= 0)
  {
    idx2_ExitIf(Size(P->InputFiles) == 0,
                "Provide a text file containing a list of data files with --input\n");
    idx2_ExitIf(P->Strides3 == v3<i64>(0), "Provide --strides\n");
    idx2_ExitIf(P->Offset == -1, "Provide --offset (in number of samples)\n");
  }
}


/* Parse the parameters to the program from the command line */
params
ParseParams(int Argc, cstr* Argv)
{
  params P;

  P.Action = OptExists(Argc, Argv, "--encode")   ? action::Encode
             : OptExists(Argc, Argv, "--decode") ? P.Action = action::Decode
                                                 : action::__Invalid__;

  idx2_ExitIf(P.Action == action::__Invalid__,
              "Provide either --encode or --decode\n"
              "Example 1: --encode /Users/abc/Miranda-Density-[384-384-256]-Float64.raw\n"
              "Example 2: --encode Miranda-Density-[384-384-256]-Float64.raw\n"
              "Example 3: --decode /Users/abc/Miranda/Density.idx2\n"
              "Example 4: --decode Miranda/Density.idx2\n");

  //// Parse the input file (--input)
  //idx2_ExitIf(!OptVal(Argc, Argv, "--input", &P.InputFile),
  //            "Provide --input\n"
  //            "Example: --input /Users/abc/MIRANDA-DENSITY-[384-384-256]-Float64.raw\n");

  // Parse the pause option (--pause): wait for keyboard input at the end
  P.Pause = OptExists(Argc, Argv, "--pause");

  // Parse the optional output directory (--out_dir)
  OptVal(Argc, Argv, "--out_dir", &P.OutDir);
  // Parse the optional output file (--out_file)
  OptVal(Argc, Argv, "--out_file", &P.OutFile);

  // Parse the dry run option (--dry): if enabled, skip writing the output file
  P.OutMode =
    OptExists(Argc, Argv, "--dry") ? params::out_mode::NoOutput : params::out_mode::RegularGridFile;

  // Perform parsing depending on the action
  if (P.Action == action::Encode)
    ParseEncodeOptions(Argc, Argv, &P);
  else if (P.Action == action::Decode)
    ParseDecodeOptions(Argc, Argv, &P);

  return P;
}


/* "Copy" the parameters from the command line to the internal idx2_file struct */
static error<idx2_err_code>
SetParams(idx2_file* Idx2, const params& P)
{
  SetName(Idx2, P.Meta.Name);
  SetField(Idx2, P.Meta.Field);
  SetVersion(Idx2, P.Version);
  SetDimensions(Idx2, P.Meta.Dims3);
  SetDataType(Idx2, P.Meta.DType);
  SetBrickSize(Idx2, P.BrickDims3);
  SetBricksPerChunk(Idx2, P.BricksPerChunk);
  SetChunksPerFile(Idx2, P.ChunksPerFile);
  SetNumIterations(Idx2, (i8)P.NLevels);
  SetAccuracy(Idx2, P.Accuracy);
  SetFilesPerDirectory(Idx2, P.FilesPerDir);
  SetDir(Idx2, P.OutDir);
  SetGroupLevels(Idx2, P.GroupLevels);
  SetGroupBitPlanes(Idx2, P.GroupBitPlanes);
  SetGroupSubLevels(Idx2, P.GroupSubLevels);

  return Finalize(Idx2, P);
}


/* Main function (entry point of the idx2 command) */
int
main(int Argc, cstr* Argv)
{
  SetHandleAbortSignals();

  idx2_RAII(params, P, P = ParseParams(Argc, Argv));

  idx2_RAII(timer,
            Timer,
            StartTimer(&Timer),
            printf("Total time: %f seconds\n", Seconds(ElapsedTime(&Timer))));

  /* Perform the action */
  idx2_RAII(idx2_file, Idx2);

  if (P.Action == action::Encode)
  {
    RemoveDir(idx2_PrintScratch("%s/%s", P.OutDir, P.Meta.Name));
    idx2_ExitIfError(SetParams(&Idx2, P));
    if (Size(P.InputFiles) > 0)
    { // the input contains multiple files
      idx2_ExitIf(true, "File list input not supported at the moment\n");
    }
    else if (Size(P.InputFiles) == 0)
    { // a single raw volume is provided
      idx2_RAII(mmap_volume, Vol, (void)Vol, Unmap(&Vol));
      //      error Result = ReadVolume(P.Meta.File, P.Meta.Dims3, P.Meta.DType, &Vol.Vol);
      idx2_ExitIfError(MapVolume(P.InputFile, P.Meta.Dims3, P.Meta.DType, &Vol, map_mode::Read));
      brick_copier Copier(&Vol.Vol);
      idx2_ExitIfError(Encode(&Idx2, P, Copier));
    }
  }
  else if (P.Action == action::Decode)
  {
    idx2_ExitIfError(Init(&Idx2, P));
    idx2_ExitIfError(Decode(Idx2, P));
  }

  if (P.Pause)
  {
    printf("Press any key to end...\n");
    getchar();
  }

  //_CrtDumpMemoryLeaks();
  return 0;
}
