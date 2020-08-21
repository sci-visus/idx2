#pragma once

#include "idx2_common.h"
#include "idx2_array.h"
#include "idx2_bitstream.h"
#include "idx2_dataset.h"
#include "idx2_error.h"
#include "idx2_hashtable.h"
#include "idx2_wavelet.h"
#include "idx2_volume.h"

idx2_Enum(action, u8, Encode, Decode)
idx2_Enum(idx2_file_err_code, u8, idx2_CommonErrs,
  BrickSizeNotPowerOfTwo, BrickSizeTooBig, TooManyIterations, TooManyTransformPassesPerIteration,
  TooManyIterationsOrTransformPasses, TooManyBricksPerFile, TooManyFilesPerDir, NotSupportedInVersion,
  CannotCreateDirectory, SyntaxError, TooManyBricksPerChunk, TooManyChunksPerFile, ChunksPerFileNotPowerOf2,
  BricksPerChunkNotPowerOf2, ChunkNotFound, BrickNotFound, FileNotFound)
idx2_Enum(func_level, u8, Subband, Sum, Max)

namespace idx2 {

// By default, decode everything
struct decode_params {
  bool Decode = false;
  f64 Accuracy = 0;
  u8 Mask = 0xFF; // for now just a mask TODO: to generalize this we need an array of subbands
  i8 Precision = 64; // number of bit planes to decode
};

struct params {
  action Action = action::Encode;
  metadata Meta;
  v2i Version = v2i(1, 0);
  v3i Dims3 = v3i(256);
  v3i BrickDims3 = v3i(32);
  cstr InputFile = nullptr;
  cstr OutputFile = nullptr;
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
  cstr OutDir = ".";
  cstr InDir = ".";
  cstr OutFile = nullptr;
  bool Pause = false;
  enum class out_mode { WriteToFile, KeepInMemory, NoOutput };
  out_mode OutMode = out_mode::KeepInMemory;
  bool GroupLevels = false;
  bool GroupBitPlanes = true;
  bool GroupSubLevels = true;
  array<int> RdoLevels;
  int DecodeLevel = 0;
  bool WaveletOnly = false;
//  bool WaveletOnly = true;
};

void Dealloc(params* P);


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
  transform_details Td;
  transform_details TdExtrpolate;
  cstr Dir = "./";
  v2d ValueRange = v2d(traits<f64>::Max, traits<f64>::Min);
  array<int> QualityLevelsIn; // [] -> bytes
  array<i64> RdoLevels; // [] -> bytes
  bool GroupLevels = false;
  bool GroupBitPlanes = true;
  bool GroupSubLevels = true;
};

idx2_T(t)
struct brick {
  t* Samples = nullptr; // TODO: data should stay compressed
  u8 LevelMask = 0; // TODO: need to change if we support more than one transform pass per brick
//  stack_array<array<u8>, 8> BlockSigs; // TODO: to support more than one transform pass per brick, we need a dynamic array
  // friend v3i Dims(const brick<t>& Brick, const array<grid>& LevelGrids);
  // friend t& At(const brick<t>& Brick, array<grid>& LevelGrids, const v3i& P3);
};

idx2_T(t)
struct brick_table {
  hash_table<u64, brick<t>> Bricks; // hash from BrickKey to Brick
  allocator* Alloc = &Mallocator();
  // TODO: let Enc->Alloc follow this allocator
};
idx2_T(t) void GetBrick(brick_table<t>* BrickTable, i8 Iter, u64 Brick) {
  //auto
  (void)BrickTable;
  (void)Iter;
  (void)Brick;
}
idx2_T(t) void Dealloc(brick_table<t>* BrickTable);

idx2_Ti(t) v3i
Dims(const brick<t>& Brick, const array<grid>& LevelGrids) {
  return Dims(LevelGrids[Brick.Level]);
}

