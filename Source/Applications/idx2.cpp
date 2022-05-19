//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

//#define idx2_Implementation
//#include "../idx2.hpp"
#include "../idx2Lib.h"

using namespace idx2;

static void
ParseDecodeOptions
(
  int Argc,
  cstr* Argv,
  params* P
)
{
  v3i First3, Last3;
  // Parse the first extent coordinates (--first)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--first", &First3),
    "Provide --first (the first sample of the box to decode)\n"
    "Example: --first 0 400 0\n"
  );
  // Parse the last extent coordinates (--last)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--last", &Last3),
    "Provide --last (the last sample of the box to decode)\n"
    "Example: --first 919 655 719\n"
  );
  P->DecodeExtent = extent(First3, Last3 - First3 + 1);
  // Parse the output level (--level)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--level", &P->OutputLevel),
    "Provide --level (0 means full resolution)\n"
    "The decoder will not decode levels less than this (finer resolution levels)\n"
    "Example: --level 0\n"
  );
  P->DecodeLevel = P->OutputLevel;
  // Parse the mask for sub level (--mask)
  u8 Mask = 0;
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--mask", &Mask),
    "Provide --mask (8-bit mask, 128 (0x80) means full resolution)\n"
    "For example, if the volume is 256 x 256 x 256 and there are 2 levels\n"
    "Level 0, mask 128 = 256 x 256 x 256\n"
    "Level 0, mask 64  = 256 x 256 x 128\n"
    "Level 0, mask 32  = 256 x 128 x 256\n"
    "Level 0, mask 16  = 128 x 256 x 256\n"
    "Level 0, mask 8   = 256 x 128 x 128\n"
    "Level 0, mask 4   = 128 x 256 x 128\n"
    "Level 0, mask 2   = 128 x 128 x 256\n"
    "Level 0, mask 1   = 128 x 128 x 128\n"
    "Level 1, mask 128 = 128 x 128 x 128\n"
    "and so on, until..."
    "Level 1, mask 1   =  64 x  64 x  64\n"
    "Example: --mask 128\n"
  );
  // swap bit 3 and 4 of the decode mask
  P->DecodeMask = Mask;
  if (BitSet(Mask, 3))
    P->DecodeMask = SetBit(Mask, 4);
  else
    P->DecodeMask = UnsetBit(Mask, 4);
  if (BitSet(Mask, 4))
    P->DecodeMask = SetBit(P->DecodeMask, 3);
  else
    P->DecodeMask = UnsetBit(P->DecodeMask, 3);
  // Parse the decode accuracy (--accuracy)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--accuracy", &P->DecodeAccuracy),
    "Provide --accuracy\n"
    "Example: --accuracy 0.01\n"
  );
  // Parse the input directory (--in_dir)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--in_dir", &P->InDir),
    "Provide --in_dir (input directory)\n"
    "For example, if the input file is C:/Data/MIRANDA/DENSITY.idx2, the --in_dir is C:/Data\n"
  );
  /* Parse the optional quality and decode levels */
  OptVal(Argc, Argv, "--quality_level", &P->QualityLevel);
  OptVal(Argc, Argv, "--decode_level", &P->DecodeLevel);
}

static void
ParseMetaData
( /* Parse the metadata (name, field, dims, dtype) from the command line */
  int Argc,
  cstr* Argv,
  params* P
  /*---------------------------------------------------------------------*/
)
{
  //stref FileStr = idx2_StRef(P->Meta.File);
  //Copy(P->InputFile, &FileStr);
  // Parse the dataset name (--name)
  //idx2_ExitIf
  //(
  //  !OptVal(Argc, Argv, "--name", &P->Meta.Name),
  //  "Provide --name\n"
  //  "Example: --name Miranda\n"
  //);
  //// Parse the field name (--field)
  //idx2_ExitIf
  //(
  //  !OptVal(Argc, Argv, "--field", &P->Meta.Field),
  //  "Provide --field\n"
  //  "Example: --field Density\n"
  //);
  // Parse the dimensions
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--dims", &P->Meta.Dims3),
    "Provide --dims\n"
    "Example: --dims 384 384 256\n"
  );
  // Parse the data type (--dtype)
 /* char DType[8];
  char* DTypePtr = DType;
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--field", &DTypePtr),
    "Provide --dtype (float32 or float64)\n"
    "Example: --dtype float32\n"
  );*/
  //P->Meta.DType = StringTo<dtype>()(stref(DType));
}

