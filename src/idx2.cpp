//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>
#include "idx2_all.h"
#include "idx2_all.cpp"

using namespace idx2;

// TODO: when decoding, construct the raw file name from info embedded inside the compressed file
params
ParseParams(int Argc, cstr* Argv) {
  params P;
  if (OptExists(Argc, Argv, "--encode"))
    P.Action = action::Encode;
  else if (OptExists(Argc, Argv, "--decode"))
    P.Action = action::Decode;
  else {
    fprintf(stderr, "Provide either --encode or --decode\n");
    exit(1);
  }
  if (!OptVal(Argc, Argv, "--input", &P.InputFile)) {
    fprintf(stderr, "Provide --input\n");
    fprintf(stderr, "Example: --input /Users/abc/MIRANDA-DENSITY-[384-384-256]-Float64.raw\n");
    exit(1);
  }
  P.Pause = OptExists(Argc, Argv, "--pause");
  OptVal(Argc, Argv, "--out_dir", &P.OutDir);
  OptVal(Argc, Argv, "--out_file", &P.OutFile);
  if (OptExists(Argc, Argv, "--dry"))
    P.OutMode = params::out_mode::NoOutput;
  else
    P.OutMode = params::out_mode::WriteToFile;
  if (P.Action == action::Encode) {
    error Err = ParseMeta(P.InputFile, &P.Meta);
    if (ErrorExists(Err)) {
      fprintf(stderr, "Error parsing input information\n");
      fprintf(stderr, "%s\n", ToString(Err));
      exit(1);
    }
    // TODO: provide support for other types
    if (P.Meta.DType != dtype::float64 && P.Meta.DType != dtype::float32) {
      fprintf(stderr, "Data type not supported\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--brick_size", &P.BrickDims3)) {
      fprintf(stderr, "Provide --brick_size\n");
      fprintf(stderr, "Example: --brick_size 32 32 32\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--num_levels", &P.NLevels)) {
      fprintf(stderr, "Provide --num_levels\n");
      fprintf(stderr, "Example: --num_levels 2\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--accuracy", &P.Accuracy)) {
      fprintf(stderr, "Provide --accuracy\n");
      fprintf(stderr, "Example: --accuracy 1e-9\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--bricks_per_tile", &P.BricksPerChunk)) {
      fprintf(stderr, "Provide --bricks_per_tile\n");
      fprintf(stderr, "Example: --bricks_per_tile 512\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--tiles_per_file", &P.ChunksPerFile)) {
      fprintf(stderr, "Provide --tiles_per_file\n");
      fprintf(stderr, "Example: --tiles_per_file 4096\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--files_per_dir", &P.FilesPerDir)) {
      fprintf(stderr, "Provide --files_per_dir\n");
      fprintf(stderr, "Example: --files_per_dir 4096\n");
      exit(1);
    }
    OptVal(Argc, Argv, "--quality_levels", &P.RdoLevels);
    OptVal(Argc, Argv, "--version", &P.Version);
    OptVal(Argc, Argv, "--out_dir", &P.OutDir);
    char Temp[8];
    cstr TempPtr = Temp;
    if (OptExists(Argc, Argv, "--group_levels")) {
      OptVal(Argc, Argv, "--group_levels", &TempPtr);
      P.GroupLevels = strcmp(TempPtr, "yes") == 0;
    }
    if (OptExists(Argc, Argv, "--group_bit_planes")) {
      OptVal(Argc, Argv, "--group_bit_planes", &TempPtr);
      P.GroupBitPlanes = strcmp(TempPtr, "yes") == 0;
    }
    if (OptExists(Argc, Argv, "--group_sub_levels")) {
      OptVal(Argc, Argv, "--group_sub_levels", &TempPtr);
      P.GroupSubLevels = strcmp(TempPtr, "yes") == 0;
    }
  } else if (P.Action == action::Decode) {
    v3i First3, Last3;
    if (!OptVal(Argc, Argv, "--first", &First3)) {
      fprintf(stderr, "Provide --first (the first sample of the box to decode)\n");
      fprintf(stderr, "Example: --first 0 400 0\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--last", &Last3)) {
      fprintf(stderr, "Provide --last (the last sample of the box to decode)\n");
      fprintf(stderr, "Example: --first 919 655 719\n");
      exit(1);
    }
    P.DecodeExtent = extent(First3, Last3 - First3 + 1);
    if (!OptVal(Argc, Argv, "--level", &P.OutputLevel)) {
      fprintf(stderr, "Provide --level (0 means full resolution)\n");
      fprintf(stderr, "The decoder will not decode levels less than this (finer resolution levels)\n");
      fprintf(stderr, "Example: --level 0\n");
      exit(1);
    }
    P.DecodeLevel = P.OutputLevel;
    u8 Mask = 0;
    if (!OptVal(Argc, Argv, "--mask", &Mask)) {
      fprintf(stderr, "Provide --mask (8-bit mask, 128 (0x80) means full resolution)\n");
      fprintf(stderr, "For example, if the volume is 256 x 256 x 256 and there are 2 levels\n");
      fprintf(stderr, "Level 0, mask 128 = 256 x 256 x 256\n");
      fprintf(stderr, "Level 0, mask 64  = 256 x 256 x 128\n");
      fprintf(stderr, "Level 0, mask 32  = 256 x 128 x 256\n");
      fprintf(stderr, "Level 0, mask 16  = 128 x 256 x 256\n");
      fprintf(stderr, "Level 0, mask 8   = 256 x 128 x 128\n");
      fprintf(stderr, "Level 0, mask 4   = 128 x 256 x 128\n");
      fprintf(stderr, "Level 0, mask 2   = 128 x 128 x 256\n");
      fprintf(stderr, "Level 0, mask 1   = 128 x 128 x 128\n");
      fprintf(stderr, "Level 1, mask 128 = 128 x 128 x 128\n");
      fprintf(stderr, "and so on, until...");
      fprintf(stderr, "Level 1, mask 1   =  64 x  64 x  64\n");
      fprintf(stderr, "Example: --mask 128\n");
      exit(1);
    }
    if (!OptVal(Argc, Argv, "--accuracy", &P.DecodeAccuracy)) {
      fprintf(stderr, "Provide --accuracy\n");
      fprintf(stderr, "Example: --accuracy 0.01\n");
      exit(1);
    }
    /* swap bit 3 and 4 */
    P.DecodeMask = Mask;
    if (BitSet(Mask, 3)) P.DecodeMask = SetBit(Mask, 4); else P.DecodeMask = UnsetBit(Mask, 4);
    if (BitSet(Mask, 4)) P.DecodeMask = SetBit(P.DecodeMask, 3); else P.DecodeMask = UnsetBit(P.DecodeMask, 3);
    if (!OptVal(Argc, Argv, "--in_dir", &P.InDir)) {
      fprintf(stderr, "Provide --in_dir (input directory)\n");
      fprintf(stderr, "For example, if the input file is C:/Data/MIRANDA/DENSITY.idx, the --in_dir is C:/Data\n");
      exit(1);
    }
    /* parse the quality level */
    if (!OptVal(Argc, Argv, "--quality_level", &P.QualityLevel)) {}
    if (!OptVal(Argc, Argv, "--decode_level", &P.DecodeLevel)) {}
  }
  return P;
}

static error<idx2_file_err_code>
SetParams(idx2_file* Idx2, const params& P) {
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

// TODO: handle float/int/int64/etc
int
main(int Argc, cstr* Argv) {
  SetHandleAbortSignals();
  /* Read the parameters */
  params P = ParseParams(Argc, Argv);
  if (P.Action == action::Encode) {
    error MetaOk = ParseMeta(P.InputFile, &P.Meta);
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
    if (P.Action == action::Encode) {
      idx2_ExitIfError(SetParams(&Idx2, P));
      idx2_RAII(mmap_volume, Vol, (void)Vol, Unmap(&Vol));
//      error Result = ReadVolume(P.Meta.File, P.Meta.Dims3, P.Meta.DType, &Vol.Vol);
      idx2_ExitIfError(MapVolume(P.Meta.File, P.Meta.Dims3, P.Meta.DType, &Vol, map_mode::Read));
      idx2_ExitIfError(Encode(&Idx2, P, Vol.Vol));
    } else if (P.Action == action::Decode) {
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