idx2_Ti(t) t&
At(const brick<t>& Brick, array<grid>& LevelGrids, const v3i& P3) {
  v3i D3 = Dims(LevelGrids[Brick.Level]);
  idx2_Assert(P3 < D3);
  idx2_Assert(D3 == Dims(Brick));
  i64 Idx = Row(D3, P3);
  return const_cast<t&>(Brick.Samples[Idx]);
}

idx2_T(t) void
Dealloc(brick<t>* Brick) { free(Brick->Samples); } // TODO: check this

struct decode_what {
  virtual void Init(const idx2_file& Idx2) = 0;
  virtual void SetExtent(const extent& Ext) = 0;
  virtual void SetMask(u8 Mask) = 0;
  virtual void SetIteration(int Iteration) = 0;
  virtual void SetAccuracy(f64 Accuracy) = 0;
  virtual void SetQuality(int Quality) = 0;
  virtual extent GetExtent() const = 0;
  virtual int GetIteration() const = 0;
  virtual u8 GetMask() const = 0;
  virtual f64 GetAccuracy() const = 0;
  virtual int GetQuality() const = 0;
//  virtual decode_params GetDecodeParams(int Iteration, const v3i& Brick3) const = 0;
  virtual void Destroy() = 0;
};

struct decode_all : public decode_what {
  const idx2_file* Idx2 = nullptr;
  extent Ext;
  f64 Accuracy = 0;
  int Iter = 0;
  u8 Mask = 0xFF;
  int QualityLevel = -1;
  void Init(const idx2_file& Idx2) override;
  void SetExtent(const extent& Ext) override;
  void SetMask(u8 Mask) override;
  void SetIteration(int Iteration) override;
  void SetAccuracy(f64 Accuracy) override;
  void SetQuality(int Quality) override;
  f64 GetAccuracy() const override;
  int GetIteration() const override;
  extent GetExtent() const override;
  u8 GetMask() const override;
  int GetQuality() const override;
//  decode_params GetDecodeParams(int Iteration, const v3i &Brick3) const override;
  void Destroy() override;
};

struct brick_volume {
  volume Vol;
  extent ExtentLocal;
  i8 NChildren = 0;
  i8 NChildrenMax = 0;
};
idx2_Inline i64 Size(const brick_volume& B) { return Prod(Dims(B.Vol)) * SizeOf(B.Vol.Type); }

// Each channel corresponds to one (iteration, subband, bit plane) tuple
struct channel {
  /* brick-related streams, to be reset once per chunk */
  bitstream BrickDeltasStream; // store data for many bricks
  bitstream BrickSzsStream; // store data for many bricks
  bitstream BrickStream; // store data for many bricks
  /* block-related streams, to be reset once per brick */
  bitstream BlockStream; // store data for many blocks
  u64 LastChunk = 0; // current chunk
  u64 LastBrick = 0;
  i32 NBricks = 0;
};

struct rdo_chunk {
  u64 Address;
  i64 Length;
  f64 Lambda;
  idx2_Inline bool operator<(const rdo_chunk& Other) const { return Address > Other.Address; }
};

// Each sub-channel corresponds to one (iteration, subband) tuple
struct sub_channel {
  bitstream BlockEMaxesStream;
  bitstream BrickEMaxesStream; // at the end of each brick we copy from BlockEMaxesStream to here
  u64 LastChunk = 0;
  u64 LastBrick = 0;
};
struct chunk_meta_info {
  array<u64> Addrs; // iteration, level, bit plane, chunk id
  bitstream Sizes; // TODO: do we need to init this?
};

struct block_sig {
  u32 Block = 0;
  i16 BitPlane = 0;
};

