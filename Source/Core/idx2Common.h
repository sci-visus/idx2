#pragma once

#include "Array.h"
#include "BitStream.h"
#include "Common.h"
#include "DataSet.h"
#include "DataTypes.h"
#include "Format.h"
#include "Memory.h"
#include "Volume.h"
#include "Wavelet.h"


/* ---------------------- MACROS ----------------------*/
// Get non-extrapolated dims
#define idx2_NonExtDims(P3) v3i(P3.X - (P3.X > 1), P3.Y - (P3.Y > 1), P3.Z - (P3.Z > 1))
#define idx2_ExtDims(P3) v3i(P3.X + (P3.X > 1), P3.Y + (P3.Y > 1), P3.Z + (P3.Z > 1))

#define idx2_NextMorton(Morton, Row3, Dims3)                                                       \
  if (!(Row3 < Dims3))                                                                             \
  {                                                                                                \
    int B = Lsb(Morton);                                                                           \
    idx2_Assert(B >= 0);                                                                           \
    Morton = (((Morton >> (B + 1)) + 1) << (B + 1)) - 1;                                           \
    continue;                                                                                      \
  }


/* ---------------------- ENUMS ----------------------*/
idx2_Enum(action, u8, Encode, Decode);

idx2_Enum(idx2_err_code,
          u8,
          idx2_CommonErrs,
          BrickSizeNotPowerOfTwo,
          BrickSizeTooBig,
          TooManyLevels,
          TooManyTransformPassesPerLevel,
          TooManyLevelsOrTransformPasses,
          TooManyBricksPerFile,
          TooManyFilesPerDir,
          NotSupportedInVersion,
          CannotCreateDirectory,
          SyntaxError,
          TooManyBricksPerChunk,
          TooManyChunksPerFile,
          ChunksPerFileNotPowerOf2,
          BricksPerChunkNotPowerOf2,
          ChunkNotFound,
          BrickNotFound,
          FileNotFound,
          UnsupportedScheme);

idx2_Enum(func_level, u8, Subband, Sum, Max);


