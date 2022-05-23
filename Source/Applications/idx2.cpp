//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

//#define idx2_Implementation
//#include "../idx2.hpp"
#include "../idx2Lib.h"

using namespace idx2;


/* Parse the decode options
---------------------------*/
static void ParseDecodeOptions(int Argc, cstr* Argv, params* P)
{
  v3i First3, Last3;
  // Parse the first extent coordinates (--first)
  idx2_ExitIf(
    !OptVal(Argc, Argv, "--first", &First3),
    "Provide --first (the first sample of the box to decode)\n"
    "Example: --first 0 400 0\n");

  // Parse the last extent coordinates (--last)
  idx2_ExitIf(!OptVal(Argc, Argv, "--last", &Last3),
    "Provide --last (the last sample of the box to decode)\n"
    "Example: --first 919 655 719\n");
  P->DecodeExtent = extent(First3, Last3 - First3 + 1);

  // Parse the output level (--level)
  idx2_ExitIf(!OptVal(Argc, Argv, "--level", &P->OutputLevel),
    "Provide --level (0 means full resolution)\n"
    "The decoder will not decode levels less than this (finer resolution levels)\n"
    "Example: --level 0\n");
  P->DecodeLevel = P->OutputLevel;

  // Parse the mask for sub level (--mask)
  u8 Mask = 0;
  idx2_ExitIf(!OptVal(Argc, Argv, "--mask", &Mask),
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
    "Example: --mask 128\n");
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
  idx2_ExitIf(!OptVal(Argc, Argv, "--accuracy", &P->DecodeAccuracy),
    "Provide --accuracy\n"
    "Example: --accuracy 0.01\n");

  // Parse the input directory (--in_dir)
  idx2_ExitIf(!OptVal(Argc, Argv, "--in_dir", &P->InDir),
    "Provide --in_dir (input directory)\n"
    "For example, if the input file is C:/Data/MIRANDA/DENSITY.idx2, the --in_dir is C:/Data\n");

  /* Parse the optional quality and decode levels */
  OptVal(Argc, Argv, "--quality_level", &P->QualityLevel);
  OptVal(Argc, Argv, "--decode_level", &P->DecodeLevel);
}


/* Parse the metadata (name, field, dims, dtype) from the command line
----------------------------------------------------------------------*/
static void ParseMetaData(int Argc, cstr* Argv, params* P)
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
  idx2_ExitIf(P->Meta.DType!=dtype::float32 && P->Meta.DType!=dtype::float64,
    "Unsupported type\n");
}


/* If the files in P->InputFiles have different sizes, return false
-------------------------------------------------------------------*/
static bool CheckFileSizes(params* P)
{
  i64 ConstantSize = 0;
  idx2_For(int, I, 0, Size(P->InputFiles)) {
    i64 S = GetFileSize(stref(P->InputFiles[I].Arr));
    if (I == 0) ConstantSize = S;
    if (S != ConstantSize)
      return false;
  }

  int DTypeSize = SizeOf(P->Meta.DType);
  idx2_ExitIf(ConstantSize%DTypeSize != 0, "File size not multiple of dtype size\n");
  P->NSamplesInFile = ConstantSize / DTypeSize;

  return true;
}