/* We use this to pass data between different stages of the encoder */
struct encode_data {
  allocator* Alloc = nullptr;
  hash_table<u64, brick_volume> BrickPool;
  hash_table<u32, channel> Channels; // each corresponds to (bit plane, iteration, level)
  hash_table<u16, sub_channel> SubChannels; // only consider level and iteration
  i8 Iter = 0;
  i8 Level = 0;
  stack_array<u64, idx2_file::MaxLevels> Brick;
  stack_array<v3i, idx2_file::MaxLevels> Bricks3;
  hash_table<u64, chunk_meta_info> ChunkMeta; // map from file address to chunk info
  hash_table<u64, bitstream> ChunkEMaxesMeta; // map from file address to a stream of chunk emax sizes
  bitstream CpresEMaxes;
  bitstream CpresChunkAddrs;
  bitstream ChunkStream;
  /* block emaxes related */
  bitstream ChunkEMaxesStream;
  array<block_sig> BlockSigs;
  array<i16> EMaxes;
  bitstream BlockStream; // only used by v0.1
  array<t2<u32, channel*>> SortedChannels;
  array<rdo_chunk> ChunkRDOs; // list of chunks and their sizes, sorted by bit plane
  hash_table<u64, u32> ChunkRDOLengths;
};

struct chunk_exp_cache {
  bitstream BrickExpsStream;
};
idx2_Inline i64 Size(const chunk_exp_cache& ChunkExpCache) { return Size(ChunkExpCache.BrickExpsStream.Stream); }
struct chunk_rdo_cache {
  array<i16> TruncationPoints;
};
idx2_Inline i64 Size(const chunk_rdo_cache& ChunkRdoCache) { return Size(ChunkRdoCache.TruncationPoints) * sizeof(i16); }

struct chunk_cache {
  i32 ChunkPos; // chunk position in the file
  array<u64> Bricks;
  array<i32> BrickSzs;
  bitstream ChunkStream;
};
idx2_Inline i64 Size(const chunk_cache& C) { return Size(C.Bricks) * sizeof(u64) + Size(C.BrickSzs) * sizeof(i32) + sizeof(C.ChunkPos) + Size(C.ChunkStream.Stream); }
struct file_exp_cache {
  array<chunk_exp_cache> ChunkExpCaches;
  array<i32> ChunkExpSzs;
};
idx2_Inline i64 Size(const file_exp_cache& F) {
  i64 Result = 0;
  idx2_ForEach(It, F.ChunkExpCaches) Result += Size(*It);
  Result += Size(F.ChunkExpSzs) * sizeof(i32);
  return Result;
}
struct file_rdo_cache {
  array<chunk_rdo_cache> TileRdoCaches;
};
idx2_Inline i64 Size(const file_rdo_cache& F) {
  i64 Result = 0;
  idx2_ForEach(It, F.TileRdoCaches) Result += Size(*It);
  return Result;
}
struct file_cache {
  array<i64> ChunkSizes; // TODO: 32-bit to store chunk sizes?
  hash_table<u64, chunk_cache> ChunkCaches; // [chunk address] -> chunk cache
};
idx2_Inline i64 Size(const file_cache& F) {
  i64 Result = 0;
  Result += Size(F.ChunkSizes) * sizeof(i64);
  idx2_ForEach(It, F.ChunkCaches) Result += Size(*It.Val);
  return Result;
}
struct file_cache_table {
  hash_table<u64, file_cache> FileCaches; // [file address] -> file cache
  hash_table<u64, file_exp_cache> FileExpCaches; // [file exp address] -> file exp cache
  hash_table<u64, file_rdo_cache> FileRdoCaches; // [file rdo address] -> file rdo cache
};
idx2_Inline i64 Size(const file_cache_table& F) {
  i64 Result = 0;
  idx2_ForEach(It, F.FileCaches) Result += Size(*It.Val);
  idx2_ForEach(It, F.FileExpCaches) Result += Size(*It.Val);
  idx2_ForEach(It, F.FileRdoCaches) Result += Size(*It.Val);
  return Result;
}
struct decode_data {
  allocator* Alloc = nullptr;
  file_cache_table FcTable;
  hash_table<u64, brick_volume> BrickPool;
  i8 Iter = 0;
  i8 Level = 0;
  stack_array<u64, idx2_file::MaxLevels> Brick;
  stack_array<v3i, idx2_file::MaxLevels> Bricks3;
  i32 ChunkInFile = 0;
  i32 BrickInChunk = 0;
  stack_array<u64, idx2_file::MaxLevels> Offsets = {{}}; // used by v0.0 only
  bitstream BlockStream; // used only by v0.1
  hash_table<i16, bitstream> Streams;
  buffer CompressedChunkExps;
  bitstream ChunkEMaxSzsStream;
  bitstream ChunkAddrsStream;
  bitstream ChunkSzsStream;
//  array<t2<u64, u64>> RequestedChunks; // is cleared after each tile
  int QualityLevel = -1;
  int EffIter = 0;
  u64 LastTile = 0;
};
idx2_Inline i64 SizeBrickPool(const decode_data& D) {
  i64 Result = 0;
  idx2_ForEach(It, D.BrickPool) Result += Size(*It.Val);
  return Result;
}