namespace idx2
{

/* ---------------------- TYPES ----------------------*/

struct file_id
{
  stref Name;
  u64 Id = 0;
};


struct params
{
  volume NasaMask;
  action Action = action::__Invalid__;
  metadata Meta;
  v2i Version = v2i(1, 0);
  // v3i Dims3 = v3i(256);
  v3i BrickDims3 = v3i(32);
  array<stack_array<char, 256>> InputFiles;
  cstr InputFile = nullptr; // TODO: change this to local storage
  int NLevels = 1;
  f64 Accuracy = 1e-7;
  int BricksPerChunk = 512;
  int ChunksPerFile = 64;
  int FilesPerDir = 512;
  /* decode exclusive */
  extent DecodeExtent;
  v3i DownsamplingFactor3 = v3i(0); // DownsamplingFactor = [1, 1, 2] means half X, half Y, quarter Z
  f64 DecodeAccuracy = 0;
  cstr OutDir = ".";       // TODO: change this to local storage
  stref InDir = ".";       // TODO: change this to local storage
  cstr OutFile = nullptr;  // TODO: change this to local storage
  bool Pause = false;
  enum class out_mode
  {
    RegularGridFile,// write a regular grid to a file (resolution decided by DownsamplingFactor3)
    RegularGridMem, // output a regular grid in memory (resolution decided by DownsamplingFactor3)
    HashMap,        // a hashmap where each element is a brick, at a potentially different resolution
    NoOutput
  };
  out_mode OutMode = out_mode::RegularGridMem;
  bool GroupLevels = false;
  bool GroupBitPlanes = true;
  bool GroupSubLevels = true;
  bool WaveletOnly = false;
  bool ComputeMinMax = false;
  // either LLC_LatLon or LLC_Cap can be provided, not both
  int LLC = -1; // one of 0, 1, 2 (the cap), 3, 4
  v3<i64> Strides3 = v3<i64>(0);
  i64 Offset = -1; // this can be used to specify the "depth"
  i64 NSamplesInFile = 0;
};


struct idx2_file
{
  // Limits:
  // Level: 6 bits
  // BitPlane: 12 bits
  // Iteration: 4 bits
  // BricksPerChunk: >= 512
  // ChunksPerFile: <= 4096
  // TODO: add limits to all configurable parameters
  // TODO: use int for all params
  static constexpr int MaxBricksPerChunk = 32768;
  static constexpr int MaxChunksPerFile = 4906;
  static constexpr int MaxFilesPerDir = 4096;
  // so max number of blocks per subband can be represented in 2 bytes
  static constexpr int MaxBrickDim = 256;
  static constexpr int MaxLevels = 16;
  static constexpr int MaxTransformPassesPerLevels = 9;
  static constexpr int MaxSpatialDepth = 4; // we have at most this number of spatial subdivisions
  char Name[64] = {};
  char Field[64] = {};
  v3i Dims3 = v3i(256);
  v3i DownsamplingFactor3 = v3i(0);
  dtype DType = dtype::__Invalid__;
  v3i BrickDims3 = v3i(32);
  v3i BrickDimsExt3 = v3i(33);
  v3i BlockDims3 = v3i(4);
  v2<i16> BitPlaneRange = v2<i16>(traits<i16>::Max, traits<i16>::Min);
  static constexpr int NTformPasses = 1;
  u64 TransformOrder = 0;
  stack_array<u8, MaxLevels> DecodeSubbandMasks; // one subband mask per level
  stack_array<v3i, MaxLevels> NBricks3; // number of bricks per level
  stack_array<v3i, MaxLevels> NChunks3;
  stack_array<v3i, MaxLevels> NFiles3;
  array<stack_string<128>> BricksOrderStr;
  array<stack_string<128>> ChunksOrderStr;
  array<stack_string<128>> FilesOrderStr;
  stack_string<16> TransformOrderFull;
  stack_array<stack_array<i8, MaxSpatialDepth>, MaxLevels> FilesDirsDepth; // how many spatial "bits" are consumed by each file/directory level
  stack_array<u64, MaxLevels> BricksOrder; // encode the order of bricks on each level, useful for brick traversal
  stack_array<u64, MaxLevels> BricksOrderInChunk;
  stack_array<u64, MaxLevels> ChunksOrderInFile;
  stack_array<u64, MaxLevels> ChunksOrder;
  stack_array<u64, MaxLevels> FilesOrder;
  f64 Accuracy = 0;
  i8 NLevels = 1;
  int FilesPerDir = 512; // maximum number of files (or sub-directories) per directory
  int BricksPerChunkIn = 512;
  int ChunksPerFileIn = 64;
  stack_array<int, MaxLevels> BricksPerChunk = { { 512 } };
  stack_array<int, MaxLevels> ChunksPerFile = { { 4096 } };
  stack_array<int, MaxLevels> BricksPerFile = { { 512 * 4096 } };
  stack_array<int, MaxLevels> FilesPerVol = { { 4096 } };         // power of two
  stack_array<int, MaxLevels> ChunksPerVol = { { 4096 * 4096 } }; // power of two
  v2i Version = v2i(1, 0);
  array<subband> Subbands;       // based on BrickDimsExt3
  array<subband> SubbandsNonExt; // based on BrickDims3
  v3i GroupBrick3; // how many bricks in the current level form a brick in the next level
  stack_array<v3i, MaxLevels> BricksPerChunk3s = { { v3i(8) } };
  stack_array<v3i, MaxLevels> ChunksPerFile3s = { { v3i(16) } };
  transform_details Td;           // used for normal transform
  transform_details TdExtrpolate; // used only for extrapolation
  stref Dir; // the directory containing the idx2 dataset
  v2d ValueRange = v2d(traits<f64>::Max, traits<f64>::Min);
  bool GroupLevels = false;
  bool GroupBitPlanes = true;
  bool GroupSubbands = true;
};


/* ---------------------- GLOBALS ----------------------*/
extern free_list_allocator BrickAlloc_;


/* ---------------------- FUNCTIONS ----------------------*/

void // TODO: should also return an error?
WriteMetaFile(const idx2_file& Idx2, const params& P, cstr FileName);

error<idx2_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName);

/* Compute the output grid (from, dims, strides) */
grid
GetGrid(const idx2_file& Idx2, const extent& Ext);

void
Dealloc(params* P);

void
SetName(idx2_file* Idx2, cstr Name);

void
SetField(idx2_file* Idx2, cstr Field);

void
SetVersion(idx2_file* Idx2, const v2i& Ver);

void
SetDimensions(idx2_file* Idx2, const v3i& Dims3);

void
SetDataType(idx2_file* Idx2, dtype DType);

void
SetBrickSize(idx2_file* Idx2, const v3i& BrickDims3);

void
SetNumIterations(idx2_file* Idx2, i8 NIterations);

void
SetAccuracy(idx2_file* Idx2, f64 Accuracy);

void
SetChunksPerFile(idx2_file* Idx2, int ChunksPerFile);

void
SetBricksPerChunk(idx2_file* Idx2, int BricksPerChunk);

void
SetFilesPerDirectory(idx2_file* Idx2, int FilesPerDir);

void
SetDir(idx2_file* Idx2, stref Dir);

void
SetGroupLevels(idx2_file* Idx2, bool GroupLevels);

void
SetGroupSubLevels(idx2_file* Idx2, bool GroupSubLevels);

void
SetGroupBitPlanes(idx2_file* Idx2, bool GroupBitPlanes);

void
SetDownsamplingFactor(idx2_file* Idx2, const v3i& DownsamplingFactor3);

error<idx2_err_code>
Finalize(idx2_file* Idx2, const params& P);

void
Dealloc(idx2_file* Idx2);


} // namespace idx2

