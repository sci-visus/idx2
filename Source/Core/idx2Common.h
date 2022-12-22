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

#if VISUS_IDX2
#include <functional>
#include <future>
#endif

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
  int NLevels = 0;
  f64 Tolerance = 0;
  int BricksPerChunk = 4096;
  int ChunksPerFile = 64;
  int FilesPerDir = 64;
  int BitPlanesPerChunk = 1;
  int BitPlanesPerFile = 16;
  /* decode exclusive */
  extent DecodeExtent;
  v3i DownsamplingFactor3 = v3i(0); // DownsamplingFactor = [1, 1, 2] means half X, half Y, quarter Z
  f64 DecodeTolerance = 0;
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
  bool ParallelDecode = false;
};


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
static constexpr int MaxTemplatePostfixLength = 48;

struct transform_template
{
  stref Full;
  stref Prefix; // the part not used for resolution indexing
  stack_string<MaxTemplatePostfixLength> Postfix; // with the character ':' removed
  array<v2<i8>> LevelParts; // where in the postfix the levels start: [begin, size]
};


struct subbands_per_level
{
  array<subband> PowerOfTwo;
  array<subband> PowerOfTwoPlusOne;
  u8 DecodeSubbandMasks = 0; // a bit field which specifies which subbands to decode
  array<v3i> DecodeSubbandSpacings;
};


struct brick_info_per_level
{
  v3i Dims3;
  v3i Spacing3;
  v3i Group3;
  v3i NBricks3; // may not be power of two (cropped against the actual domain)
  v3i NBricksPerChunk3; // power of two
  stref Template;
  stref IndexTemplate; // the part that precedes the BrickTemplate
  stref IndexTemplateInChunk; // the part that precedes the BrickTemplate but restricted to a chunk
};


struct chunk_info_per_level
{
  v3i Dims3;
  v3i NChunks3; // may not be power of two (cropped against the actual domain)
  v3i NChunksPerFile3; // power of two
  stref Template;
  stref IndexTemplate;
  stref IndexTemplateInFile;
};


struct file_info_per_level
{
  v3i Dims3;
  v3i NFiles3; // may not be power of two
  stref Template;
  stref IndexTemplate;
  array<i8> FileDirDepths; // how many spatial "bits" are consumed by each file/directory level
};


struct idx2_file
{
  char Name[64] = {};
  v3i Dims3 = v3i(256);
  v3i DownsamplingFactor3 = v3i(0); // TODO NEXT: should be part of params only
  dtype DType = dtype::__Invalid__;
  v3i BlockDims3 = v3i(4);
  v2<i16> BitPlaneRange = v2<i16>(traits<i16>::Max, traits<i16>::Min);
  f64 Tolerance = 0;
  i8 NLevels = 1;
  v2i Version = v2i(1, 0);
  stref Dir; // the directory containing the idx2 dataset // TODO NEXT: should be part of params
  v2d ValueRange = v2d(traits<f64>::Max, traits<f64>::Min);

  transform_template Template;
  array<subbands_per_level> Subbands;
  array<v3i> Dimensions3; // dimensions per level
  array<brick_info_per_level> BrickInfo;
  array<chunk_info_per_level> ChunkInfo;
  array<file_info_per_level>  FileInfo;

  i8 BitsPerBrick = 15;
  i8 BrickBitsPerChunk = 12;
  i8 ChunkBitsPerFile = 6;
  i8 FileBitsPerDir = 9;
  i8 BitPlanesPerChunk = 1;
  i8 BitPlanesPerFile = 16;

#if VISUS_IDX2
  //introducing the future for async-read
  std::function<std::future<bool> (const idx2_file&, buffer&, u64) > external_read;
  //write is always syncronous and slow, don't use this
  std::function<bool(const idx2_file&, buffer&, u64)> external_write;
#endif
};


/* ---------------------- GLOBALS ----------------------*/
extern free_list_allocator BrickAlloc_;