static void
ParseEncodeOptions
( /* Parse the options specific to encoding */
  int Argc,
  cstr* Argv,
  params* P
) /*----------------------------------------*/
{
  // First, try to parse the metadata from the file name
  auto ParseOk = StrToMetaData(P->InputFile, &P->Meta);
  // if the previous parse fails, parse metadata from the command line
  if (!ParseOk)
  {
    ParseMetaData(Argc, Argv, P);
    // If the input file is a .txt file, read all the file names in the txt into an array
    if (GetExtension(P->InputFile) == idx2_StRef("txt"))
    {
      // Parse
    }
  }
  idx2_ExitIf(P->Meta.DType == dtype::__Invalid__, "Data type not supported\n");
  // Parse the brick dimensions (--brick_size)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--brick_size", &P->BrickDims3),
    "Provide --brick_size\n"
    "Example: --brick_size 32 32 32\n"
  );
  // Parse the number of levels (--num_levels)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--num_levels", &P->NLevels),
    "Provide --num_levels\n"
    "Example: --num_levels 2\n"
  );
  // Parse the accuracy (--accuracy)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--accuracy", &P->Accuracy),
    "Provide --accuracy\n"
    "Example: --accuracy 1e-9\n"
  );
  // Parse the number of bricks per tile (--bricks_per_tile)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--bricks_per_tile", &P->BricksPerChunk),
    "Provide --bricks_per_tile\n"
    "Example: --bricks_per_tile 512\n"
  );
  // Parse the number of tiles per file (--tiles_per_file)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--tiles_per_file", &P->ChunksPerFile),
    "Provide --tiles_per_file\n"
    "Example: --tiles_per_file 4096\n"
  );
  // Parse the number of files per directory (--files_per_dir)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--files_per_dir", &P->FilesPerDir),
    "Provide --files_per_dir\n"
    "Example: --files_per_dir 4096\n"
  );
  // Parse the optional RDO levels (--quality_levels)
  OptVal(Argc, Argv, "--quality_levels", &P->RdoLevels);
  // Parse the optional version (--version)
  OptVal(Argc, Argv, "--version", &P->Version);
  // Parse the optional output directory (--out_dir)
  OptVal(Argc, Argv, "--out_dir", &P->OutDir);

  // Parse various grouping options (levels, bit planes, sub levels)
  P->GroupLevels = OptExists(Argc, Argv, "--group_levels");
  P->GroupBitPlanes = OptExists(Argc, Argv, "--group_bit_planes");
  P->GroupSubLevels = OptExists(Argc, Argv, "--group_sub_levels");
}

params
ParseParams
( /* Parse the parameters to the program from the command line */
  int Argc,
  cstr* Argv
) /*-----------------------------------------------------------*/
{
  params P;
  P.Action = OptExists(Argc, Argv, "--encode")
           ? action::Encode
           : OptExists(Argc, Argv, "--decode")
           ? P.Action = action::Decode
           : action::__Invalid__;
  idx2_ExitIf(P.Action == action::__Invalid__, "Provide either --encode or --decode\n");

  // Parse the input file (--input)
  idx2_ExitIf
  (
    !OptVal(Argc, Argv, "--input", &P.InputFile),
    "Provide --input\n"
    "Example: --input /Users/abc/MIRANDA-DENSITY-[384-384-256]-Float64.raw\n"
  );

  // Parse the pause option (--pause): wait for keyboard input at the end
  P.Pause = OptExists(Argc, Argv, "--pause");

  // Parse the optional output directory (--out_dir)
  OptVal(Argc, Argv, "--out_dir", &P.OutDir);
  // Parse the optional output file (--out_file)
  OptVal(Argc, Argv, "--out_file", &P.OutFile);

  // Parse the dry run option (--dry): if enabled, skip writing the output file
  P.OutMode = OptExists(Argc, Argv, "--dry")
            ? params::out_mode::NoOutput
            : params::out_mode::WriteToFile;

  // Perform parsing depending on the action
  idx2_Case_1(P.Action == action::Encode)
    ParseEncodeOptions(Argc, Argv, &P);
  idx2_Case_2(P.Action == action::Decode)
    ParseDecodeOptions(Argc, Argv, &P);

  return P;
}