/* Parse the options specific to encoding
-----------------------------------------*/
static void ParseEncodeOptions(int Argc, cstr* Argv, params* P)
{
  // First, try to parse the metadata from the file name
  auto ParseOk = StrToMetaData(P->InputFile, &P->Meta);
  // if the previous parse fails, parse metadata from the command line
  if (!ParseOk) {
    ParseMetaData(Argc, Argv, P);

    // If the input file is a .txt file, read all the file names in the txt into an array
    if (GetExtension(P->InputFile) == idx2_StRef("txt")) {
      // Parse the file names
      idx2_RAII(FILE*, Fp = fopen(P->InputFile, "rb"),, if (Fp) fclose(Fp));
      ReadLines(Fp, &P->InputFiles);
      idx2_ExitIf(!CheckFileSizes(P), "Input files have to be of the same size\n");
    }
  }

  // Check the data type
  idx2_ExitIf(P->Meta.DType == dtype::__Invalid__, "Data type not supported\n");

  // Parse the brick dimensions (--brick_size)
  idx2_ExitIf(!OptVal(Argc, Argv, "--brick_size", &P->BrickDims3),
    "Provide --brick_size\n"
    "Example: --brick_size 32 32 32\n");

  // Parse the number of levels (--num_levels)
  idx2_ExitIf(!OptVal(Argc, Argv, "--num_levels", &P->NLevels),
    "Provide --num_levels\n"
    "Example: --num_levels 2\n");

  // Parse the accuracy (--accuracy)
  idx2_ExitIf(!OptVal(Argc, Argv, "--accuracy", &P->Accuracy),
    "Provide --accuracy\n"
    "Example: --accuracy 1e-9\n");

  // Parse the number of bricks per tile (--bricks_per_tile)
  idx2_ExitIf(!OptVal(Argc, Argv, "--bricks_per_tile", &P->BricksPerChunk),
    "Provide --bricks_per_tile\n"
    "Example: --bricks_per_tile 512\n");

  // Parse the number of tiles per file (--tiles_per_file)
  idx2_ExitIf(!OptVal(Argc, Argv, "--tiles_per_file", &P->ChunksPerFile),
    "Provide --tiles_per_file\n"
    "Example: --tiles_per_file 4096\n");

  // Parse the number of files per directory (--files_per_dir)
  idx2_ExitIf(!OptVal(Argc, Argv, "--files_per_dir", &P->FilesPerDir),
    "Provide --files_per_dir\n"
    "Example: --files_per_dir 4096\n");

  // Parse the optional RDO levels (--quality_levels)
  OptVal(Argc, Argv, "--quality_levels", &P->RdoLevels);
  // Parse the optional version (--version)
  OptVal(Argc, Argv, "--version", &P->Version);
  // Parse the optional output directory (--out_dir)
  OptVal(Argc, Argv, "--out_dir", &P->OutDir);

  // Parse various grouping options (levels, bit planes, sub levels)
  //P->GroupLevels = OptExists(Argc, Argv, "--group_levels");
  //P->GroupBitPlanes = OptExists(Argc, Argv, "--group_bit_planes");
  //P->GroupSubLevels = OptExists(Argc, Argv, "--group_sub_levels");

  // Parse the optional strides and offset
  OptVal(Argc, Argv, "--strides", &P->Strides3);
  OptVal(Argc, Argv, "--offset", &P->Offset);

  // Parse the --llc_latlon or --llc_cap options (for NASA datasets)
  OptVal(Argc, Argv, "--llc_latlon", &P->LLC_LatLon);
  OptVal(Argc, Argv, "--llc_cap", &P->LLC_Cap);
  if (P->LLC_LatLon > 0  ||  P->LLC_Cap > 0) {
    idx2_ExitIf(P->LLC_Cap > 0 && P->LLC_LatLon > 0,
      "Provide only one of { --llc_latlon, --llc_cap }, not both\n");
    idx2_ExitIf(Size(P->InputFiles) == 0,
      "Provide a text file containing a list of data files with --input\n");
    idx2_ExitIf(P->Strides3 == v3<i64>(0), "Provide --strides\n");
    idx2_ExitIf(P->Offset == -1, "Provide --offset\n");
  }
}