/* ---------------------- FUNCTIONS ----------------------*/

void // TODO: should also return an error?
WriteMetaFile(const idx2_file& Idx2, const params& P, cstr FileName);

error<idx2_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName);

error<idx2_err_code>
ReadMetaFileFromBuffer(idx2_file* Idx2, buffer& Buf);

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
SetNumLevels(idx2_file* Idx2, i8 NIterations);

void
SetTolerance(idx2_file* Idx2, f64 Accuracy);

void
SetChunksPerFile(idx2_file* Idx2, int ChunksPerFile);

void
SetBricksPerChunk(idx2_file* Idx2, int BricksPerChunk);

void
SetFilesPerDirectory(idx2_file* Idx2, int FilesPerDir);

void
SetBitPlanesPerChunk(idx2_file* Idx2, int BitPlanesPerChunk);

void
SetBitPlanesPerFile(idx2_file* Idx2, int BitPlanesPerFile);

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
Finalize(idx2_file* Idx2, params* P);

void
ComputeExtentsForTraversal(const idx2_file& Idx2,
                           const extent& Ext,
                           i8 Level,
                           extent* ExtentInBricks,
                           extent* ExtentInChunks,
                           extent* ExtentInFiles,
                           extent* VolExtentInBricks,
                           extent* VolExtentInChunks,
                           extent* VolExtentInFiles);
void
Dealloc(idx2_file* Idx2);


struct traverse_item
{
  v3i From3, To3;
  u64 TraverseOrder, PrevTraverseOrder;
  u64 Address = 0;
  i32 ItemOrder = 0; // e.g., brick order in chunk, chunk order in file
  bool LastItem = false;
};

struct brick_traverse
{
  u64 BrickOrder, PrevOrder;
  v3i BrickFrom3, BrickTo3;
  i64 NBricksBefore = 0;
  i32 BrickInChunk = 0;
  u64 Address = 0;
};


struct chunk_traverse
{
  u64 ChunkOrder, PrevOrder;
  v3i ChunkFrom3, ChunkTo3;
  i64 NChunksBefore = 0;
  i32 ChunkInFile = 0;
  u64 Address = 0;
};


struct file_traverse
{
  u64 FileOrder, PrevOrder;
  v3i FileFrom3, FileTo3;
  u64 Address = 0;
};


#if !defined(idx2_FileTraverse)
#define idx2_FileTraverse(                                                                         \
  Body, StackSize, FileOrderIn, FileFrom3In, FileDims3In, ExtentInFiles, Extent2)                  \
  {                                                                                                \
    file_traverse FileStack[StackSize];                                                            \
    int FileTopIdx = 0;                                                                            \
    v3i FileDims3Ext(                                                                              \
      (int)NextPow2(FileDims3In.X), (int)NextPow2(FileDims3In.Y), (int)NextPow2(FileDims3In.Z));   \
    FileStack[FileTopIdx] =                                                                        \
      file_traverse{ FileOrderIn, FileOrderIn, FileFrom3In, FileFrom3In + FileDims3Ext, u64(0) };  \
    while (FileTopIdx >= 0)                                                                        \
    {                                                                                              \
      file_traverse& FileTop = FileStack[FileTopIdx];                                              \
      int FD = FileTop.FileOrder & 0x3;                                                            \
      FileTop.FileOrder >>= 2;                                                                     \
      if (FD == 3)                                                                                 \
      {                                                                                            \
        if (FileTop.FileOrder == 3)                                                                \
          FileTop.FileOrder = FileTop.PrevOrder;                                                   \
        else                                                                                       \
          FileTop.PrevOrder = FileTop.FileOrder;                                                   \
        continue;                                                                                  \
      }                                                                                            \
      --FileTopIdx;                                                                                \
      if (FileTop.FileTo3 - FileTop.FileFrom3 == 1)                                                \
      {                                                                                            \
        {                                                                                          \
          Body                                                                                     \
        }                                                                                          \
        continue;                                                                                  \
      }                                                                                            \
      file_traverse First = FileTop, Second = FileTop;                                             \
      First.FileTo3[FD] =                                                                          \
        FileTop.FileFrom3[FD] + (FileTop.FileTo3[FD] - FileTop.FileFrom3[FD]) / 2;                 \
      Second.FileFrom3[FD] =                                                                       \
        FileTop.FileFrom3[FD] + (FileTop.FileTo3[FD] - FileTop.FileFrom3[FD]) / 2;                 \
      extent Skip(First.FileFrom3, First.FileTo3 - First.FileFrom3);                               \
      First.Address = FileTop.Address;                                                             \
      Second.Address = FileTop.Address + Prod<u64>(First.FileTo3 - First.FileFrom3);               \
      if (Second.FileFrom3 < To(ExtentInFiles) && From(ExtentInFiles) < Second.FileTo3)            \
        FileStack[++FileTopIdx] = Second;                                                          \
      if (First.FileFrom3 < To(ExtentInFiles) && From(ExtentInFiles) < First.FileTo3)              \
        FileStack[++FileTopIdx] = First;                                                           \
    }                                                                                              \
  }
