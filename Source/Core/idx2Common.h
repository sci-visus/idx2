#pragma once

#include "Array.h"
#include "Common.h"
#include "DataSet.h"
#include "DataTypes.h"
#include "BitStream.h"
#include "Memory.h"
#include "Volume.h"
#include "Wavelet.h"

/* ---------------------- MACROS ----------------------*/
// Get non-extrapolated dims
#define idx2_NonExtDims(P3)\
  v3i(P3.X - (P3.X > 1), P3.Y - (P3.Y > 1), P3.Z - (P3.Z > 1))
#define idx2_ExtDims(P3)\
  v3i(P3.X + (P3.X > 1), P3.Y + (P3.Y > 1), P3.Z + (P3.Z > 1))

#define idx2_NextMorton(Morton, Row3, Dims3)\
  if (!(Row3 < Dims3)) {\
    int B = Lsb(Morton);\
    idx2_Assert(B >= 0);\
    Morton = (((Morton >> (B + 1)) + 1) << (B + 1)) - 1;\
    continue;\
  }

/* ---------------------- ENUMS ----------------------*/
idx2_Enum(action, u8,
  Encode,
  Decode
)

idx2_Enum(idx2_err_code, u8, idx2_CommonErrs,
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
  UnsupportedScheme
)

idx2_Enum(func_level, u8,
  Subband,
  Sum,
  Max
)

