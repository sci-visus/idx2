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
#include "nd_volume.h"

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

struct dimension_info
{
  /* A dimension can either be numerical or categorical.
  In the latter case, each category is given a name (e.g., a field).
  In the former case, the dimension starts at 0 and has an upper limit. */
  array<stref> Names;
  i32 Limit = 0; // exclusive upper limit
  char ShortName = '?';
};


idx2_Inline i32
Size(const dimension_info& Dim)
{
  return Size(Dim.Names) > 0 ? i32(Size(Dim.Names)) : Dim.Limit;
}


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


static constexpr i8 MaxNumDimensions_ = nd_size::Size();


idx2_Inline i8
Size(const template_view& TemplateView)
{
  return TemplateView.Size;
}


struct transform_template
{
  template_str Original; // e.g., xyzxyz:xyz:xyz
  template_int Processed; // 012012012012
  stack_array<template_view, MaxNumLevels_> LevelViews; // where the levels are in the Processed template
};


struct subbands_per_level
{
  stack_array<subband, MaxNumSubbandsPerLevel_> Pow2;
  stack_array<subband, MaxNumSubbandsPerLevel_> Pow2Plus1;
  u8 DecodeMasks = 0; // a bit field which specifies which subbands to decode
  stack_array<nd_size, MaxNumSubbandsPerLevel_> Spacings;
};


struct brick_info_per_level
{
  nd_size DimsPow2;
  nd_size DimsPow2Plus1;
  nd_size Spacing;
  nd_size Group;
  nd_size NBricks; // may not be power of two (cropped against the actual domain)
  nd_size NBricksPerChunk; // power of two
  template_view Template;
  template_view IndexTemplate; // the part that precedes the BrickTemplate
  template_view IndexTemplateInChunk; // the part that precedes the BrickTemplate but restricted to a chunk
};


struct chunk_info_per_level
{
  nd_size Dims;
  nd_size NChunks; // may not be power of two (cropped against the actual domain)
  nd_size NChunksPerFile; // power of two
  template_view Template;
  template_view IndexTemplate;
  template_view IndexTemplateInFile;
};


struct file_info_per_level
{
  nd_size Dims;
  nd_size NFiles; // may not be power of two
  template_view Template;
  template_view IndexTemplate;
  // TODO NEXT
  stack_array<i8, 8> FileDirDepths; // how many spatial "bits" are consumed by each file/directory level
};


struct idx2_file
{
  static constexpr v3i BlockDims3 = v3i(4);

  v2i Version = v2i(2, 0);
  stack_string<64> Name;
  nd_size Dims;
  dtype DType = dtype::__Invalid__;
  f64 Tolerance = 0;
  i8 NLevels = 1;
  v2d ValueRange = v2d(traits<f64>::Max, traits<f64>::Min);

  stack_array<dimension_info, MaxNumDimensions_> DimensionInfo; // TODO NEXT: initialize this
  stack_array<i8, 'z' - 'a' + 1> DimensionMap; // map from ['a' - 'a', 'z' - 'a'] -> [0, Size(Idx2->Dimensions)]
  transform_template Template;
  stack_array<subbands_per_level, MaxNumLevels_> Subbands;
  stack_array<brick_info_per_level, MaxNumLevels_> BrickInfo;
  stack_array<chunk_info_per_level, MaxNumLevels_> ChunkInfo;
  stack_array<file_info_per_level, MaxNumLevels_>  FileInfo;

  i8 BitsPerBrick = 15;
  i8 BrickBitsPerChunk = 12;
  i8 ChunkBitsPerFile = 6;
  i8 FileBitsPerDir = 9;
  i8 BitPlanesPerChunk = 1;
  i8 BitPlanesPerFile = 16;
};


/* ---------------------- GLOBALS ----------------------*/
extern free_list_allocator BrickAlloc_;


/* ---------------------- FUNCTIONS ----------------------*/


/* e.g., xyzxyz -> v3i(4, 4, 4) */
nd_size
Dims(const template_view& Template);


void // TODO: should also return an error?
WriteMetaFile(const idx2_file& Idx2, const params& P, cstr FileName);


error<idx2_err_code>
ReadMetaFile(idx2_file* Idx2, cstr FileName);


error<idx2_err_code>
ReadMetaFileFromBuffer(idx2_file* Idx2, buffer& Buf);


enum class template_hint
{
  Isotropic, // alternate the dimensions at the end of the template
  Anisotropic, // alternate the dimensions at the beginning of the template
  Size
};


template_str
GuessTransformTemplate(const idx2_file& Idx2, template_hint Hint);


/* Compute the output grid (from, dims, strides) */
grid
GetGrid(const idx2_file& Idx2, const nd_extent& Ext);


void
Dealloc(params* P);


error<idx2_err_code>
Finalize(idx2_file* Idx2, params* P);


void
ComputeExtentsForTraversal(const idx2_file& Idx2,
                           const nd_extent& Ext,
                           i8 Level,
                           nd_extent* ExtentInBricks,
                           nd_extent* ExtentInChunks,
                           nd_extent* ExtentInFiles,
                           nd_extent* VolExtentInBricks,
                           nd_extent* VolExtentInChunks,
                           nd_extent* VolExtentInFiles);


void
Dealloc(idx2_file* Idx2);


struct traverse_item
{
  nd_size From, To;
  i8 Pos = 0;
  u64 Address = 0;
  i32 ItemOrder = 0; // e.g., brick order in chunk, chunk order in file
  bool LastItem = false;
};


struct file_chunk_brick_traversal;


using traverse_callback = error<idx2_err_code> (const file_chunk_brick_traversal&, const traverse_item&);


struct file_chunk_brick_traversal
{
  const idx2_file* Idx2;
  const nd_extent* Extent;
  i8 Level;
  nd_extent ExtentInBricks;
  nd_extent ExtentInChunks;
  nd_extent ExtentInFiles;
  nd_extent VolExtentInBricks;
  nd_extent VolExtentInChunks;
  nd_extent VolExtentInFiles;
  traverse_callback* BrickCallback;

  file_chunk_brick_traversal(const idx2_file* Idx2,
                             const nd_extent* Extent,
                             i8 Level,
                             traverse_callback* BrickCallback);

  error<idx2_err_code> Traverse(stref Template,
                                const nd_size& From3,
                                const nd_size& Dims3,
                                const nd_extent& Extent,
                                const nd_extent& VolExtent,
                                traverse_callback* Callback) const;
};


error<idx2_err_code>
TraverseFiles(const file_chunk_brick_traversal& Traversal);


error<idx2_err_code>
TraverseChunks(const file_chunk_brick_traversal& Traversal, const traverse_item& FileTop);


error<idx2_err_code>
TraverseBricks(const file_chunk_brick_traversal& Traversal, traverse_item& ChunkTop);


} // namespace idx2