static error<idx2_err_code>
SetParams
( /* "Copy" the parameters from the command line to the internal idx2_file struct */
  idx2_file* Idx2,
  const params& P
) /*------------------------------------------------------------------------------*/
{
  SetName(Idx2, P.Meta.Name);
  SetField(Idx2, P.Meta.Field);
  SetVersion(Idx2, P.Version);
  SetDimensions(Idx2, P.Meta.Dims3);
  SetDataType(Idx2, P.Meta.DType);
  SetBrickSize(Idx2, P.BrickDims3);
  SetBricksPerChunk(Idx2, P.BricksPerChunk);
  SetChunksPerFile(Idx2,P.ChunksPerFile);
  SetNumIterations(Idx2, (i8)P.NLevels);
  SetAccuracy(Idx2, P.Accuracy);
  SetFilesPerDirectory(Idx2, P.FilesPerDir);
  SetDir(Idx2, P.OutDir);
  SetGroupLevels(Idx2, P.GroupLevels);
  SetGroupBitPlanes(Idx2, P.GroupBitPlanes);
  SetGroupSubLevels(Idx2, P.GroupSubLevels);
  SetQualityLevels(Idx2, P.RdoLevels);
  return Finalize(Idx2);
}

int
main
( /* Program's entry point */
  int Argc,
  cstr* Argv
) /*-----------------------*/
{
  SetHandleAbortSignals();
  /* Read the parameters */
  params P = ParseParams(Argc, Argv);
  if (P.Action == action::Encode) {
    error MetaOk = StrToMetaData(P.InputFile, &P.Meta);
    if (!MetaOk) {
      cstr Str = P.Meta.Name;
      if (!OptVal(Argc, Argv, "--name", &Str)) {
        fprintf(stderr, "Provide --name\n");
        fprintf(stderr, "Example: --name miranda\n");
        exit(1);
      }
      Str = P.Meta.Field;
      if (!OptVal(Argc, Argv, "--field", &Str)) {
        fprintf(stderr, "Provide --field\n");
        fprintf(stderr, "Example: --field density\n");
        exit(1);
      }
      if (!OptVal(Argc, Argv, "--dims", &P.Meta.Dims3)) {
        fprintf(stderr, "Provide --dims\n");
        fprintf(stderr, "Example: --dims 96 96 96\n");
        exit(1);
      }
      char TypeBuf[8]; cstr Type = TypeBuf;
      if (!OptVal(Argc, Argv, "--type", &Type)) {
        fprintf(stderr, "Provide --type\n");
        fprintf(stderr, "Example: --type float64\n");
        exit(1);
      }
      P.Meta.DType = StringTo<dtype>()(Type);
    }
  }

  { /* Perform the action */
    idx2_RAII(timer, Timer, StartTimer(&Timer), printf("Total time: %f seconds\n", Seconds(ElapsedTime(&Timer))));
    idx2_file Idx2;
    idx2_Case_1 (P.Action == action::Encode)
    {
//      RemoveDir(idx2_PrintScratch("%s/%s", P.OutDir, P.Meta.Name));
      idx2_ExitIfError(SetParams(&Idx2, P));
      idx2_RAII(mmap_volume, Vol, (void)Vol, Unmap(&Vol));
//      error Result = ReadVolume(P.Meta.File, P.Meta.Dims3, P.Meta.DType, &Vol.Vol);
      idx2_ExitIfError(MapVolume(P.InputFile, P.Meta.Dims3, P.Meta.DType, &Vol, map_mode::Read));
			idx2_ExitIfError(Encode(&Idx2, P, brick_copier(&Vol.Vol)));
    }
    idx2_Case_2 (P.Action == action::Decode)
    {
      SetDir(&Idx2, P.InDir);
//      brick_table<f64> BrickTable;
      idx2_ExitIfError(ReadMetaFile(&Idx2, idx2_PrintScratch("%s", P.InputFile)));
      idx2_ExitIfError(Finalize(&Idx2));
      Decode(Idx2, P);
      // TODO: convert the brick table to a regular volume
    }
    Dealloc(&Idx2);
  }
  if (P.Pause) {
    printf("Press any key to end...\n");
    getchar();
  }
  Dealloc(&P);
  //_CrtDumpMemoryLeaks();
  return 0;
}