struct brick_traverse {
  u64 BrickOrder, PrevOrder;
  v3i BrickFrom3, BrickTo3;
  i64 NBricksBefore = 0;
  i32 BrickInChunk = 0;
  u64 Address = 0;
};
struct chunk_traverse {
  u64 ChunkOrder, PrevOrder;
  v3i ChunkFrom3, ChunkTo3;
  i64 NChunksBefore = 0;
  i32 ChunkInFile = 0;
  u64 Address = 0;
};
struct file_traverse {
  u64 FileOrder, PrevOrder;
  v3i FileFrom3, FileTo3;
  u64 Address = 0;
};
struct file_id {
  stref Name;
  u64 Id = 0;
};

void SetName(idx2_file* Idx2, cstr Name);
void SetField(idx2_file* Idx2, cstr Field);
void SetVersion(idx2_file* Idx2, const v2i& Ver);
void SetDimensions(idx2_file* Idx2, const v3i& Dims3);
void SetDataType(idx2_file* Idx2, dtype DType);
void SetBrickSize(idx2_file* Idx2, const v3i& BrickDims3);
void SetNumIterations(idx2_file* Idx2, i8 NIterations);
void SetAccuracy(idx2_file* Idx2, f64 Accuracy);
void SetChunksPerFile(idx2_file* Idx2, int ChunksPerFile);
void SetBricksPerChunk(idx2_file* Idx2, int BricksPerChunk);
void SetFilesPerDirectory(idx2_file* Idx2, int FilesPerDir);
void SetDir(idx2_file* Idx2, cstr Dir);
error<idx2_file_err_code> Finalize(idx2_file* Idx2);
void Dealloc(idx2_file* Idx2);
void SetGroupLevels(idx2_file* Idx2, bool GroupLevels);
void SetGroupSubLevels(idx2_file* Idx2, bool GroupSubLevels);
void SetGroupBitPlanes(idx2_file* Idx2, bool GroupBitPlanes);
void SetQualityLevels(idx2_file* Idx2, const array<int>& QualityLevels);

error<idx2_file_err_code> ReadMetaFile(idx2_file* Idx2, cstr FileName);
void WriteMetaFile(const idx2_file& Idx2, cstr FileName);

// TODO: return an error code?
error<idx2_file_err_code> Encode(idx2_file* Idx2, const params& P, const volume& Vol);
void Decode(const idx2_file& Idx2, const params& P, buffer* OutBuf = nullptr);
void EncodeSubbandV0_0(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol);
error<idx2_file_err_code> DecodeSubbandV0_0(const idx2_file& Idx2, decode_data* D, const grid& SbGrid, volume* BVol);
void EncodeSubbandV0_1(idx2_file* Idx2, encode_data* E, const grid& SbGrid, volume* BrickVol);
error<idx2_file_err_code> DecodeSubbandV0_1(const idx2_file& Idx2, decode_data* D, const grid& SbGrid, volume* BVol);

} // namespace idx2