#endif

#if !defined(idx2ChunkTraverse)
#define idx2_ChunkTraverse(                                                                        \
  Body, StackSize, ChunkOrderIn, ChunkFrom3In, ChunkDims3In, ExtentInChunks, Extent2)              \
  {                                                                                                \
    chunk_traverse ChunkStack[StackSize];                                                          \
    int ChunkTopIdx = 0;                                                                           \
    v3i ChunkDims3Ext((int)NextPow2(ChunkDims3In.X),                                               \
                      (int)NextPow2(ChunkDims3In.Y),                                               \
                      (int)NextPow2(ChunkDims3In.Z));                                              \
    ChunkStack[ChunkTopIdx] = chunk_traverse{                                                      \
      ChunkOrderIn, ChunkOrderIn, ChunkFrom3In, ChunkFrom3In + ChunkDims3Ext, u64(0)               \
    };                                                                                             \
    while (ChunkTopIdx >= 0)                                                                       \
    {                                                                                              \
      chunk_traverse& ChunkTop = ChunkStack[ChunkTopIdx];                                          \
      int CD = ChunkTop.ChunkOrder & 0x3;                                                          \
      ChunkTop.ChunkOrder >>= 2;                                                                   \
      if (CD == 3)                                                                                 \
      {                                                                                            \
        if (ChunkTop.ChunkOrder == 3)                                                              \
          ChunkTop.ChunkOrder = ChunkTop.PrevOrder;                                                \
        else                                                                                       \
          ChunkTop.PrevOrder = ChunkTop.ChunkOrder;                                                \
        continue;                                                                                  \
      }                                                                                            \
      --ChunkTopIdx;                                                                               \
      if (ChunkTop.ChunkTo3 - ChunkTop.ChunkFrom3 == 1)                                            \
      {                                                                                            \
        {                                                                                          \
          Body                                                                                     \
        }                                                                                          \
        continue;                                                                                  \
      }                                                                                            \
      chunk_traverse First = ChunkTop, Second = ChunkTop;                                          \
      First.ChunkTo3[CD] =                                                                         \
        ChunkTop.ChunkFrom3[CD] + (ChunkTop.ChunkTo3[CD] - ChunkTop.ChunkFrom3[CD]) / 2;           \
      Second.ChunkFrom3[CD] =                                                                      \
        ChunkTop.ChunkFrom3[CD] + (ChunkTop.ChunkTo3[CD] - ChunkTop.ChunkFrom3[CD]) / 2;           \
      extent Skip(First.ChunkFrom3, First.ChunkTo3 - First.ChunkFrom3);                            \
      Second.NChunksBefore = First.NChunksBefore + Prod<u64>(Dims(Crop(Skip, ExtentInChunks)));    \
      Second.ChunkInFile = First.ChunkInFile + Prod<i32>(Dims(Crop(Skip, Extent2)));               \
      First.Address = ChunkTop.Address;                                                            \
      Second.Address = ChunkTop.Address + Prod<u64>(First.ChunkTo3 - First.ChunkFrom3);            \
      if (Second.ChunkFrom3 < To(ExtentInChunks) && From(ExtentInChunks) < Second.ChunkTo3)        \
        ChunkStack[++ChunkTopIdx] = Second;                                                        \
      if (First.ChunkFrom3 < To(ExtentInChunks) && From(ExtentInChunks) < First.ChunkTo3)          \
        ChunkStack[++ChunkTopIdx] = First;                                                         \
    }                                                                                              \
  }