namespace idx2 {

/* ---------------------- TYPES ----------------------*/

struct file_id {
  stref Name;
  u64 Id = 0;
};

struct params {
  volume NasaMask;
  action Action = action::Encode;
  metadata Meta;
  v2i Version = v2i(1, 0);
  v3i Dims3 = v3i(256);
  v3i BrickDims3 = v3i(32);
  cstr InputFile = nullptr; // TODO: change this to local storage
  int NLevels = 1;
  f64 Accuracy = 1e-9;
  int BricksPerChunk = 512;
  int ChunksPerFile = 4096;
  int FilesPerDir = 4096;
  /* decode exclusive */
  extent DecodeExtent;
  f64 DecodeAccuracy = 0;
  int DecodePrecision = 0;
  int OutputLevel = 0;
  u8 DecodeMask = 0xFF;
  int QualityLevel = -1;
  cstr OutDir = "."; // TODO: change this to local storage
  cstr InDir = "."; // TODO: change this to local storage
  cstr OutFile = nullptr; // TODO: change this to local storage
  bool Pause = false;
  enum class out_mode { WriteToFile, KeepInMemory, NoOutput };
  out_mode OutMode = out_mode::KeepInMemory;
  bool GroupLevels = false;
  bool GroupBitPlanes = true;
  bool GroupSubLevels = true;
  array<int> RdoLevels;
  int DecodeLevel = 0;
  bool WaveletOnly = false;
  bool ComputeMinMax = false;
};

struct idx2_file {
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
  static constexpr int MaxBrickDim = 256; // so max number of blocks per subband can be represented in 2 bytes
  static constexpr int MaxLevels = 16;
  static constexpr int MaxTformPassesPerLevels = 9;
  static constexpr int MaxSpatialDepth = 4; // we have at most this number of spatial subdivisions
  char Name[32] = {};
  char Field[32] = {};
  v3i Dims3 = v3i(256);
  dtype DType = dtype::__Invalid__;
  v3i BrickDims3 = v3i(32);
  v3i BrickDimsExt3 = v3i(33);
  v3i BlockDims3 = v3i(4);
  v2<i16> BitPlaneRange = v2<i16>(traits<i16>::Max, traits<i16>::Min);
  static constexpr int NTformPasses = 1;
  u64 TformOrder = 0;
  stack_array<v3i, MaxLevels> NBricks3s; // number of bricks per iteration
  stack_array<v3i, MaxLevels> NChunks3s;
  stack_array<v3i, MaxLevels> NFiles3s;
  array<stack_string<128>> BrickOrderStrs;
  array<stack_string<128>> ChunkOrderStrs;
  array<stack_string<128>> FileOrderStrs;
  stack_string<16> TformOrderFull;
  stack_array<stack_array<i8, MaxSpatialDepth>, MaxLevels> FileDirDepths;
  stack_array<u64, MaxLevels> BrickOrders;
  stack_array<u64, MaxLevels> BrickOrderChunks;
  stack_array<u64, MaxLevels> ChunkOrderFiles;
  stack_array<u64, MaxLevels> ChunkOrders;
  stack_array<u64, MaxLevels> FileOrders;
  f64 Accuracy = 0;
  i8 NLevels = 1;
  int FilesPerDir = 4096; // maximum number of files (or sub-directories) per directory
  int BricksPerChunkIn = 512;
  int ChunksPerFileIn = 4096;
  stack_array<int, MaxLevels> BricksPerChunks = {{512}};
  stack_array<int, MaxLevels> ChunksPerFiles = {{4096}};
  stack_array<int, MaxLevels> BricksPerFiles = {{512 * 4096}};
  stack_array<int, MaxLevels> FilesPerVol = {{4096}}; // power of two
  stack_array<int, MaxLevels> ChunksPerVol = {{4096 * 4096}}; // power of two
  v2i Version = v2i(1, 0);
  array<subband> Subbands; // based on BrickDimsExt3
  array<subband> SubbandsNonExt; // based on BrickDims3
  v3i GroupBrick3; // how many bricks in the current iteration form a brick in the next iteration
  stack_array<v3i, MaxLevels> BricksPerChunk3s = {{v3i(8)}};
  stack_array<v3i, MaxLevels> ChunksPerFile3s = {{v3i(16)}};
  transform_details Td; // used for normal transform
  transform_details TdExtrpolate; // used only for extrapolation
  cstr Dir = "./";
  v2d ValueRange = v2d(traits<f64>::Max, traits<f64>::Min);
  array<int> QualityLevelsIn; // [] -> bytes
  array<i64> RdoLevels; // [] -> bytes
  bool GroupLevels = false;
  bool GroupBitPlanes = true;
  bool GroupSubLevels = true;
};

struct brick_volume {
  volume Vol;
  extent ExtentLocal;
  i8 NChildren = 0;
  i8 NChildrenMax = 0;
};

/* ---------------------- GLOBALS ----------------------*/
extern free_list_allocator BrickAlloc_;

/* ---------------------- FUNCTIONS ----------------------*/

grid
GetGrid(const extent& Ext, int Iter, u8 Mask, const array<subband>& Subbands);

void
Dealloc(params* P);

idx2_Inline i64
Size(const brick_volume& B) { return Prod(Dims(B.Vol)) * SizeOf(B.Vol.Type); }

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
SetDir(idx2_file* Idx2, cstr Dir);
void
SetGroupLevels(idx2_file* Idx2, bool GroupLevels);
void
SetGroupSubLevels(idx2_file* Idx2, bool GroupSubLevels);
void
SetGroupBitPlanes(idx2_file* Idx2, bool GroupBitPlanes);
void
SetQualityLevels(idx2_file* Idx2, const array<int>& QualityLevels);
error<idx2_err_code>
Finalize(idx2_file* Idx2);

void
Dealloc(idx2_file* Idx2);

/* -------- VERSION 0 : UNUSED ---------*/
idx2_Inline u64
GetFileAddressV0_0(int BricksPerFile, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
  (void)BricksPerFile;
  (void)Brick;
  (void)Level;
  (void)BitPlane;
  return u64(Iter);
}

idx2_Inline file_id
ConstructFilePathV0_0(const idx2_file& Idx2, u64 Brick, i8 Iter, i8 Level, i16 BitPlane) {
#define idx2_PrintIteration idx2_Print(&Pr, "/I%02x", Iter);
#define idx2_PrintExtension idx2_Print(&Pr, ".bin");
  thread_local static char FilePath[256];
  printer Pr(FilePath, sizeof(FilePath));
  idx2_Print(&Pr, "%s/%s/", Idx2.Name, Idx2.Field);
  idx2_PrintIteration; idx2_PrintExtension;
  u64 FileId = GetFileAddressV0_0(Idx2.BricksPerFiles[Iter], Brick, Iter, Level, BitPlane);
  return file_id{ stref{FilePath, Pr.Size}, FileId };
#undef idx2_PrintIteration
#undef idx2_PrintExtension
}

}