/* Parse the parameters to the program from the command line
------------------------------------------------------------*/
params ParseParams(int Argc, cstr* Argv)
{
  params P;

  P.Action = OptExists(Argc, Argv, "--encode")
           ? action::Encode
           : OptExists(Argc, Argv, "--decode")
           ? P.Action = action::Decode
           : action::__Invalid__;

  idx2_ExitIf(P.Action == action::__Invalid__, "Provide either --encode or --decode\n");

  // Parse the input file (--input)
  idx2_ExitIf(!OptVal(Argc, Argv, "--input", &P.InputFile),
    "Provide --input\n"
    "Example: --input /Users/abc/MIRANDA-DENSITY-[384-384-256]-Float64.raw\n");

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


/* "Copy" the parameters from the command line to the internal idx2_file struct
-------------------------------------------------------------------------------*/
static error<idx2_err_code> SetParams(idx2_file* Idx2, const params& P)
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

struct llc_latlon_brick_copier : public brick_copier
{
  params* P = nullptr;
  FILE* Fp = nullptr;
  int CurrentFile = -1;

  llc_latlon_brick_copier() = delete;
  llc_latlon_brick_copier(params* P) : P(P) { idx2_Assert(P->LLC_LatLon > 0); }

  virtual v2d
  Copy(const extent& ExtentGlobal, const extent& ExtentLocal, brick_volume* Brick);

  virtual ~llc_latlon_brick_copier() { if (Fp) fclose(Fp); }
};


v2d llc_latlon_brick_copier::Copy(
  const extent& ExtentGlobal,
  const extent& ExtentLocal,
  brick_volume* Brick)
{
  int N = P->LLC_LatLon;
  v3i N12(N, N*3, P->Dims3.Z); // dimensions of the first and second face
  v3i N45(N*3, N, P->Dims3.Z); // dimensions of the fourth and fifth face
  v3<i64> T3 = P->Strides3; // strides
  i64 Offset = P->Offset;
  i64 NSamplesInFile = P->NSamplesInFile;

  v2d MinMax = v2d(traits<f64>::Max, traits<f64>::Min);

  // Check ExtentGlobal against the first face
  {
    extent FaceExtent{v3i(0), N12};
    extent E = Crop(ExtentGlobal, FaceExtent);
    if (E) {
      extent R = Relative(E, FaceExtent);
      extent D = Relative(E, ExtentGlobal);
      v3i SFrom3 = From(E); // TODO = offset
      v3i STo3   = To(E);
      v3i DFrom3 = From(D);
      v3i DTo3   = To(D);
      v3i DstDims3 = Dims(Brick->Vol);
      v3i S3, D3;
      static i64 iter = 0;
      idx2_BeginFor3Lockstep(S3, SFrom3, STo3, v3i(1), D3, DFrom3, DTo3, v3i(1)) {
        ++iter;
        i64 I = Offset + i64(S3.Z*T3.Z) + i64(S3.Y*T3.Y) + i64(S3.X*T3.X);
        i64 J = Row(D3, DstDims3);
        i64 F = I / NSamplesInFile; // file id
        i64 O = I % NSamplesInFile; // offset in file (in number of samples)
        if (F != CurrentFile) {
          if (Fp)
            fclose(Fp);
          Fp = fopen(P->InputFiles[F].Arr, "rb");
          idx2_Assert(Fp);
          CurrentFile = F;
          //printf("Opening file %s\n", P->InputFiles[F].Arr);
        }
        // TODO: branch based on the dtype
        f64* idx2_Restrict DstPtr = (f64*)Brick->Vol.Buffer.Data;
        idx2_FSeek(Fp, O*sizeof(f32), SEEK_SET);
        f32 Val = 0;
        ReadPOD<f32>(Fp, &Val);
        DstPtr[J] = Val;
        MinMax.Min = Min(MinMax.Min, DstPtr[J]);
        MinMax.Max = Max(MinMax.Max, DstPtr[J]);
      } idx2_EndFor3
    }
    // Copy the data over, taking into account the offset and strides
  }

  // Check ExtentGlobal against the second face
  {
    // Crop ExtentGlobal against the second face
    // Check if the cropped extent has a volume > 0
    // Compute the relative position of ExtentGlobal in the second face
    // Copy the data over, taking into account the offset and strides
  }

  // The third face is the cap

  // Check ExtentGlobal against the fourth face
  {
    // Crop ExtentGlobal against the fourth face
    // Check if the cropped extent has a volume > 0
    // Transform ExtentGlobal into the coordinate system of the fourth face
    // Copy the data over, taking into account the offset and strides
  }

  // Check ExtentGlobal against the fifth face
  {
    // Crop ExtentGlobal against the fifth face
    // Check if the cropped extent has a volume > 0
    // Transform ExtentGlobal into the coordinate system of the fifth face
    // Copy the data over, taking into account the offset and strides
  }

  //v2d MinMax;
  //idx2_Case_1(Volume->Type == dtype::float32)
  //  MinMax = (CopyExtentExtentMinMax<f32, f64>(ExtentGlobal, *Volume, ExtentLocal, &Brick->Vol));
  //idx2_Case_2(Volume->Type == dtype::float64)
  //  MinMax = (CopyExtentExtentMinMax<f64, f64>(ExtentGlobal, *Volume, ExtentLocal, &Brick->Vol));
  return MinMax;
}


/* Main function (entry point of the idx2 command)
------------------------------------------------*/
int main(int Argc, cstr* Argv)
{
  SetHandleAbortSignals();

  idx2_RAII(params, P, P = ParseParams(Argc, Argv));

  idx2_RAII(timer, Timer, StartTimer(&Timer),
    printf("Total time: %f seconds\n", Seconds(ElapsedTime(&Timer))));

  /* Perform the action */
  idx2_RAII(idx2_file, Idx2);

  idx2_Case_1 (P.Action == action::Encode) {
//      RemoveDir(idx2_PrintScratch("%s/%s", P.OutDir, P.Meta.Name));
    idx2_ExitIfError(SetParams(&Idx2, P));
    idx2_Case_1(Size(P.InputFiles) > 0) { // the input contains multiple files
      llc_latlon_brick_copier Copier(&P);
      idx2_ExitIfError(Encode(&Idx2, P, Copier));
    }
    idx2_Case_2(Size(P.InputFiles) == 0) { // a single raw volume is provided
      idx2_RAII(mmap_volume, Vol, (void)Vol, Unmap(&Vol));
  //      error Result = ReadVolume(P.Meta.File, P.Meta.Dims3, P.Meta.DType, &Vol.Vol);
      idx2_ExitIfError(MapVolume(P.InputFile, P.Meta.Dims3, P.Meta.DType, &Vol, map_mode::Read));
      brick_copier Copier(&Vol.Vol);
      idx2_ExitIfError(Encode(&Idx2, P, Copier));
    }
  }
  idx2_Case_2 (P.Action == action::Decode) {
    SetDir(&Idx2, P.InDir);
    idx2_ExitIfError(ReadMetaFile(&Idx2, idx2_PrintScratch("%s", P.InputFile)));
    idx2_ExitIfError(Finalize(&Idx2));
    Decode(Idx2, P);
  }

  if (P.Pause) {
    printf("Press any key to end...\n");
    getchar();
  }

  //_CrtDumpMemoryLeaks();
  return 0;
}

