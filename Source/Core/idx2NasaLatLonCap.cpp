#include "idx2Common.h"
#include "idx2Lookup.h"
#include "idx2NasaLatLonCap.h"
#include "Memory.h"
#include "ScopeGuard.h"
#include "Timer.h"
#include "Volume.h"

namespace idx2 {

void Dealloc(nasa_params* P) {
  Dealloc((params*) P);
  Dealloc(&P->FileNames);
  idx2_For(i16, I, 0, Size(P->FileQueue)) {
    if (P->FileQueue[I].Mapped)
      CloseFile(&P->FileQueue[I].File);
  }
  Dealloc(&P->FileIndex);
}

/* Encode one of U12, U3, U45, possibly per time step, and depth (depending on P.Scheme) */
//error<idx2_err_code>
//EncodeNASA(idx2_file* Idx2, const nasa_params& P) {
//  const int BrickBytes = Prod(Idx2->BrickDimsExt3) * sizeof(f64);
//  BrickAlloc_ = free_list_allocator(BrickBytes);
//  idx2_RAII(encode_data, E, Init(&E));
//  idx2_BrickTraverse(
//    timer Timer; StartTimer(&Timer);
//    brick_volume BVol;
//    Resize(&BVol.Vol, Idx2->BrickDimsExt3, dtype::float64, E.Alloc);
//    Fill(idx2_Range(f64, BVol.Vol), 0.0);
//    extent BrickExtent(Top.BrickFrom3 * Idx2->BrickDims3, Idx2->BrickDims3);
//    extent BrickExtentCrop = Crop(BrickExtent, extent(Idx2->Dims3));
//    BVol.ExtentLocal = Relative(BrickExtentCrop, BrickExtent);
////    v2d MinMax = (CopyExtentExtentMinMax<f32, f64>(BrickExtentCrop, Vol, BVol.ExtentLocal, &BVol.Vol));
////    Idx2->ValueRange.Min = Min(Idx2->ValueRange.Min, MinMax.Min);
////    Idx2->ValueRange.Max = Max(Idx2->ValueRange.Max, MinMax.Max);
//    E.Iter = 0;
//    E.Bricks3[E.Iter] = Top.BrickFrom3;
//    E.Brick[E.Iter] = GetLinearBrick(*Idx2, E.Iter, E.Bricks3[E.Iter]);
//    idx2_Assert(E.Brick[E.Iter] == Top.Address);
//    u64 BrickKey = GetBrickKey(E.Iter, E.Brick[E.Iter]);
//    Insert(&E.BrickPool, BrickKey, BVol);
//    EncodeBrick(Idx2, P, &E);
//    TotalTime_ += Seconds(ElapsedTime(&Timer));
//    , 128
//    , Idx2->BrickOrders[E.Iter]
//    , v3i(0)
//    , Idx2->NBricks3s[E.Iter]
//    , extent(Idx2->NBricks3s[E.Iter])
//    , extent(Idx2->NBricks3s[E.Iter])
//  );
//
//  /* dump the bit streams to files */
//  timer Timer; StartTimer(&Timer);
//  idx2_PropagateIfError(FlushChunks(*Idx2, &E));
//  idx2_PropagateIfError(FlushChunkExponents(*Idx2, &E));
//  timer RdoTimer; StartTimer(&RdoTimer);
//  RateDistortionOpt(*Idx2, &E);
//  TotalTime_ += Seconds(ElapsedTime(&Timer));
//  printf("rdo time                = %f\n", Seconds(ElapsedTime(&RdoTimer)));
//
//  WriteMetaFile(*Idx2, P, idx2_PrintScratch("%s/%s/%s.idx2", P.OutDir, P.Meta.Name, P.Meta.Field));
//  printf("num channels            = %" PRIi64 "\n", Size(E.Channels));
//  printf("num sub channels        = %" PRIi64 "\n", Size(E.SubChannels));
//  printf("num chunks              = %" PRIi64 "\n", ChunkStreamStat.Count());
//  printf("brick deltas      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickDeltasStat.Sum(), BrickDeltasStat.Avg(), BrickDeltasStat.StdDev());
//  printf("brick sizes       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickSzsStat.Sum(), BrickSzsStat.Avg(), BrickSzsStat.StdDev());
//  printf("brick stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickStreamStat.Sum(), BrickStreamStat.Avg(), BrickStreamStat.StdDev());
//  printf("block stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BlockStat.Sum(), BlockStat.Avg(), BlockStat.StdDev());
//  printf("chunk sizes       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkSzsStat.Sum(), ChunkSzsStat.Avg(), ChunkSzsStat.StdDev());
//  printf("chunk addrs       total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkAddrsStat.Sum(), ChunkAddrsStat.Avg(), ChunkAddrsStat.StdDev());
//  printf("cpres chunk addrs total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", CpresChunkAddrsStat.Sum(), CpresChunkAddrsStat.Avg(), CpresChunkAddrsStat.StdDev());
//  printf("chunk stream      total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkStreamStat.Sum(), ChunkStreamStat.Avg(), ChunkStreamStat.StdDev());
//  printf("brick exps        total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BrickEMaxesStat.Sum(), BrickEMaxesStat.Avg(), BrickEMaxesStat.StdDev());
//  printf("block exps        total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", BlockEMaxStat.Sum(), BlockEMaxStat.Avg(), BlockEMaxStat.StdDev());
//  printf("chunk exp sizes   total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkEMaxSzsStat.Sum(), ChunkEMaxSzsStat.Avg(), ChunkEMaxSzsStat.StdDev());
//  printf("chunk exps stream total = %12.0f avg = %12.1f stddev = %12.1f bytes\n", ChunkEMaxesStat.Sum(), ChunkEMaxesStat.Avg(), ChunkEMaxesStat.StdDev());
//  printf("total time              = %f seconds\n", TotalTime_);
////  _ASSERTE( _CrtCheckMemory( ) );
//  return idx2_Error(idx2_err_code::NoError);
//}

// NASA_TODO: continue from here
// Copy from the mmap_files to BVol.Vol
//idx2_TT(stype, dtype) static v2d
//CopyNASABrickMinMax(const v3i& BrickFrom3, idx2_file* Idx2, nasa_params& P, brick_volume* BVol) {
//  v2d MinMax = v2d(traits<f64>::Max, traits<f64>::Min);
//  v3i DstFrom3 = From(BVol->ExtentLocal);
//  v3i DstTo3   = To(BVol->ExtentLocal);
//  v3i DstDims3 = Dims(BVol->ExtentLocal);
//  dtype* idx2_Restrict DstPtr = (dtype*)BVol->Vol.Buffer.Data;
//  v3i S3, D3;
//  i32 N = P.N;
//  if (P.Grouping == grouping::Depth) { // group every P.BrickDims3.Z depth levels
//    idx2_RAII(mmap_volume, Vol, (void)Vol, Unmap(&Vol));
//    idx2_ExitIfError(MapVolume(P.FileNames[P.Time].Data, v3i(P.N * P.N, 13, P.D), P.Meta.DType, &Vol, map_mode::Read));
//    if (P.Face == face::Face01) {
//      idx2_Assert((BrickFrom3.X < 2 * N) && (BrickFrom3.Y < 3 * N));
//      idx2_BeginFor3(D3, DstFrom3, DstTo3, v3i(1)) {
//        v3i G3 = BrickFrom3 + D3; // global coordinates
//        i32 X = (G3.X % N) + (G3.Y % N) * N; // linear coordinate inside a single 6 smaller face
//        i32 Y = (G3.X / N) * 3 + (G3.Y / N); // one of the first 6 smaller faces
//        idx2_Assert(Y < 6);
//        i32 Z = G3.Z; // depth
//
//        // TODO: handle the case where the sample is out of the input volume
//        // TODO: handle big endian -> little endian conversion
//      } idx2_EndFor3
//    } else if (P.Face == face::Face2) {
//      idx2_Assert((BrickFrom3.X < N) && (BrickFrom3.Y < N));
//      idx2_BeginFor3(D3, DstFrom3, DstTo3, v3i(1)) {
//        v3i G3 = BrickFrom3 + D3;
//        i32 X = (G3.X % N) + (G3.Y % N) * N; // linear coordinate inside a single 13 smaller face
//        i32 Y = 6;
//        i32 Z = G3.Z; // depth
//      } idx2_EndFor3
//    } else if (P.Face == face::Face34) {
//      idx2_BeginFor3(D3, DstFrom3, DstTo3, v3i(1)) {
//        v3i G3 = BrickFrom3 + D3;
//        i32 X = (G3.X % N ) * N + ((3 * N - G3.Y) % N);
//        i32 Y = (G3.X / N) * 3 + ((3 * N - G3.Y) / N); // one of the last 6 smaller faces
//        idx2_Assert((Y > 6) && (Y < 13));
//        i32 Z = G3.Z; // depth
//      } idx2_EndFor3
//    }
//  } else if (P.Grouping == grouping::Time) {
//    if (P.Face == face::Face01) {
//      idx2_BeginFor3(D3, DstFrom3, DstTo3, v3i(1)) {
//        // first, convert the local brick xyz to global xyz, then
//        // ((x/N) * 3 * N*N + (x%N) + y * N) * sizeof(f32) = index in the depth (k)
//        // then (d * 13*N*N) + k = index in the file, d = the current depth = P.Depth
//        // then z is used to index the time steps (files)
//      } idx2_EndFor3
//    } else if (P.Face == face::Face2) {
//      idx2_BeginFor3(D3, DstFrom3, DstTo3, v3i(1)) {
//        // first, convert the local brick xyz to global xyz, then
//        // (2*N * 3*N) + (y*N + x) * sizeof(f32) = index in the depth (k)
//        // then (d * 13*N*N) + k = index in the file (m), d = current depth = P.Depth
//        // then z is used to index the time steps (files)
//      } idx2_EndFor3
//    } else if (P.Face == face::Face34) {
//      idx2_BeginFor3(D3, DstFrom3, DstTo3, v3i(1)) {
//        // first, convert the local brick xyz to global xyz, then
//        // x = N - y; y = x (rotate 90 degrees cw)
//        // ((x/N) * 3 * N*N + (x%N) + y * N) * sizeof(f32) = index in the depth (k)
//        // then (d * 13*N*N) + k = index in the file (m), d = current depth = P.Depth
//        // then z is used to index the time steps (files)
//      } idx2_EndFor3
//    }
//  }
//  return MinMax;
//}

}