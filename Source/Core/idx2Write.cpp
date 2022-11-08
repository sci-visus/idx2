#include "idx2Common.h"
#include "idx2Encode.h"
#include "idx2Lookup.h"
#include "InputOutput.h"
#include "FileSystem.h"
#include "Statistics.h"
#include "VarInt.h"

#include <iostream>

namespace idx2
{


/* book-keeping stuffs */
static stat BlockStat;
static stat BlockEMaxStat;
static stat BrickEMaxesStat;
static stat ChunkEMaxesStat;
static stat ChunkEMaxSzsStat;
static stat BrickDeltasStat;
static stat BrickSzsStat;
static stat BrickStreamStat;
static stat ChunkStreamStat;
static stat CpresChunkAddrsStat;
static stat ChunkAddrsStat;
static stat ChunkSzsStat;


// Write once per chunk
void
WriteChunkExponents(const idx2_file& Idx2, encode_data* E, sub_channel* Sc, i8 Iter, i8 Level)
{
  /* brick exponents */
  Flush(&Sc->BrickEMaxesStream);
  BrickEMaxesStat.Add((f64)Size(Sc->BrickEMaxesStream));
  Rewind(&E->ChunkEMaxesStream);
  CompressBufZstd(ToBuffer(Sc->BrickEMaxesStream), &E->ChunkEMaxesStream);
//  PushBack(&E->FileEMaxBuffer, E->ChunkEMaxesStream.Stream.Data, Size(E->ChunkEMaxesStream));
  ChunkEMaxesStat.Add((f64)Size(E->ChunkEMaxesStream));

  /* rewind */
  Rewind(&Sc->BrickEMaxesStream);

  /* write to file */
  file_id FileId = ConstructFilePath(Idx2, Sc->LastBrick, Iter, Level, ExponentBitPlane_);
  // TODO: have one file emax buffer for each file
  //idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  //WriteBuffer(Fp, ToBuffer(E->ChunkEMaxesStream));
  /* keep track of the chunk sizes */
  auto CemIt = Lookup(&E->ChunkEMaxesMeta, FileId.Id);
  if (!CemIt)
  {
    chunk_emax_info ChunkEMaxInfo;
    InitWrite(&ChunkEMaxInfo.EMaxSizes, 128);
    //Init(&ChunkEMaxInfo.FileEMaxBuffer, 128);
    Insert(&CemIt, FileId.Id, ChunkEMaxInfo);
  }
  bitstream* ChunkEMaxSzs = &CemIt.Val->EMaxSizes;
  GrowToAccomodate(ChunkEMaxSzs, 4);
  // write the size of the exponent stream for current chunk
  WriteVarByte(ChunkEMaxSzs, Size(E->ChunkEMaxesStream));
  array<u8>* EMaxBuffer = &CemIt.Val->FileEMaxBuffer;
  // write the exponents to the exponent buffer for the whole file
  PushBack(EMaxBuffer, E->ChunkEMaxesStream.Stream.Data, Size(E->ChunkEMaxesStream));

  u64 ChunkAddress = GetChunkAddress(Idx2, Sc->LastBrick, Iter, Level, ExponentBitPlane_);
  Insert(&E->ChunkRDOLengths, ChunkAddress, (u32)Size(E->ChunkEMaxesStream));

  Rewind(&E->ChunkEMaxesStream);
}


// TODO: check the error path
error<idx2_err_code>
FlushChunkExponents(const idx2_file& Idx2, encode_data* E)
{
  idx2_ForEach (ScIt, E->SubChannels)
  {
    i8 Iteration = IterationFromChannelKey(*ScIt.Key);
    i8 Level = LevelFromChannelKey(*ScIt.Key);
    WriteChunkExponents(Idx2, E, ScIt.Val, Iteration, Level);
  }
  // TODO: deallocate the file emax buffer after it is flushed to a file
  // TODO: need to "interleave" this with FlushChunk
  // TODO: detect that we are done with a file to flush it as soon as possible instead of at the end (maybe count the number of chunks in a file)
  // TODO: as soon as we get out of a spatial domain for a file, flush every files in the buffer
  // (since we know that all files in the buffer cannot be traversed again in the spatial DFS order)
  idx2_ForEach (CemIt, E->ChunkEMaxesMeta)
  {
    bitstream* ChunkEMaxSzs = &CemIt.Val->EMaxSizes;
    file_id FileId = ConstructFilePath(Idx2, *CemIt.Key);
    idx2_Assert(FileId.Id == *CemIt.Key);
    /* write chunk emax sizes */
    idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
    Flush(ChunkEMaxSzs);
    ChunkEMaxSzsStat.Add((f64)Size(*ChunkEMaxSzs));
    // write the exponent buffer
    WriteBuffer(Fp, ToBuffer(CemIt.Val->FileEMaxBuffer));
    // write the (compressed) sizes of the exponents
    WriteBuffer(Fp, ToBuffer(*ChunkEMaxSzs));
    // write the size (in bytes) of the compressed exponent sizes
    WritePOD(Fp, (int)Size(*ChunkEMaxSzs));
    // write the total number of bytes used for storing the exponents
    auto TotalExpBytes = ToBuffer(CemIt.Val->FileEMaxBuffer).Bytes + ToBuffer(*ChunkEMaxSzs).Bytes + sizeof(int);
    WritePOD(Fp, (int)TotalExpBytes);
  }

  return idx2_Error(idx2_err_code::NoError);
}


// TODO: return error
void
WriteChunk(const idx2_file& Idx2, encode_data* E, channel* C, i8 Iter, i8 Level, i16 BitPlane)
{
  BrickDeltasStat.Add((f64)Size(C->BrickDeltasStream)); // brick deltas
  BrickSzsStat.Add((f64)Size(C->BrickSzsStream));       // brick sizes
  BrickStreamStat.Add((f64)Size(C->BrickStream));       // brick data
  i64 ChunkSize = Size(C->BrickDeltasStream) + Size(C->BrickSzsStream) + Size(C->BrickStream) + 64;
  Rewind(&E->ChunkStream);
  GrowToAccomodate(&E->ChunkStream, ChunkSize);
  WriteVarByte(&E->ChunkStream, C->NBricks);
  WriteStream(&E->ChunkStream, &C->BrickDeltasStream);
  WriteStream(&E->ChunkStream, &C->BrickSzsStream);
  WriteStream(&E->ChunkStream, &C->BrickStream);
  Flush(&E->ChunkStream);
  ChunkStreamStat.Add((f64)Size(E->ChunkStream));

  /* we are done with these, rewind */
  Rewind(&C->BrickDeltasStream);
  Rewind(&C->BrickSzsStream);
  Rewind(&C->BrickStream);

  /* write to file */
  file_id FileId = ConstructFilePath(Idx2, C->LastBrick, Iter, Level, BitPlane);
  idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
  WriteBuffer(Fp, ToBuffer(E->ChunkStream));
  /* keep track of the chunk addresses and sizes */
  auto ChunkMetaIt = Lookup(&E->ChunkMeta, FileId.Id);
  if (!ChunkMetaIt)
  {
    chunk_meta_info Cm;
    InitWrite(&Cm.Sizes, 128);
    Insert(&ChunkMetaIt, FileId.Id, Cm);
  }
  idx2_Assert(ChunkMetaIt);
  chunk_meta_info* ChunkMeta = ChunkMetaIt.Val;
  GrowToAccomodate(&ChunkMeta->Sizes, 4);
  // Write the size of the chunk stream
  WriteVarByte(&ChunkMeta->Sizes, Size(E->ChunkStream));
  u64 ChunkAddress = GetChunkAddress(Idx2, C->LastBrick, Iter, Level, BitPlane);
  PushBack(&ChunkMeta->Addrs, ChunkAddress);
  PushBack(&E->ChunkRDOs, rdo_chunk{ ChunkAddress, Size(E->ChunkStream), 0.0 });
  Rewind(&E->ChunkStream);
}


// TODO: check the error path
error<idx2_err_code>
FlushChunks(const idx2_file& Idx2, encode_data* E)
{
  Reserve(&E->SortedChannels, Size(E->Channels));
  Clear(&E->SortedChannels);
  idx2_ForEach (Ch, E->Channels)
  {
    PushBack(&E->SortedChannels, t2<u32, channel*>{ *Ch.Key, Ch.Val });
  }
  InsertionSort(Begin(E->SortedChannels), End(E->SortedChannels));
  idx2_ForEach (Ch, E->SortedChannels)
  {
    i8 Iter = IterationFromChannelKey(Ch->First);
    i8 Level = LevelFromChannelKey(Ch->First);
    i16 BitPlane = BitPlaneFromChannelKey(Ch->First);
    WriteChunk(Idx2, E, Ch->Second, Iter, Level, BitPlane);
  }

  /* write the chunk meta */
  idx2_ForEach (CmIt, E->ChunkMeta)
  {
    chunk_meta_info* Cm = CmIt.Val;
    file_id FileId = ConstructFilePath(Idx2, *CmIt.Key);
    if (FileId.Id != *CmIt.Key)
    {
      FileId = ConstructFilePath(Idx2, *CmIt.Key);
    }
    printf("%llu %s\n", FileId.Id, FileId.Name.ConstPtr);
    idx2_Assert(FileId.Id == *CmIt.Key);
    /* compress and write chunk sizes */
    idx2_OpenMaybeExistingFile(Fp, FileId.Name.ConstPtr, "ab");
    Flush(&Cm->Sizes);
    WriteBuffer(Fp, ToBuffer(Cm->Sizes));
    ChunkSzsStat.Add((f64)Size(Cm->Sizes));
    WritePOD(Fp, (int)Size(Cm->Sizes));
    /* compress and write chunk addresses */
    CompressBufZstd(ToBuffer(Cm->Addrs), &E->CpresChunkAddrs);
    WriteBuffer(Fp, ToBuffer(E->CpresChunkAddrs));
    // write size of the compressed chunk addresses
    WritePOD(Fp, (int)Size(E->CpresChunkAddrs));
    WritePOD(Fp, (int)Size(Cm->Addrs)); // number of chunks
    std::cout << Size(Cm->Sizes) << " " << Size(E->CpresChunkAddrs) << " " << Size(Cm->Addrs)
              << "\n";
    ChunkAddrsStat.Add((f64)Size(Cm->Addrs) * sizeof(Cm->Addrs[0]));
    CpresChunkAddrsStat.Add((f64)Size(E->CpresChunkAddrs));
  }
  return idx2_Error(idx2_err_code::NoError);
}


void
PrintStats()
{
  printf("num chunks              = %" PRIi64 "\n", ChunkStreamStat.Count());
  printf("brick deltas      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         BrickDeltasStat.Sum(),
         BrickDeltasStat.Avg(),
         BrickDeltasStat.StdDev());
  printf("brick sizes       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         BrickSzsStat.Sum(),
         BrickSzsStat.Avg(),
         BrickSzsStat.StdDev());
  printf("brick stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         BrickStreamStat.Sum(),
         BrickStreamStat.Avg(),
         BrickStreamStat.StdDev());
  //printf("block stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
  //       BlockStat.Sum(),
  //       BlockStat.Avg(),
  //       BlockStat.StdDev());
  printf("chunk sizes       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         ChunkSzsStat.Sum(),
         ChunkSzsStat.Avg(),
         ChunkSzsStat.StdDev());
  printf("chunk addrs       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         ChunkAddrsStat.Sum(),
         ChunkAddrsStat.Avg(),
         ChunkAddrsStat.StdDev());
  printf("cpres chunk addrs total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         CpresChunkAddrsStat.Sum(),
         CpresChunkAddrsStat.Avg(),
         CpresChunkAddrsStat.StdDev());
  printf("chunk stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         ChunkStreamStat.Sum(),
         ChunkStreamStat.Avg(),
         ChunkStreamStat.StdDev());
  printf("brick exps        total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         BrickEMaxesStat.Sum(),
         BrickEMaxesStat.Avg(),
         BrickEMaxesStat.StdDev());
  //printf("block exps        total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
  //       BlockEMaxStat.Sum(),
  //       BlockEMaxStat.Avg(),
  //       BlockEMaxStat.StdDev());
  printf("chunk exp sizes   total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         ChunkEMaxSzsStat.Sum(),
         ChunkEMaxSzsStat.Avg(),
         ChunkEMaxSzsStat.StdDev());
  printf("chunk exps stream total = %12.0f avg = %12.1f stddev = %12.1f bytes\n",
         ChunkEMaxesStat.Sum(),
         ChunkEMaxesStat.Avg(),
         ChunkEMaxesStat.StdDev());
}


} // namespace idx2