#endif

#if !defined(idx2_BrickTraverse)
#define idx2_BrickTraverse(                                                                        \
  Body, StackSize, BrickOrderIn, BrickFrom3In, BrickDims3In, ExtentInBricks, Extent2)              \
  {                                                                                                \
    brick_traverse Stack[StackSize];                                                               \
    int TopIdx = 0;                                                                                \
    v3i BrickDims3Ext((int)NextPow2(BrickDims3In.X),                                               \
                      (int)NextPow2(BrickDims3In.Y),                                               \
                      (int)NextPow2(BrickDims3In.Z));                                              \
    Stack[TopIdx] = brick_traverse{                                                                \
      BrickOrderIn, BrickOrderIn, BrickFrom3In, BrickFrom3In + BrickDims3Ext, u64(0)               \
    };                                                                                             \
    while (TopIdx >= 0)                                                                            \
    {                                                                                              \
      brick_traverse& Top = Stack[TopIdx];                                                         \
      int DD = Top.BrickOrder & 0x3;                                                               \
      Top.BrickOrder >>= 2;                                                                        \
      if (DD == 3)                                                                                 \
      {                                                                                            \
        if (Top.BrickOrder == 3)                                                                   \
          Top.BrickOrder = Top.PrevOrder;                                                          \
        else                                                                                       \
          Top.PrevOrder = Top.BrickOrder;                                                          \
        continue;                                                                                  \
      }                                                                                            \
      --TopIdx;                                                                                    \
      if (Top.BrickTo3 - Top.BrickFrom3 == 1)                                                      \
      {                                                                                            \
        {                                                                                          \
          Body                                                                                     \
        }                                                                                          \
        continue;                                                                                  \
      }                                                                                            \
      brick_traverse First = Top, Second = Top;                                                    \
      First.BrickTo3[DD] = Top.BrickFrom3[DD] + (Top.BrickTo3[DD] - Top.BrickFrom3[DD]) / 2;       \
      Second.BrickFrom3[DD] = Top.BrickFrom3[DD] + (Top.BrickTo3[DD] - Top.BrickFrom3[DD]) / 2;    \
      extent Skip(First.BrickFrom3, First.BrickTo3 - First.BrickFrom3);                            \
      Second.NBricksBefore = First.NBricksBefore + Prod<u64>(Dims(Crop(Skip, ExtentInBricks)));    \
      Second.BrickInChunk = First.BrickInChunk + Prod<i32>(Dims(Crop(Skip, Extent2)));             \
      First.Address = Top.Address;                                                                 \
      Second.Address = Top.Address + Prod<u64>(First.BrickTo3 - First.BrickFrom3);                 \
      if (Second.BrickFrom3 < To(ExtentInBricks) && From(ExtentInBricks) < Second.BrickTo3)        \
        Stack[++TopIdx] = Second;                                                                  \
      if (First.BrickFrom3 < To(ExtentInBricks) && From(ExtentInBricks) < First.BrickTo3)          \
        Stack[++TopIdx] = First;                                                                   \
    }                                                                                              \
  }
#endif

} // namespace idx2

