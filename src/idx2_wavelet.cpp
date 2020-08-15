#include "idx2_algorithm.h"
#include "idx2_array.h"
#include "idx2_assert.h"
#include "idx2_bitops.h"
#include "idx2_common.h"
#include "idx2_data_types.h"
#include "idx2_math.h"
#include "idx2_memory.h"
#include "idx2_wavelet.h"
#include "idx2_logger.h"
#include "idx2_scopeguard.h"
#include "idx2_function.h"
//#include "robinhood/robin_hood.h"
#include "idx2_circular_queue.h"
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
//#pragma GCC diagnostic ignored "-W#warnings"
//#pragma GCC diagnostic ignored "-Wpedantic"
//#include "stlab/concurrency/default_executor.hpp"
//#include "stlab/concurrency/immediate_executor.hpp"
//#include "stlab/concurrency/future.hpp"
//#pragma GCC diagnostic pop
//#include <condition_variable>
//#include <mutex>

namespace idx2 {

//static i64 Counter;
//static std::mutex Mutex;
//static std::mutex MemMutex;
//static std::condition_variable Cond;

// NOTE: when called with a different parameter, the old instance will be
// invalidated
static inline free_list_allocator& FreeListAllocator(i64 Bytes) {
  static i64 LastBytes = Bytes;
  static free_list_allocator Instance(Bytes, &Mallocator());
  if (LastBytes != Bytes) {
    LastBytes = Bytes;
    Instance.DeallocAll();
    Instance = free_list_allocator(Bytes, &Mallocator());
  }
  return Instance;
}

/* Compute the norms of the CDF5/3 scaling and wavelet functions

nlevels is the number of times the wavelet transform is done (it is the number of levels minus one).
The first two elements are the scaling norms and wavelet norms, the last two are the lengths of the
scaling and wavelet functions. */
wav_basis_norms
GetCdf53Norms(int NLevels) {
  array<f64> ScalNorms, WaveNorms;
  array<f64> ScalWeights = { 0.5, 1.0, 0.5 };
  array<f64> ScalFunc; Clone(ScalWeights, &ScalFunc);
  array<f64> WaveWeights = { -0.125, -0.25, 0.75, -0.25, -0.125 };
  array<f64> WaveFunc; Clone(WaveWeights, &WaveFunc);

  Resize(&WaveNorms, NLevels + 1);
  Resize(&ScalNorms, NLevels + 1);
  for (int L = 0; L < NLevels + 1; ++L) {
    ScalNorms[L] = Norm(Begin(ScalFunc), End(ScalFunc));
    WaveNorms[L] = Norm(Begin(WaveFunc), End(WaveFunc));
    Upsample(WaveWeights, &WaveWeights);
    array<f64> NewWavFunc;
    Convolve(WaveWeights, ScalFunc, &NewWavFunc);
    Dealloc(&WaveFunc);
    WaveFunc = NewWavFunc;
    Upsample(ScalWeights, &ScalWeights);
    array<f64> NewScalFunc;
    Convolve(ScalWeights, ScalFunc, &NewScalFunc);
    Dealloc(&ScalFunc);
    ScalFunc = NewScalFunc;
  }
  Dealloc(&ScalFunc);
  Dealloc(&WaveFunc);
  Dealloc(&ScalWeights);
  Dealloc(&WaveWeights);
  return { ScalNorms, WaveNorms };
}

void
Dealloc(wav_basis_norms* WbN) {
  Dealloc(&WbN->ScalNorms);
  Dealloc(&WbN->WaveNorms);
}

idx2_I(N) wav_basis_norms_static<N>
GetCdf53NormsFast() {
  wav_basis_norms_static<N> Result;
  f64 Num1 = 3, Num2 = 23;
  for (int I = 0; I < N; ++I) {
    Result.ScalNorms[I] = sqrt(Num1 / (1 << (I + 1)));
    Num1 = Num1 * 4 - 1;
    Result.WaveNorms[I] = sqrt(Num2 / (1 << (I + 5)));
    Num2 = Num2 * 4 - 33;
  }
  return Result;
}

void
ComputeTransformDetails(transform_details* Td, const v3i& Dims3, int NPasses, u64 TformOrder) {
  int Pass = 0;
  u64 PrevOrder = TformOrder;
  v3i D3 = Dims3;
  v3i R3 = D3;
  v3i S3(1);
  grid G(Dims3);
  int StackSize = 0;
  while (Pass < NPasses) {
   idx2_Assert(TformOrder != 0);
   int D = TformOrder & 0x3;
   TformOrder >>= 2;
   if (D == 3) { // next level
     if (TformOrder == 3)  // next one is the last |
       TformOrder = PrevOrder;
     else
       PrevOrder = TformOrder;
     SetStrd(&G, S3);
     SetDims(&G, D3);
     R3 = D3;
     ++Pass;
   } else {
     Td->StackGrids[StackSize] = G;
     Td->StackAxes[StackSize++] = D;
     R3[D] = D3[D] + IsEven(D3[D]);
     SetDims(&G, R3);
     D3[D] = (R3[D] + 1) >> 1;
     S3[D] <<= 1;
   }
  }
  Td->TformOrder = TformOrder;
  Td->StackSize = StackSize;
  Td->BasisNorms = GetCdf53NormsFast<16>();
  Td->NPasses = NPasses;
}

// TODO: this won't work for a general (sub)volume
void
ForwardCdf53Old(volume* Vol, int NLevels) {
#define Body(type)\
  v3i Dims3 = Dims(*Vol);\
  type* FPtr = (type*)(Vol->Buffer.Data);\
  for (int I = 0; I < NLevels; ++I) {\
    FLiftCdf53OldX(FPtr, Dims3, v3i(I));\
    FLiftCdf53OldY(FPtr, Dims3, v3i(I));\
    FLiftCdf53OldZ(FPtr, Dims3, v3i(I));\
  }

  idx2_DispatchOnType(Vol->Type)
#undef Body
}

struct tile_buf {
  i8 NDeps = 0; // number of dependent tiles
  i8 MDeps = 0; // maximum number of dependencies
  volume Vol = {}; // storing tile data
};
// they key is the row major index of the tile
//using tile_map = robin_hood::unordered_map<i64, tile_buf>;
//using tile_map = std::map<i64, tile_buf>;

//#define idx2_Cdf53TileDebug

//void
//ForwardCdf53Tile2D(
//  const v3i& TDims3, // dimensions of a tile (e.g. 32 x 32 x 32)
//  int Lvl, // level of the current tile
//  const v3i& Pos3, // index of the tile in each dimension (within a subband)
//  const array<v3i>& Dims3s, // dimensions of the big array on each level
//  array<array<tile_map>>* Vols // level -> subband -> tiles
//#if defined(idx2_Cdf53TileDebug)
//  , volume* BigVol // we will copy the coefficients here to debug
//  , const array<extent>& BigSbands // subbands of the big volume
//#endif
//)
//{
//  idx2_Assert(IsEven(TDims3.X) && IsEven(TDims3.Y));
//  int NLevels = Size(*Vols) - 1;
//  idx2_Assert(Size(Dims3s) == NLevels + 1);
//  idx2_Assert(Lvl <= NLevels);
//  const int NSbands = 4; // number of subbands in 2D
//  /* transform the current tile */
//  v3i NTiles3 = (Dims3s[Lvl] + TDims3 - 1) / TDims3;
//  idx2_Assert(Pos3 < NTiles3);
//  v3i M(Min(TDims3, v3i(Dims3s[Lvl] - Pos3 * TDims3))); // dims of the current tile
//  volume Vol;
//  {
//    std::unique_lock<std::mutex> Lock(MemMutex);
//    Vol = (*Vols)[Lvl][0][Row(NTiles3, Pos3)].Vol;
//  }
//  idx2_Assert(Vol.Buffer);
//  int LvlNxt = Lvl + 1;
//  if (LvlNxt <= NLevels) {
//    M = M + IsEven(M);
//    if (M.X > 1) {
//      if (Pos3.X + 1 < NTiles3.X) // not last tile in X
//        FLiftCdf53X<f64>(grid(M), M, lift_option::PartialUpdateLast, &Vol);
//      else // last tile in X
//        FLiftCdf53X<f64>(grid(M), M, lift_option::Normal, &Vol);
//    }
//    if (M.Y > 1) {
//      if (Pos3.Y + 1 < NTiles3.Y) // not last tile
//        FLiftCdf53Y<f64>(grid(M), M, lift_option::PartialUpdateLast, &Vol);
//      else // last tile in Y
//        FLiftCdf53Y<f64>(grid(M), M, lift_option::Normal, &Vol);
//    }
//  }
//  /* end the recursion if this is the last level */
//  if (LvlNxt > NLevels) { // last level, no need to add to the next level
//#if defined(idx2_Cdf53TileDebug)
//    v3i CopyM = Min(M, TDims3);
//    if (CopyM > v3i(0))
//      Copy(extent(CopyM), Vol, extent(Pos3 * TDims3, CopyM), BigVol);
//#endif
//    {
//      std::unique_lock<std::mutex> Lock(MemMutex);
//      DeallocBuf(&Vol.Buffer);
//      (*Vols)[Lvl][0].erase(Row(NTiles3, Pos3));
//    }
//    return;
//  }
//  v3i TDims3Ext = TDims3 + v3i(1, 1, 0);
//  stack_linear_allocator<NSbands * sizeof(grid)> Alloc;
//  array<grid> Sbands(&Alloc); BuildSubbands(TDims3Ext, 1, &Sbands);
//  /* spread the samples to the parent subbands */
//  v3i Dims3Next = Dims3s[LvlNxt];
//  v3i NTiles3Nxt = (Dims3Next + TDims3 - 1) / TDims3;
//  for (int Sb = 0; Sb < NSbands; ++Sb) { // through subbands
//    v3i D01 = Pos3 - (Pos3 / 2) * 2; // either 0 or 1 in each dimension
//    v3i D11 = D01 * 2 - 1; // map [0, 1] to [-1, 1]
//    grid SrcG = Sbands[Sb];
//    extent DstG(v3i(0), Dims(SrcG));
//    v2i L = SubbandToLevel(2, Sb).XY;
//    v2i Nb(0); // neighbor
//    for (int Iy = 0; Iy < 2; Nb.Y += D11.Y, ++Iy) {
//      if (Nb.Y == 1 && L.Y == 1)
//        continue;
//      grid SrcGY = SrcG;
//      extent DstGY = DstG;
//      if (Nb.Y != 0) { // contributing to top/bottom parents, take 1 slab only
//        if (Nb.Y == -1) // contributing to the top parent tile, so shift down full
//          DstGY = Translate(DstGY, dimension::Y, TDims3.Y);
//        SrcGY = Slab(SrcGY, dimension::Y, -Nb.Y);
//        DstGY = Slab(DstGY, dimension::Y,  1);
//      } else if (D01.Y == 1) { // second child, shift down half
//        DstGY = Translate(DstGY, dimension::Y, TDims3.Y / 2);
//      }
//      Nb.X = 0;
//      for (int Ix = 0; Ix < 2; Nb.X += D11.X, ++Ix) {
//        v3i Pos3Nxt = Pos3 / 2 + v3i(Nb, 0);
//        if (Nb.X == 1 && L.X == 1)
//          continue;
//        grid SrcGX = SrcGY;
//        extent DstGX = DstGY;
//        if (Nb.X != 0) { // contributing to left/right parents, take 1 slab only
//          if (Nb.X == -1) // contributing to left parent, shift right full
//            DstGX = Translate(DstGX, dimension::X, TDims3.X);
//          SrcGX = Slab(SrcGX, dimension::X, -Nb.X);
//          DstGX = Slab(DstGX, dimension::X,  1);
//        } else if (D01.X == 1) { // second child, shift right half
//          DstGX = Translate(DstGX, dimension::X, TDims3.X / 2);
//        }
//        /* locate the finer tile */
//        if (!(Pos3Nxt >= v3i(0) && Pos3Nxt < NTiles3Nxt))
//          continue; // tile outside the domain
//        tile_buf* TileNxt = nullptr;
//        volume* VolNxt = nullptr;
//        {
//          std::unique_lock<std::mutex> Lock(MemMutex);
//          TileNxt = &(*Vols)[LvlNxt][Sb][Row(NTiles3Nxt, Pos3Nxt)];
//          VolNxt = &TileNxt->Vol;
//          /* add contribution to the finer tile, allocating its memory if needed */
//          if (TileNxt->MDeps == 0) {
//            idx2_Assert(TileNxt->NDeps == 0);
//            idx2_Assert(!VolNxt->Buffer);
//            buffer Buf; CallocBuf(&Buf, sizeof(f64) * Prod(TDims3Ext));
//            *VolNxt = volume(Buf, TDims3Ext, dtype::float64);
//            /* compute the number of dependencies for the finer tile if necessary */
//            v3i MDeps3(4, 4, 1); // by default each tile depends on 16 finer tiles
//            for (int I = 0; I < 2; ++I) {
//              MDeps3[I] -= (Pos3Nxt[I] == 0) || (L[I] == 1);
//              MDeps3[I] -= Pos3Nxt[I] == NTiles3Nxt[I] - 1;
//              MDeps3[I] -= Dims3Next[I] - Pos3Nxt[I] * TDims3[I] <= TDims3[I] / 2;
//            }
//            TileNxt->MDeps = Prod(MDeps3);
//          }
//          // TODO: the following line does not have to be in the same lock
//          Add(SrcGX, Vol, DstGX, VolNxt);
//          ++TileNxt->NDeps;
//        }
//        /* if the finer tile receives from all its dependencies, recurse */
//        if (TileNxt->MDeps == TileNxt->NDeps) {
//          if (Sb == 0) { // recurse
//#if defined(idx2_Cdf53TileDebug)
//            // TODO: spawn a task here
//            ForwardCdf53Tile2D(TDims3, LvlNxt, Pos3Nxt, Dims3s, Vols, BigVol, BigSbands);
//#else
//            ForwardCdf53Tile(TDims3, LvlNxt, Pos3Nxt, Dims3s, Vols);
//#endif
//          } else { // copy data to the big buffer, for testing
//#if defined(idx2_Cdf53TileDebug)
//            int BigSb = (NLevels - LvlNxt) * 3 + Sb;
//            v3i F3 = From(BigSbands[BigSb]);
//            F3 = F3 + Pos3Nxt * TDims3;
//            // NOTE: for subbands other than 0, F3 can be outside of the big volume
//            idx2_Assert(VolNxt->Buffer);
//            v3i CopyDims3 = Min(From(BigSbands[BigSb]) + Dims(BigSbands[BigSb]) - F3, TDims3);
//            if (CopyDims3 > v3i(0)) {
//              idx2_Assert(F3 + CopyDims3 <= Dims(*BigVol));
//              Copy(extent(CopyDims3), *VolNxt, extent(F3, CopyDims3), BigVol);
//            }
//#endif
//            {
//              std::unique_lock<std::mutex> Lock(MemMutex);
//              DeallocBuf(&VolNxt->Buffer);
//              (*Vols)[LvlNxt][Sb].erase(Row(NTiles3Nxt, Pos3Nxt));
//            }
//          }
//        }
//      } // end X loop
//    } // end Y loop
//  } // end subband loop
//  {
//    std::unique_lock<std::mutex> Lock(MemMutex);
//    DeallocBuf(&Vol.Buffer);
//    (*Vols)[Lvl][0].erase(Row(NTiles3, Pos3));
//  }
//}

//void
//ForwardCdf53Tile2D(int NLvls, const v3i& TDims3, const volume& Vol
//#if defined(idx2_Cdf53TileDebug)
//  , volume* OutVol
//#endif
//) {
//  /* calculate the power-of-two dimensions encompassing the volume */
//  v3i M = Dims(Vol);
//  v3i N(v2i::One, 1);
//  while (N.X < M.X || N.Y < M.Y)
//    N = N * 2;
//  N.Z = 1;
//  /* loop through the tiles in Z (morton) order */
//  array<v3i> Dims3s; Init(&Dims3s, NLvls + 1);
//  idx2_CleanUp(0, Dealloc(&Dims3s); )
//  for (int I = 0; I < Size(Dims3s); ++I) {
//    M = M + IsEven(M);
//    Dims3s[I] = M;
//    M = (M + 1) / 2;
//  }
//  array<array<tile_map>> Vols; Init(&Vols, NLvls + 1);
//  idx2_CleanUp(1,
//    for (int I = 0; I < Size(Vols); ++I) {
//      for (int J = 0; J < Size(Vols[I]); ++J)
//        Vols[I][J].~tile_map();
//      Dealloc(&Vols[I]);
//    }
//    Dealloc(&Vols);
//  );
//  for (int I = 0; I < Size(Vols); ++I) {
//    Vols[I] = array<tile_map>();
//    Init(&Vols[I], 4);
//    for (int J = 0; J < Size(Vols[I]); ++J)
//      new (&Vols[I][J]) tile_map;
//  }
//  M = Dims(Vol);
//  v3i NTiles3 = (M + IsEven(M) + TDims3 - 1) / TDims3;
//  v3i NTilesBig3 = (N + TDims3 - 1) / TDims3;
//  v3i TDims3Ext = TDims3 + v3i(1, 1, 0);
//  array<extent> BigSbands; BuildSubbands(M, NLvls, &BigSbands);
//  idx2_CleanUp(2, Dealloc(&BigSbands))
//  for (u32 I = 0; I < Prod<u32>(NTilesBig3); ++I) {
//    // TODO: count the number of tiles processed and break if we are done
//    u32 X = DecodeMorton2X(I), Y = DecodeMorton2Y(I);
//    v3i Pos3(X, Y, 0);
//    if (!(Pos3 * TDims3 < M)) // tile outside the domain
//      continue;
//    i64 Idx = Row(NTiles3, Pos3);
//    buffer Buf; CallocBuf(&Buf, Prod(TDims3Ext) * sizeof(f64));
//    volume& TileVol = Vols[0][0][Idx].Vol;
//    TileVol = volume(Buf, TDims3Ext, dtype::float64);
//    extent E(Pos3 * TDims3, TDims3Ext);
//    v3i From3 = From(E);
//    v3i Dims3 = Min(Dims(E), M - From3);
//    SetDims(&E, Dims3);
//    if (!(From3 < M)) // tile outside domain
//      continue;
//    Copy(E, Vol, extent(v3i(0), Dims(E)), &TileVol);
//    ForwardCdf53Tile2D(TDims3, 0, Pos3, Dims3s, &Vols
//#if defined(idx2_Cdf53TileDebug)
//      , OutVol, BigSbands
//#endif
//    );
//  }
//}

// TODO: replace f64 with a template parameter
//void
//ForwardCdf53Tile(
//  const v3i& TDims3, // dimensions of a tile (e.g. 32 x 32 x 32)
//  int Lvl, // level of the current tile
//  const v3i& Pos3, // index of the tile in each dimension (within a subband)
//  const array<v3i>& Dims3s, // dimensions of the big array on each level
//  array<array<tile_map>>* Vols // level -> subband -> tiles
//#if defined(idx2_Cdf53TileDebug)
//  , volume* BigVol // we will copy the coefficients here to debug
//  , const array<extent>& BigSbands // subbands of the big volume
//#endif
//)
//{
//  idx2_Assert(IsEven(TDims3.X) && IsEven(TDims3.Y) && IsEven(TDims3.Z));
//  int NLevels = Size(*Vols) - 1;
//  idx2_Assert(Size(Dims3s) == NLevels + 1);
//  idx2_Assert(Lvl <= NLevels);
//  const int NSbands = 8; // number of subbands in 3D
//  /* transform the current tile */
//  v3i NTiles3 = (Dims3s[Lvl] + TDims3 - 1) / TDims3;
//  idx2_Assert(Pos3 < NTiles3);
//  v3i M(Min(TDims3, v3i(Dims3s[Lvl] - Pos3 * TDims3))); // dims of the current tile
//  volume Vol;
//  { // TODO: consider using a pin lock here
//    std::unique_lock<std::mutex> Lock(MemMutex);
//    Vol = (*Vols)[Lvl][0][Row(NTiles3, Pos3)].Vol;
//  }
//  idx2_Assert(Vol.Buffer);
//  int LvlNxt = Lvl + 1;
//  if (LvlNxt <= NLevels) {
//    // TODO: since M is always odd, the extension will not happen
//    M = M + IsEven(M);
//    if (M.X > 1) {
//      if (Pos3.X + 1 < NTiles3.X)
//        FLiftCdf53X<f64>(grid(M), M, lift_option::PartialUpdateLast, &Vol);
//      else if (M.X > 1)// last tile in X
//        FLiftCdf53X<f64>(grid(M), M, lift_option::Normal, &Vol);
//    }
//    if (M.Y > 1) {
//      if (Pos3.Y + 1 < NTiles3.Y)
//        FLiftCdf53Y<f64>(grid(M), M, lift_option::PartialUpdateLast, &Vol);
//      else if (M.Y > 1)// last tile in Y
//        FLiftCdf53Y<f64>(grid(M), M, lift_option::Normal, &Vol);
//    }
//    if (M.Z > 1) {
//      if (Pos3.Z + 1 < NTiles3.Z)
//        FLiftCdf53Z<f64>(grid(M), M, lift_option::PartialUpdateLast, &Vol);
//      else if (M.Z > 1)// last tile in Z
//        FLiftCdf53Z<f64>(grid(M), M, lift_option::Normal, &Vol);
//    }
//  }
//  /* end the recursion if this is the last level */
//  if (LvlNxt > NLevels) { // last level, no need to add to the next level
//#if defined(idx2_Cdf53TileDebug)
//    v3i CopyM = Min(M, TDims3);
//    if (CopyM > v3i(0))
//      Copy(extent(CopyM), Vol, extent(Pos3 * TDims3, CopyM), BigVol);
//#endif
//    { // TODO: consider using a spin lock here
//      std::unique_lock<std::mutex> Lock(MemMutex);
//      DeallocBuf(&Vol.Buffer);
//      (*Vols)[Lvl][0].erase(Row(NTiles3, Pos3));
//    }
//    return;
//  }
//  v3i TDims3Ext = TDims3 + v3i(1);
//  stack_linear_allocator<NSbands * sizeof(grid)> Alloc;
//  array<grid> Sbands(&Alloc); BuildSubbands(TDims3Ext, 1, &Sbands);
//  idx2_CleanUp(0, Dealloc(&Sbands));
//  /* spread the samples to the parent subbands */
//  v3i Dims3Next = Dims3s[LvlNxt];
//  v3i NTiles3Nxt = (Dims3Next + TDims3 - 1) / TDims3;
//  for (int Sb = 0; Sb < NSbands; ++Sb) { // through subbands
//    v3i D01 = Pos3 - (Pos3 / 2) * 2; // either 0 or 1 in each dimension
//    v3i D11 = D01 * 2 - 1; // map [0, 1] to [-1, 1]
//    grid SrcG = Sbands[Sb];
//    extent DstG(v3i(0), Dims(SrcG));
//    v3i L = SubbandToLevel(3, Sb);
//    v3i Nb(0); // neighbor
//    for (int Iz = 0; Iz < 2; Nb.Z += D11.Z, ++Iz) { // through (next-level) neighbors
//      if (Nb.Z == 1 && L.Z == 1)
//        continue;
//      grid SrcGZ = SrcG;
//      extent DstGZ = DstG;
//      if (Nb.Z != 0)  {
//        if (Nb.Z == -1)
//          DstGZ = Translate(DstGZ, dimension::Z, TDims3.Z);
//        SrcGZ = Slab(SrcGZ, dimension::Z, -Nb.Z);
//        DstGZ = Slab(DstGZ, dimension::Z,  1);
//      } else if (D01.Z == 1) {
//        DstGZ = Translate(DstGZ, dimension::Z, TDims3.Z / 2);
//      }
//      Nb.Y = 0;
//      for (int Iy = 0; Iy < 2; Nb.Y += D11.Y, ++Iy) {
//        if (Nb.Y == 1 && L.Y == 1)
//          continue;
//        grid SrcGY = SrcGZ;
//        extent DstGY = DstGZ;
//        if (Nb.Y != 0) {
//          if (Nb.Y == -1)
//            DstGY = Translate(DstGY, dimension::Y, TDims3.Y);
//          SrcGY = Slab(SrcGY, dimension::Y, -Nb.Y);
//          DstGY = Slab(DstGY, dimension::Y,  1);
//        } else if (D01.Y == 1) {
//          DstGY = Translate(DstGY, dimension::Y, TDims3.Y / 2);
//        }
//        Nb.X = 0;
//        for (int Ix = 0; Ix < 2; Nb.X += D11.X, ++Ix) {
//          v3i Pos3Nxt = Pos3 / 2 + Nb;
//          if (Nb.X == 1 && L.X == 1)
//            continue;
//          grid SrcGX = SrcGY;
//          extent DstGX = DstGY;
//          if (Nb.X != 0) {
//            if (Nb.X == -1)
//              DstGX = Translate(DstGX, dimension::X, TDims3.X);
//            SrcGX = Slab(SrcGX, dimension::X, -Nb.X);
//            DstGX = Slab(DstGX, dimension::X,  1);
//          } else if (D01.X == 1) {
//            DstGX = Translate(DstGX, dimension::X, TDims3.X / 2);
//          }
//          /* locate the finer tile */
//          if (!(Pos3Nxt >= v3i(0) && Pos3Nxt < NTiles3Nxt))
//            continue; // tile outside the domain
//          tile_buf* TileNxt = nullptr;
//          volume* VolNxt = nullptr;
//          { // TODO: consider using a spin lock here
//            std::unique_lock<std::mutex> Lock(MemMutex);
//            TileNxt = &(*Vols)[LvlNxt][Sb][Row(NTiles3Nxt, Pos3Nxt)];
//            VolNxt = &TileNxt->Vol;
//            /* add contribution to the finer tile, allocating its memory if needed */
//            if (TileNxt->MDeps == 0) {
//              idx2_Assert(TileNxt->NDeps == 0);
//              idx2_Assert(!VolNxt->Buffer);
//              i64 TileSize = sizeof(f64) * Prod(TDims3Ext);
//              buffer Buf; CallocBuf(&Buf, TileSize, &FreeListAllocator(TileSize));
//              *VolNxt = volume(Buf, TDims3Ext, dtype::float64);
//              /* compute the number of dependencies for the finer tile if necessary */
//              v3i MDeps3(4, 4, 4); // by default each tile depends on 64 finer tiles
//              for (int I = 0; I < 3; ++I) {
//                MDeps3[I] -= (Pos3Nxt[I] == 0) || (L[I] == 1);
//                MDeps3[I] -= Pos3Nxt[I] == NTiles3Nxt[I] - 1;
//                MDeps3[I] -= Dims3Next[I] - Pos3Nxt[I] * TDims3[I] <= TDims3[I] / 2;
//              }
//              TileNxt->MDeps = Prod(MDeps3);
//            }
//            // TODO: the following line does not have to be in the same lock
//            // To solve this we could add a lock to the tile_map struct
//            Add(SrcGX, Vol, DstGX, VolNxt);
//            ++TileNxt->NDeps;
//          }
//          /* if the finer tile receives from all its dependencies, recurse */
//          if (TileNxt->MDeps == TileNxt->NDeps) {
//            if (Sb == 0) { // recurse
//#if defined(idx2_Cdf53TileDebug)
//              // TODO: spawn a task here
//              ForwardCdf53Tile(TDims3, LvlNxt, Pos3Nxt, Dims3s, Vols, BigVol, BigSbands);
//#else
//              ForwardCdf53Tile(TDims3, LvlNxt, Pos3Nxt, Dims3s, Vols);
//#endif
//            } else { // copy data to the big buffer, for testing
//#if defined(idx2_Cdf53TileDebug)
//              int BigSb = (NLevels - LvlNxt) * 7 + Sb;
//              v3i F3 = From(BigSbands[BigSb]);
//              F3 = F3 + Pos3Nxt * TDims3;
//              // NOTE: for subbands other than 0, F3 can be outside of the big volume
//              idx2_Assert(VolNxt->Buffer);
//              v3i CopyDims3 = Min(From(BigSbands[BigSb]) + Dims(BigSbands[BigSb]) - F3, TDims3);
//              if (CopyDims3 > v3i(0)) {
//                idx2_Assert(F3 + CopyDims3 <= Dims(*BigVol));
//                Copy(extent(CopyDims3), *VolNxt, extent(F3, CopyDims3), BigVol);
//              }
//#endif
//              { // TODO: consider using a spin lock here
//                std::unique_lock<std::mutex> Lock(MemMutex);
//                DeallocBuf(&VolNxt->Buffer);
//                (*Vols)[LvlNxt][Sb].erase(Row(NTiles3Nxt, Pos3Nxt));
//              }
//            }
//          }
//        } // end X loop
//      } // end Y loop
//    } // end neighbor loop
//  } // end subband loop
//  {
//    std::unique_lock<std::mutex> Lock(MemMutex);
//    DeallocBuf(&Vol.Buffer);
//    (*Vols)[Lvl][0].erase(Row(NTiles3, Pos3));
//  }
//}

// TODO: replace f64 with a generic type
//void
//ForwardCdf53Tile(int NLvls, const v3i& TDims3, const volume& Vol
//#if defined(idx2_Cdf53TileDebug)
//  , volume* OutVol
//#endif
//) {
//  /* calculate the power-of-two dimensions encompassing the volume */
//  v3i M = Dims(Vol);
//  v3i N = v3i(1);
//  while (N.X < M.X || N.Y < M.Y || N.Z < M.Z)
//    N = N * 2;
//  /* loop through the tiles in Z (morton) order */
//  array<v3i> Dims3s; Init(&Dims3s, NLvls + 1);
//  idx2_CleanUp(0, Dealloc(&Dims3s))
//  for (int I = 0; I < Size(Dims3s); ++I) {
//    M = M + IsEven(M);
//    Dims3s[I] = M;
//    M = (M + 1) / 2;
//  }
//  // TODO: release the memory for this array
//  array<array<tile_map>> Vols; Init(&Vols, NLvls + 1);
//  idx2_CleanUp(1,
//    for (int I = 0; I < Size(Vols); ++I) {
//      for (int J = 0; J < Size(Vols[I]); ++J)
//        Vols[I][J].~tile_map();
//      Dealloc(&Vols[I]);
//    }
//    Dealloc(&Vols);
//  );
//  for (int I = 0; I < Size(Vols); ++I) {
//    Vols[I] = array<tile_map>();
//    Init(&Vols[I], 8);
//    for (int J = 0; J < Size(Vols[I]); ++J)
//      new (&Vols[I][J]) tile_map;
//  }
//  M = Dims(Vol);
//  v3i NTiles3 = (M + IsEven(M) + TDims3 - 1) / TDims3;
//  v3i NTilesBig3 = (N + TDims3 - 1) / TDims3;
//  v3i TDims3Ext = TDims3 + v3i(1);
//  array<extent> BigSbands; BuildSubbands(M, NLvls, &BigSbands);
//  idx2_CleanUp(2, Dealloc(&BigSbands);)
//  // TODO: use 64-bit morton code
//  for (u32 I = 0; I < Prod<u32>(NTilesBig3); ++I) {
//    v3i Pos3(DecodeMorton3(I));
//    if (!(Pos3 * TDims3 < M)) // tile outside the domain
//      continue;
//    i64 Idx = Row(NTiles3, Pos3);
//    i64 TileSize = sizeof(f64) * Prod(TDims3Ext);
//    buffer Buf; CallocBuf(&Buf, TileSize);
//    volume& TileVol = Vols[0][0][Idx].Vol;
//    TileVol = volume(Buf, TDims3Ext, dtype::float64);
//    extent E(Pos3 * TDims3, TDims3Ext);
//    v3i From3 = From(E);
//    v3i Dims3 = Min(Dims(E), M - From3);
//    SetDims(&E, Dims3);
//    if (!(From3 < M)) // tile outside domain
//      continue;
//    Copy(E, Vol, extent(v3i(0), Dims(E)), &TileVol);
//    {
//      std::unique_lock<std::mutex> Lock(Mutex);
//      ++Counter;
//    }
//    auto Fut = stlab::async(stlab::default_executor, [&, Pos3, Dims3s, TDims3]() {
//      ForwardCdf53Tile(TDims3, 0, Pos3, Dims3s, &Vols
//#if defined(idx2_Cdf53TileDebug)
//      , OutVol, BigSbands
//#endif
//      );
//      {
//        std::unique_lock<std::mutex> Lock(Mutex);
//        --Counter;
//      }
//      if (Counter == 0)
//        Cond.notify_all();
//    });
//    Fut.detach();
//  }
//  std::unique_lock<std::mutex> Lock(Mutex);
//  Cond.wait(Lock, []{ return Counter == 0; });
//}

void
ExtrapolateCdf53(const v3i& Dims3, u64 TransformOrder, volume* Vol) {
  v3i N3 = Dims(*Vol);
  v3i M3(N3.X == 1 ? 1 : N3.X - 1, N3.Y == 1 ? 1 : N3.Y - 1, N3.Z == 1 ? 1 : N3.Z - 1);
  // printf("M3 = " idx2_PrStrV3i "\n", idx2_PrV3i(M3));
  idx2_Assert(IsPow2(M3.X) && IsPow2(M3.Y) && IsPow2(M3.Z));
  int NLevels = Log2Floor(Max(Max(N3.X, N3.Y), N3.Z));
  ForwardCdf53(Dims3, M3, 0, NLevels, TransformOrder, Vol);
  InverseCdf53(N3, M3, 0, NLevels, TransformOrder, Vol);
}

void
ForwardCdf53(const v3i& M3, const transform_details& Td, volume* Vol) {
  idx2_For(int, I, 0, Td.StackSize) {
    int D = Td.StackAxes[I];
    #define Body(type)\
    switch (D) {\
      case 0: FLiftCdf53X<type>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 1: FLiftCdf53Y<type>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 2: FLiftCdf53Z<type>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      default: idx2_Assert(false); break;\
    };
    idx2_DispatchOnType(Vol->Type);
    #undef Body
  }
}

void
InverseCdf53(const v3i& M3, const transform_details& Td, volume* Vol) {
  int I = Td.StackSize;
  while (I-- > 0) {
    int D = Td.StackAxes[I];
    #define Body(type)\
    switch (D) {\
      case 0: ILiftCdf53X<f64>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 1: ILiftCdf53Y<f64>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 2: ILiftCdf53Z<f64>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      default: idx2_Assert(false); break;\
    };
    idx2_DispatchOnType(Vol->Type);
    #undef Body
  }
}

/* Extrapolate a volume to arbitrary (2^X+1) x (2^Y+1) x (2^Z+1) */
void
ExtrapolateCdf53(const transform_details& Td, volume* Vol) {
  v3i N3 = Dims(*Vol);
  v3i M3(N3.X == 1 ? 1 : N3.X - 1, N3.Y == 1 ? 1 : N3.Y - 1, N3.Z == 1 ? 1 : N3.Z - 1);
  idx2_Assert(IsPow2(M3.X) && IsPow2(M3.Y) && IsPow2(M3.Z));
  ForwardCdf53(M3, Td, Vol);
  InverseCdf53(M3, Td, Vol);
}

/* With extrapolation (if needed). Support custom transform order. */
// TODO: as currently implemented, the behavior of this is different from the BuildSubbands() routine
// when given a transform order such as XXY++ (BuildSubbands will restrict the second X to the coarse
// subband produced by the first X, while this function will not).
void
ForwardCdf53(const v3i& Dims3, const v3i& M3, int Iter, int NLevels, u64 TformOrder, volume* Vol, bool Normalize) {
  idx2_Assert(Dims3 <= Dims(*Vol));
  int Level = 0;
  u64 PrevOrder = TformOrder;
  v3i D3 = Dims3;
  v3i R3 = D3;
  v3i S3(1);
  grid G(Dims3);
  while (Level < NLevels) {
    idx2_Assert(TformOrder != 0);
    int D = TformOrder & 0x3;
    TformOrder >>= 2;
    if (D == 3) { // next level
      if (TformOrder == 3)  // next one is the last |
        TformOrder = PrevOrder;
      else
        PrevOrder = TformOrder;
      SetStrd(&G, S3);
      SetDims(&G, D3);
      R3 = D3;
      ++Level;
    } else {
      #define Body(type)\
      switch (D) {\
        case 0: idx2_Assert(Dims3.X > 1); FLiftCdf53X<type>(G, M3, lift_option::Normal, Vol); break;\
        case 1: idx2_Assert(Dims3.Y > 1); FLiftCdf53Y<type>(G, M3, lift_option::Normal, Vol); break;\
        case 2: idx2_Assert(Dims3.Z > 1); FLiftCdf53Z<type>(G, M3, lift_option::Normal, Vol); break;\
        default: idx2_Assert(false); break;\
      };
      idx2_DispatchOnType(Vol->Type);
      #undef Body
      R3[D] = D3[D] + IsEven(D3[D]);
      SetDims(&G, R3);
      D3[D] = (R3[D] + 1) >> 1;
      S3[D] <<= 1;
    }
  }

  /* Optionally normalize */
  if (NLevels > 0 && Normalize) {
    idx2_Assert(IsFloatingPoint(Vol->Type));
    idx2_RAII(array<subband>, Subbands, BuildSubbands(Dims3, NLevels, TformOrder, &Subbands));
    auto BasisNorms = GetCdf53NormsFast<16>();
    for (int I = 0; I < Size(Subbands); ++I) {
      subband& S = Subbands[I];
      f64 Wx = Dims3.X == 1 ? 1 : (S.LowHigh3.X == 0 ? BasisNorms.ScalNorms[Iter * NLevels + S.Level3Rev.X - 1] : BasisNorms.WaveNorms[Iter * NLevels + S.Level3Rev.X]);
      f64 Wy = Dims3.Y == 1 ? 1 : (S.LowHigh3.Y == 0 ? BasisNorms.ScalNorms[Iter * NLevels + S.Level3Rev.Y - 1] : BasisNorms.WaveNorms[Iter * NLevels + S.Level3Rev.Y]);
      f64 Idx2 = Dims3.Z == 1 ? 1 : (S.LowHigh3.Z == 0 ? BasisNorms.ScalNorms[Iter * NLevels + S.Level3Rev.Z - 1] : BasisNorms.WaveNorms[Iter * NLevels + S.Level3Rev.Z]);
      f64 W = Wx * Wy * Idx2;
      #define Body(type)\
      auto ItEnd = End<type>(S.Grid, *Vol);\
      for (auto It = Begin<type>(S.Grid, *Vol); It != ItEnd; ++It) *It = type(*It * W);
      idx2_DispatchOnType(Vol->Type);
      #undef Body
    }
  }
}

void
ForwardCdf53(const v3i& M3, int Iter, const array<subband>& Subbands, const transform_details& Td, volume* Vol, bool LastIter) {
  idx2_For(int, I, 0, Td.StackSize) {
    int D = Td.StackAxes[I];
    #define Body(type)\
    switch (D) {\
      case 0: FLiftCdf53X<type>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 1: FLiftCdf53Y<type>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 2: FLiftCdf53Z<type>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      default: idx2_Assert(false); break;\
    };
    idx2_DispatchOnType(Vol->Type);
    #undef Body
  }

  /* Optionally normalize */
  idx2_Assert(IsFloatingPoint(Vol->Type));
  for (int I = 0; I < Size(Subbands); ++I) {
    if (I == 0 && !LastIter) continue; // do not normalize subband 0
    subband& S = Subbands[I];
    f64 Wx = M3.X == 1 ? 1 : (S.LowHigh3.X == 0 ? Td.BasisNorms.ScalNorms[Iter * Td.NPasses + S.Level3Rev.X - 1] : Td.BasisNorms.WaveNorms[Iter * Td.NPasses + S.Level3Rev.X]);
    f64 Wy = M3.Y == 1 ? 1 : (S.LowHigh3.Y == 0 ? Td.BasisNorms.ScalNorms[Iter * Td.NPasses + S.Level3Rev.Y - 1] : Td.BasisNorms.WaveNorms[Iter * Td.NPasses + S.Level3Rev.Y]);
    f64 Idx2 = M3.Z == 1 ? 1 : (S.LowHigh3.Z == 0 ? Td.BasisNorms.ScalNorms[Iter * Td.NPasses + S.Level3Rev.Z - 1] : Td.BasisNorms.WaveNorms[Iter * Td.NPasses + S.Level3Rev.Z]);
    f64 W = Wx * Wy * Idx2;
    #define Body(type)\
    auto ItEnd = End<type>(S.Grid, *Vol);\
    for (auto It = Begin<type>(S.Grid, *Vol); It != ItEnd; ++It) *It = type(*It * W);
    idx2_DispatchOnType(Vol->Type);
    #undef Body
  }
}
void
InverseCdf53(const v3i& M3, int Iter, const array<subband>& Subbands, const transform_details& Td, volume* Vol, bool LastIter) {
  /* inverse normalize if required */
  idx2_Assert(IsFloatingPoint(Vol->Type));
  for (int I = 0; I < Size(Subbands); ++I) {
    if (I == 0 && !LastIter) continue; // do not normalize subband 0
    subband& S = Subbands[I];
    f64 Wx = M3.X == 1 ? 1 : (S.LowHigh3.X == 0 ? Td.BasisNorms.ScalNorms[Iter * Td.NPasses + S.Level3Rev.X - 1] : Td.BasisNorms.WaveNorms[Iter * Td.NPasses + S.Level3Rev.X]);
    f64 Wy = M3.Y == 1 ? 1 : (S.LowHigh3.Y == 0 ? Td.BasisNorms.ScalNorms[Iter * Td.NPasses + S.Level3Rev.Y - 1] : Td.BasisNorms.WaveNorms[Iter * Td.NPasses + S.Level3Rev.Y]);
    f64 Idx2 = M3.Z == 1 ? 1 : (S.LowHigh3.Z == 0 ? Td.BasisNorms.ScalNorms[Iter * Td.NPasses + S.Level3Rev.Z - 1] : Td.BasisNorms.WaveNorms[Iter * Td.NPasses + S.Level3Rev.Z]);
    f64 W = 1.0 / (Wx * Wy * Idx2);
    #define Body(type)\
    auto ItEnd = End<type>(S.Grid, *Vol);\
    for (auto It = Begin<type>(S.Grid, *Vol); It != ItEnd; ++It) *It = type(*It * W);
    idx2_DispatchOnType(Vol->Type);
    #undef Body
  }

  /* perform the inverse transform */
  int I = Td.StackSize;
  while (I-- > 0) {
    int D = Td.StackAxes[I];
    #define Body(type)\
    switch (D) {\
      case 0: ILiftCdf53X<f64>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 1: ILiftCdf53Y<f64>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      case 2: ILiftCdf53Z<f64>(Td.StackGrids[I], M3, lift_option::Normal, Vol); break;\
      default: idx2_Assert(false); break;\
    };
    idx2_DispatchOnType(Vol->Type);
    #undef Body
  }
}

void
InverseCdf53(const v3i& Dims3, const v3i& M3, int Iter, int NLevels, u64 TformOrder, volume* Vol, bool Normalize) {
  idx2_Assert(Dims3 <= Dims(*Vol));
  /* "simulate" the forward transform */
  stack_array<grid, 32> StackGrids;
  stack_array<int, 32> StackAxes;
  int Level = 0;
  u64 PrevOrder = TformOrder;
  v3i D3 = Dims3;
  v3i R3 = D3;
  v3i S3(1);
  grid G(Dims3);
  int Iteration = 0;
  while (Level < NLevels) {
   idx2_Assert(TformOrder != 0);
   int D = TformOrder & 0x3;
   TformOrder >>= 2;
   if (D == 3) { // next level
     if (TformOrder == 3)  // next one is the last |
       TformOrder = PrevOrder;
     else
       PrevOrder = TformOrder;
     SetStrd(&G, S3);
     SetDims(&G, D3);
     R3 = D3;
     ++Level;
   } else {
     StackGrids[Iteration] = G;
     StackAxes[Iteration++] = D;
     R3[D] = D3[D] + IsEven(D3[D]);
     SetDims(&G, R3);
     D3[D] = (R3[D] + 1) >> 1;
     S3[D] <<= 1;
   }
  }

  /* inverse normalize if required */
  if (NLevels > 0 && Normalize) {
    idx2_Assert(IsFloatingPoint(Vol->Type));
    idx2_RAII(array<subband>, Subbands, BuildSubbands(Dims3, NLevels, TformOrder, &Subbands));
    auto BasisNorms = GetCdf53NormsFast<16>();
    for (int I = 0; I < Size(Subbands); ++I) {
      subband& S = Subbands[I];
      f64 Wx = Dims3.X == 1 ? 1 : (S.LowHigh3.X == 0 ? BasisNorms.ScalNorms[Iter * NLevels + S.Level3Rev.X - 1] : BasisNorms.WaveNorms[Iter * NLevels + S.Level3Rev.X]);
      f64 Wy = Dims3.Y == 1 ? 1 : (S.LowHigh3.Y == 0 ? BasisNorms.ScalNorms[Iter * NLevels + S.Level3Rev.Y - 1] : BasisNorms.WaveNorms[Iter * NLevels + S.Level3Rev.Y]);
      f64 Idx2 = Dims3.Z == 1 ? 1 : (S.LowHigh3.Z == 0 ? BasisNorms.ScalNorms[Iter * NLevels + S.Level3Rev.Z - 1] : BasisNorms.WaveNorms[Iter * NLevels + S.Level3Rev.Z]);
      f64 W = 1.0 / (Wx * Wy * Idx2);
      #define Body(type)\
      auto ItEnd = End<type>(S.Grid, *Vol);\
      for (auto It = Begin<type>(S.Grid, *Vol); It != ItEnd; ++It) *It = type(*It * W);
      idx2_DispatchOnType(Vol->Type);
      #undef Body
    }
  }

  /* perform the inverse transform */
  while (Iteration-- > 0) {
    int D = StackAxes[Iteration];
    #define Body(type)\
    switch (D) {\
      case 0: ILiftCdf53X<f64>(StackGrids[Iteration], M3, lift_option::Normal, Vol); break;\
      case 1: ILiftCdf53Y<f64>(StackGrids[Iteration], M3, lift_option::Normal, Vol); break;\
      case 2: ILiftCdf53Z<f64>(StackGrids[Iteration], M3, lift_option::Normal, Vol); break;\
      default: idx2_Assert(false); break;\
    };
    idx2_DispatchOnType(Vol->Type);
    #undef Body
  }
}

void
ForwardCdf53(const extent& Ext, int NLevels, volume* Vol) {
  #define Body(type)\
  v3i Dims3 = Dims(Ext), M = Dims(Ext), Strd3 = v3i(1);\
  array<grid> Grids;\
  for (int I = 0; I < NLevels; ++I) {\
    PushBack(&Grids, grid(v3i(0), Dims3, Strd3));\
    Dims3.X += IsEven(Dims3.X); /* extrapolate */\
    PushBack(&Grids, grid(v3i(0), Dims3, Strd3));\
    Dims3.Y += IsEven(Dims3.Y); /* extrapolate */\
    PushBack(&Grids, grid(v3i(0), Dims3, Strd3));\
    Dims3.Z += IsEven(Dims3.Z); /* extrapolate */\
    Strd3 = Strd3 * 2;\
    Dims3 = (Dims3 + 1) / 2;\
  }\
  for (int I = 0, J = 0; I < NLevels; ++I) {\
    FLiftCdf53X<type>(Grids[J++], M, lift_option::Normal, Vol);\
    FLiftCdf53Y<type>(Grids[J++], M, lift_option::Normal, Vol);\
    FLiftCdf53Z<type>(Grids[J++], M, lift_option::Normal, Vol);\
  }
  idx2_DispatchOnType(Vol->Type)
  #undef Body
}

void
InverseCdf53(const extent& Ext, int NLevels, volume* Vol) {
  #define Body(type)\
  v3i Dims3 = Dims(Ext), M = Dims(Ext), Strd3 = v3i(1);\
  array<grid> Grids;\
  for (int I = 0; I < NLevels; ++I) {\
    PushBack(&Grids, grid(v3i(0), Dims3, Strd3));\
    Dims3.X += IsEven(Dims3.X); /* extrapolate */\
    PushBack(&Grids, grid(v3i(0), Dims3, Strd3));\
    Dims3.Y += IsEven(Dims3.Y); /* extrapolate */\
    PushBack(&Grids, grid(v3i(0), Dims3, Strd3));\
    Dims3.Z += IsEven(Dims3.Z); /* extrapolate */\
    Strd3 = Strd3 * 2;\
    Dims3 = (Dims3 + 1) / 2;\
  }\
  for (int I = NLevels - 1, J = (int)Size(Grids) - 1; I >= 0; --I) {\
    ILiftCdf53Z<type>(Grids[J--], M, lift_option::Normal, Vol);\
    ILiftCdf53Y<type>(Grids[J--], M, lift_option::Normal, Vol);\
    ILiftCdf53X<type>(Grids[J--], M, lift_option::Normal, Vol);\
  }

  idx2_DispatchOnType(Vol->Type)
  #undef Body
}

// TODO: this won't work for a general (sub)volume
void
InverseCdf53Old(volume* Vol, int NLevels) {
#define Body(type)\
  v3i Dims3 = Dims(*Vol);\
  type* FPtr = (type*)(Vol->Buffer.Data);\
  for (int I = NLevels - 1; I >= 0; --I) {\
    ILiftCdf53OldZ(FPtr, Dims3, v3i(I));\
    ILiftCdf53OldY(FPtr, Dims3, v3i(I));\
    ILiftCdf53OldX(FPtr, Dims3, v3i(I));\
  }

  idx2_DispatchOnType(Vol->Type)
#undef Body
}

/*
//TODO: this won't work for a general (sub)volume
void
ForwardCdf53Ext(const extent& Ext, volume* Vol) {
#define Body(type)\
  v3i N = Dims(Ext);\
  v3i NN = Dims(*Vol);\
  if (NN.Y > 1) idx2_Assert(NN.X == NN.Y);\
  if (NN.Z > 1) idx2_Assert(NN.Y == NN.Z);\
  idx2_Assert(IsPow2(NN.X - 1));\
  type* FPtr = (type*)(Vol->Buffer.Data);\
  int NLevels = Log2Floor(NN.X - 1) + 1;\
  for (int I = 0; I < NLevels; ++I) {\
    FLiftExtCdf53X(FPtr, N, NN, v3i(I));\
    FLiftExtCdf53Y(FPtr, N, NN, v3i(I));\
    FLiftExtCdf53Z(FPtr, N, NN, v3i(I));\
  }

  idx2_DispatchOnType(Vol->Type)
#undef Body
}
*/

// TODO: test this code
void
AggregateSubbands(const array<grid>& Subbands, array<grid>* AggSubbands) {
  idx2_Assert(Size(Subbands) > 0);
  Clear(AggSubbands);
  PushBack(AggSubbands, Subbands[0]);
  for (int I = 1; I < Size(Subbands); ++I) {
    const grid& AggS = (*AggSubbands)[I - 1];
    const grid& S = Subbands[I];
    v3i AggFrom3 = From(AggS), AggDims3 = Dims(AggS), AggStrd3 = Strd(AggS);
    v3i From3 = From(S), Dims3 = Dims(S);
    v3i NewAggFrom3 = AggFrom3, NewAggStrd3 = AggStrd3, NewAggDims3 = AggDims3;
    for (int D = 0; D < 3; ++D) {
      bool B = (From3[D] - AggFrom3[D]) % AggStrd3[D] != 0;
      NewAggStrd3[D] /= 1 << (int)B;
      NewAggDims3[D] += Dims3[D] * B;
    }
    PushBack(AggSubbands, grid(NewAggFrom3, NewAggDims3, NewAggStrd3));
  }
}

/*
Deposit WavBlock to VolBlock
NDims         = number of dimensions (1, 2, 3)
ValExt        = the extent of the block on finest resolution
Sb            = subband
WavGrid       = grid of the block of wavelet coefficients
WavVol        = the wavelet coefficients
ValGrid       = grid of the values
ValGridInVol  = block of values in the input volume
ValVol        = volume of values */
// TODO: write a function to return the size of the new block with additional wavelet coefficients
// TODO: write a function to copy the existing wavelet coefficients to the new grid
// TODO: complete this function (which should only contain code to do inverse wavelet transform)
//grid WavGrid(SbFrom3 + WavBlockFrom3 * SbStrd3, WavBlockDims3, SbStrd3);
// TODO: what is a good starting value for ValGrid?
wav_grids
ComputeWavGrids(int NDims, int Sb, const extent& ValExt, const grid& WavGrid, const v3i& ValStrd) {
  // TODO: can we figure out the subband through the input grid?
  // TODO: can we figure out the NDims from the WavGrid?
  /* find the support of the value block */
  v3i Lvl3 = SubbandToLevel(NDims, Sb, true);
  v3i WavFrst3 = Frst(WavGrid), WavStrd3 = Strd(WavGrid);
  v3i SbFrst3 = WavFrst3 % WavStrd3;
  v3i S3(Lvl3.X ? 3 * WavStrd3.X / 2 - 1 : WavStrd3.X - 1,
         Lvl3.Y ? 3 * WavStrd3.Y / 2 - 1 : WavStrd3.Y - 1,
         Lvl3.Z ? 3 * WavStrd3.Z / 2 - 1 : WavStrd3.Z - 1);
  v3i ValFrst3 = Frst(ValExt), ValLast3 = Last(ValExt);
  v3i Frst3 = Max(ValFrst3, SbFrst3 + S3), Last3 = Max(ValLast3, SbFrst3 - S3);
  Frst3 = SbFrst3 + ((Frst3 - S3 - SbFrst3 + WavStrd3 - 1) / WavStrd3) * WavStrd3;
  Last3 = SbFrst3 + ((Last3 + S3 - SbFrst3) / WavStrd3) * WavStrd3;
  /* "crop" the WavGrid by First3 and Last3 */
  WavFrst3 = Max(Frst3, WavFrst3);
  v3i WavLast3 = Min(Last3, Last(WavGrid));
  wav_grids Output;
  if (WavLast3 >= WavFrst3) {
    v3i NewStrd3 = WavStrd3 / (1 << Lvl3);
    if (!(ValStrd == 0))
      NewStrd3 = Min(NewStrd3, ValStrd);
    v3i NewFrst3, NewLast3;
    NewFrst3 = ((ValFrst3 + NewStrd3 - 1) / NewStrd3) * NewStrd3;
    NewLast3 = (ValLast3 / NewStrd3) * NewStrd3;
    Output.WavGrid = grid(WavFrst3, Dims(WavFrst3, WavLast3, WavStrd3), WavStrd3);
    Output.ValGrid = grid(NewFrst3, Dims(NewFrst3, NewLast3, NewStrd3), NewStrd3);
    /* for the work grid, first sample has to be even, and the dims have to be odd */
    v3i WrkFrst3 = Max(WavFrst3 - S3, v3i(0));
    WrkFrst3 = WrkFrst3 - IsOdd(WrkFrst3 / NewStrd3) * NewStrd3;
    v3i WrkLast3 = WavLast3 + S3;
    v3i WrkDims3 = Dims(WrkFrst3, WrkLast3, NewStrd3);
    WrkDims3 = WrkDims3 + IsEven(WrkDims3);
    Output.WrkGrid = grid(WrkFrst3, WrkDims3, NewStrd3);
  } else {
    Output.WavGrid = grid::Invalid();
    Output.ValGrid = grid::Invalid();
    Output.WrkGrid = grid::Invalid();
  }
  return Output;
}

// copy the existing values
// TODO: visualize to test this function
//void CopyWavs(
//  const grid& WavGrid, const volume& WavVol, const grid& WrkGrid, volume* WrkVol)
//{
//
//}

/* //TODO: this won't work for a general (sub)volume
void
InverseCdf53Ext(const extent& Ext, volume* Vol) {
#define Body(type)\
  v3i N = Dims(Ext);\
  v3i NN = Dims(*Vol);\
  if (NN.Y > 1) idx2_Assert(NN.X == NN.Y);\
  if (NN.Z > 1) idx2_Assert(NN.Y == NN.Z);\
  idx2_Assert(IsPow2(NN.X - 1));\
  type* FPtr = (type*)(Vol->Buffer.Data);\
  int NLevels = Log2Floor(NN.X - 1) + 1;\
  for (int I = NLevels - 1; I >= 0; --I) {\
    ILiftExtCdf53Z(FPtr, N, NN, v3i(I));\
    ILiftExtCdf53Y(FPtr, N, NN, v3i(I));\
    ILiftExtCdf53X(FPtr, N, NN, v3i(I));\
  }

  idx2_DispatchOnType(Vol->Type)
#undef Body
}
*/

stack_array<u8, 8>
SubbandOrders[4] = {
  { 127, 127, 127, 127, 127, 127, 127, 127 }, // not used
  { 0, 1, 127, 127, 127, 127, 127, 127 }, // for 1D
  { 0, 1, 2, 3, 127, 127, 127, 127 }, // for 2D
  { 0, 1, 2, 4, 3, 5, 6, 7 } // for 3D
};

/* Here we assume the wavelet transform is done in X, then Y, then Z */
void
BuildSubbands(const v3i& N, int NLevels, array<extent>* Subbands) {
  int NDims = NumDims(N);
  stack_array<u8, 8>& Order = SubbandOrders[NDims];
  Clear(Subbands);
  Reserve(Subbands, ((1 << NDims) - 1) * NLevels + 1);
  v3i M = N;
  for (int I = 0; I < NLevels; ++I) {
    v3i P((M.X + 1) >> 1, (M.Y + 1) >> 1, (M.Z + 1) >> 1);
    for (int J = (1 << NDims) - 1; J > 0; --J) {
      u8 Z = BitSet(Order[J], Max(NDims - 3, 0)),
         Y = BitSet(Order[J], Max(NDims - 2, 0)),
         X = BitSet(Order[J], Max(NDims - 1, 0));
      v3i Sm((X == 0) ? P.X : M.X - P.X,
             (Y == 0) ? P.Y : M.Y - P.Y,
             (Z == 0) ? P.Z : M.Z - P.Z);
      if (NDims == 3 && Sm.X != 0 && Sm.Y != 0 && Sm.Z != 0) // child exists
        PushBack(Subbands, extent(v3i(X, Y, Z) * P, Sm));
      else if (NDims == 2 && Sm.X != 0 && Sm.Y != 0)
        PushBack(Subbands, extent(v3i(X * P.X, Y * P.Y, 0), v3i(Sm.X, Sm.Y, 1)));
      else
        PushBack(Subbands, extent(v3i(X * P.X, 0, 0), v3i(Sm.X, 1, 1)));
    }
    M = P;
  }
  PushBack(Subbands, extent(v3i(0), M));
  Reverse(Begin(*Subbands), End(*Subbands));
}

/* This version assumes the coefficients were not moved */
void
BuildSubbands(const v3i& N, int NLevels, array<grid>* Subbands) {
  int NDims = NumDims(N);
  stack_array<u8, 8>& Order = SubbandOrders[NDims];
  Clear(Subbands);
  Reserve(Subbands, ((1 << NDims) - 1) * NLevels + 1);
  v3i M = N; // dimensions of all the subbands at the current level
  v3i S(1, 1, 1); // strides
  for (int I = 0; I < NLevels; ++I) {
    S = S * 2;
    v3i P((M.X + 1) >> 1, (M.Y + 1) >> 1, (M.Z + 1) >> 1); // next dimensions
    for (int J = (1 << NDims) - 1; J > 0; --J) { // for each subband
      u8 Z = BitSet(Order[J], Max(NDims - 3, 0)),
         Y = BitSet(Order[J], Max(NDims - 2, 0)),
         X = BitSet(Order[J], Max(NDims - 1, 0));
      v3i Sm((X == 0) ? P.X : M.X - P.X, // dimensions of the current subband
             (Y == 0) ? P.Y : M.Y - P.Y,
             (Z == 0) ? P.Z : M.Z - P.Z);
      if (NDims == 3 && Sm.X != 0 && Sm.Y != 0 && Sm.Z != 0) // subband exists
        PushBack(Subbands, grid(v3i(X, Y, Z) * S / 2, Sm, S));
      else if (NDims == 2 && Sm.X != 0 && Sm.Y != 0)
        PushBack(Subbands, grid(v3i(X, Y, 0) * S / 2, v3i(Sm.X, Sm.Y, 1), S));
      else if (NDims == 1 && Sm.X != 0)
        PushBack(Subbands, grid(v3i(X, 0, 0) * S / 2, v3i(Sm.X, 1, 1), S));
    }
    M = P;
  }
  PushBack(Subbands, grid(v3i(0), M, S)); // final (coarsest) subband
  Reverse(Begin(*Subbands), End(*Subbands));
}

u64
EncodeTransformOrder(const stref& TransformOrder) {
  u64 Result = 0;
  int Len = Size(TransformOrder);
  for (int I = Len - 1; I >= 0; --I) {
    char C = TransformOrder[I];
    idx2_Assert(C == 'X' || C == 'Y' || C == 'Z' || C == '+');
    int T = C == '+' ? 3 : C - 'X';
    Result = (Result << 2) + T;
  }
  return Result;
}

void
DecodeTransformOrder(u64 Input, str Output) {
  int Len = 0;
  while (Input != 0) {
    int T = Input & 0x3;
    Output[Len++] = T == 3 ? '+' : char('X' + T);
    Input >>= 2;
  }
  Output[Len] = '\0';
}

i8
DecodeTransformOrder(u64 Input, v3i N3, str Output) {
  N3 = v3i((int)NextPow2(N3.X), (int)NextPow2(N3.Y), (int)NextPow2(N3.Z));
  i8 Len = 0;
  u64 SavedInput = Input;
  while (Prod<u64>(N3) > 1) {
    int T = Input & 0x3;
    Input >>= 2;
    if (T == 3) {
      if (Input == 3)
        Input = SavedInput;
      else
        SavedInput = Input;
    } else {
      Output[Len++] = char('X' + T);
      N3[T] >>= 1;
    }
  }
  Output[Len] = '\0';
  return Len;
}

i8
DecodeTransformOrder(u64 Input, int Passes, str Output) {
  i8 Len = 0;
  u64 SavedInput = Input;
  v3i Passes3(Passes);
  while (true) {
    int T = Input & 0x3;
    Input >>= 2;
    if (T == 3) {
      if (Input == 3)
        Input = SavedInput;
      else
        SavedInput = Input;
    } else {
      --Passes3[T];
      if (Passes3[T] < 0)
        break;
      Output[Len++] = char('X' + T);
    }
  }
  Output[Len] = '\0';
  return Len;
}

//void
//DecodeTransformOrder(u64 TransformOrder, str Output) {
//  u64 Inverse = 0;
//  for (int I = Len - 1; I >= 0; --I) {
//    char C = TransformOrder[I];
//    idx2_Assert(C == 'X' || C == 'Y' || C == 'Z' || C == '|');
//    int T = C == '|' ? 3 : C - 'X';
//    Result = (Result << 2) + T;
//  }
//  return Result;
//}

/*
In string form, a TransformOrder is made from 4 characters: X,Y,Z, and +
X, Y, and Z denotes the axis where the transform happens, while + denotes where the next
level begins (any subsequent transform will be done on the coarsest level subband only).
Two consecutive ++ signifies the end of the string. If the end is reached before the number of
levels, the last order gets applied.
In integral form, TransformOrder = T encodes the axis order of the transform in base-4 integers.
T % 4 = D in [0, 3] where 0 = X, 1 = Y, 2 = Z, 3 = +*/
void
BuildSubbands(const v3i& N3, int NLevels, u64 TransformOrder, array<subband>* Subbands) {
  Clear(Subbands);
  Reserve(Subbands, ((1 << NumDims(N3)) - 1) * NLevels + 1);
  circular_queue<subband, 256> Queue;
  PushBack(&Queue, subband{grid(N3), grid(N3), v3<i8>(0), v3<i8>(0), v3<i8>(0), i8(0)});
  i8 Level = 0;
  u64 PrevOrder = TransformOrder;
  v3<i8> MaxLevel3(0);
  stack_array<grid, 32> Grids;
  while (Level < NLevels) {
    idx2_Assert(TransformOrder != 0);
    int D = TransformOrder & 0x3;
    TransformOrder >>= 2;
    if (D == 3) { // next level
      if (TransformOrder == 3)  // next one is the last +
        TransformOrder = PrevOrder;
      else
        PrevOrder = TransformOrder;
      i16 Sz = Size(Queue);
      for (i16 I = Sz - 1; I >= 1; --I) {
        PushBack(Subbands, Queue[I]);
        PopBack(&Queue);
      }
      ++Level;
      Grids[Level] = Queue[0].Grid;
    } else {
      ++MaxLevel3[D];
      i16 Sz = Size(Queue);
      for (i16 I = 0; I < Sz; ++I) {
        const grid& G = Queue[0].Grid;
        grid_split Gs = SplitAlternate(G, dimension(D));
        v3<i8> NextLevel3 = Queue[0].Level3; ++NextLevel3[D];
        v3<i8> NextLowHigh3 = Queue[0].LowHigh3; idx2_Assert(NextLowHigh3[D] == 0); NextLowHigh3[D] = 1;
        PushBack(&Queue, subband{Gs.First, Gs.First, NextLevel3, NextLevel3, Queue[0].LowHigh3, Level});
        PushBack(&Queue, subband{Gs.Second, Gs.Second, Queue[0].Level3, Queue[0].Level3, NextLowHigh3, Level});
        PopFront(&Queue);
      }
    }
  }
  if (Size(Queue) > 0)
    PushBack(Subbands, Queue[0]);
  Reverse(Begin(Grids), Begin(Grids) + Level + 1);
  i64 Sz = Size(*Subbands);
  for (i64 I = 0; I < Sz; ++I) {
    subband& Sband = (*Subbands)[I];
    Sband.Level3 = MaxLevel3 - Sband.Level3Rev;
    Sband.Level = i8(NLevels - Sband.Level - 1);
    Sband.AccumGrid = MergeSubbandGrids(Grids[Sband.Level], Sband.Grid);
  }
  Reverse(Begin(*Subbands), End(*Subbands));
}

// TODO: this function behaves like the ForwardCdf53 function with regards to transform orders such
// as XXY++ (see the comments for that ForwardCdf53).
void
BuildLevelGrids(const v3i& N3, int NLevels, u64 TransformOrder, array<grid>* LevelGrids) {
  Clear(LevelGrids);
  int Level = 0;
  u64 PrevOrder = TransformOrder;
  v3i D3 = N3;
  v3i R3 = D3;
  v3i S3(1);
  grid G(N3);
  PushBack(LevelGrids, G);
  while (Level < NLevels) {
    idx2_Assert(TransformOrder != 0);
    int D = TransformOrder & 0x3;
    TransformOrder >>= 2;
    if (D == 3) { // next level
      if (TransformOrder == 3)  // next one is the last +
        TransformOrder = PrevOrder;
      else
        PrevOrder = TransformOrder;
      SetStrd(&G, S3);
      SetDims(&G, D3);
      R3 = D3;
      PushBack(LevelGrids, G);
      ++Level;
    } else {
      R3[D] = D3[D] + IsEven(D3[D]);
      SetDims(&G, R3);
      D3[D] = (R3[D] + 1) >> 1;
      S3[D] <<= 1;
    }
  }
  idx2_Assert(Size(*LevelGrids) == NLevels + 1);
  Reverse(Begin(*LevelGrids), End(*LevelGrids));
}

grid MergeSubbandGrids(const grid& Sb1, const grid& Sb2) {
  v3i Off3 = Abs(From(Sb2) - From(Sb1));
  v3i Strd3 = Min(Strd(Sb1), Strd(Sb2)) * Equals(Off3, v3i(0)) + Off3;
  v3i Dims3 = Dims(Sb1) + NotEquals(From(Sb1), From(Sb2)) * Dims(Sb2); // TODO: works in case of subbands but not in general
  v3i From3 = Min(From(Sb2), From(Sb1));
  return grid(From3, Dims3, Strd3);
}

// TODO: generalize this to arbitrary transform order
int
LevelToSubband(const v3i& Lvl3) {
  if (Lvl3.X + Lvl3.Y + Lvl3.Z == 0)
    return 0;
  int Lvl = Max(Max(Lvl3.X, Lvl3.Y), Lvl3.Z);
  return 7 * (Lvl - 1) +
    SubbandOrders[3][4 * (Lvl3.X == Lvl) + 2 * (Lvl3.Y == Lvl) + 1 * (Lvl3.Z == Lvl)];
}

//void
//FormSubbands(int NLevels, const volume& SVol, volume* DVol) {
//  idx2_Assert(SVol.Dims == DVol->Dims);
//  idx2_Assert(SVol.Type == DVol->Type);
//  v3i Dims3 = Dims(SVol);
//  array<extent> Subbands;
//  array<grid> SubbandsInPlace;
//  BuildSubbands(Dims3, NLevels, &Subbands);
//  BuildSubbands(Dims3, NLevels, &SubbandsInPlace);
//  for (int I = 0; I < Size(Subbands); ++I)
//    Copy(SubbandsInPlace[I], SVol, Subbands[I], DVol);
//}

// TODO: generalize this to arbitrary transform order
v3i
SubbandToLevel(int NDims, int Sb, bool Norm) {
  if (Sb == 0)
    return v3i(0);
  /* handle level 0 which has only 1 subband (other levels have 7 subbands) */
  int N = (1 << NDims) - 1; // 3 for 2D, 7 for 3D
  int Lvl = (Sb + N - 1) / N;
  /* subtract all subbands on previous levels (except the subband 0);
  basically it reduces the case to the 2x2x2 case where subband 0 is in corner */
  Sb -= N * (Lvl - 1);
  Sb = SubbandOrders[NDims][Sb]; // in 3D, swap 4 and 3
  /* bit 0 -> z axis offset; bit 1 -> y axis offset; bit 2 -> x axis offset
  we subtract from Lvl as it corresponds to the +x, +y, +z corner */
  if (Norm)
    return v3i(BitSet(Sb, NDims - 1), BitSet(Sb, NDims - 2), BitSet(Sb, 0));
  return v3i(Lvl - !BitSet(Sb, NDims - 1), Lvl - !BitSet(Sb, NDims - 2), Lvl - !BitSet(Sb, 0));
}

v3i
ExpandDomain(const v3i& N, int NLevels) {
  v3i Count(0, 0, 0);
  v3i M = N;
  for (int I = 0; I < NLevels; ++I) {
    v3i Add(IsEven(M.X), IsEven(M.Y), IsEven(M.Z));
    Count = Count + Add;
    M = (M + Add + 1) / 2;
  }
  return N + Count;
}

// TODO: generalize this to arbitrary transform order
extent
WavFootprint(int NDims, int Sb, const grid& WavGrid) {
  // TODO: 2D?
  v3i Lvl3 = SubbandToLevel(NDims, Sb, true);
  v3i From3 = From(WavGrid);
  v3i Strd3 = Strd(WavGrid);
  From3.X -= Lvl3.X ? (3 * Strd3.X / 2 - 1) : (Strd3.X - 1);
  From3.Y -= Lvl3.Y ? (3 * Strd3.Y / 2 - 1) : (Strd3.Y - 1);
  From3.Z -= Lvl3.Z ? (3 * Strd3.Z / 2 - 1) : (Strd3.Z - 1);
  v3i Last3 = Last(WavGrid);
  Last3.X += Lvl3.X ? (3 * Strd3.X / 2 - 1) : (Strd3.X - 1);
  Last3.Y += Lvl3.Y ? (3 * Strd3.Y / 2 - 1) : (Strd3.Y - 1);
  Last3.Z += Lvl3.Z ? (3 * Strd3.Z / 2 - 1) : (Strd3.Z - 1);
  return extent(From3, Last3 - From3 + 1);
}

// TODO: generalize this to arbitrary transform order
idx2_T(t) void
Extrapolate3D(v3i D3, volume* Vol) {
  if constexpr (is_same_type<t, f32>::Value)
    idx2_Assert(Vol->Type == dtype::float32);
  if constexpr (is_same_type<t, f64>::Value)
    idx2_Assert(Vol->Type == dtype::float64);
  v3i N3 = Dims(*Vol);
  idx2_Assert(N3 > 1);
  idx2_Assert(D3 <= N3);
  idx2_Assert(N3.X == N3.Y && N3.Y == N3.Z);
  idx2_Assert(IsPow2(N3.X - 1));
  v3i R3 = D3 + IsEven(D3);
  v3i S3(1); // Strides
  int NLevels = LogFloor(2, N3.X - 1);
  stack_array<v3i, 10> Dims3Stack;
  for (int L = 0; L < NLevels; ++L) {
    FLiftCdf53X<t>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), N3 - 1, lift_option::Normal, Vol);
    FLiftCdf53Y<t>(grid(v3i(0), v3i(R3.X, D3.Y, D3.Z), S3), N3 - 1, lift_option::Normal, Vol);
    FLiftCdf53Z<t>(grid(v3i(0), v3i(R3.X, R3.Y, D3.Z), S3), N3 - 1, lift_option::Normal, Vol);
    Dims3Stack[L] = N3 - 1; // TODO: or N3?
    D3 = (R3 + 1) / 2;
    R3 = D3 + IsEven(D3);
    S3 = S3 * 2;
    N3 = (N3 + 1) / 2;
  }
  /* extrapolate by inverse transform */
  for (int I = NLevels - 1; I >= 0; --I) {
    S3 = S3 / 2;
    ILiftCdf53Z<t>(grid(v3i(0), Dims3Stack[I], S3), N3 - 1, lift_option::Normal, Vol);
    ILiftCdf53Y<t>(grid(v3i(0), Dims3Stack[I], S3), N3 - 1, lift_option::Normal, Vol);
    ILiftCdf53X<t>(grid(v3i(0), Dims3Stack[I], S3), N3 - 1, lift_option::Normal, Vol);
  }
}

// TODO: this should just
idx2_T(t) void
Extrapolate2D(v3i D3, volume* Vol) {
  if constexpr (is_same_type<t, f32>::Value)
    idx2_Assert(Vol->Type == dtype::float32);
  if constexpr (is_same_type<t, f64>::Value)
    idx2_Assert(Vol->Type == dtype::float64);
  v3i N3 = Dims(*Vol);
  idx2_Assert(N3.XY  > 1);
  idx2_Assert(D3 <= N3);
  idx2_Assert(N3.X == N3.Y && N3.Z == 1);
  idx2_Assert(IsPow2(N3.X - 1));
  v3i R3 = D3 + IsEven(D3);
  v3i S3(1); // Strides
  int NLevels = LogFloor(2, N3.X - 1);
  stack_array<v3i, 10> Dims3Stack;
  for (int L = 0; L < NLevels; ++L) {
    FLiftCdf53X<t>(grid(v3i(0), v3i(D3.X, D3.Y, 1), S3), N3 - v3i(1, 1, 0), lift_option::Normal, Vol);
    FLiftCdf53Y<t>(grid(v3i(0), v3i(R3.X, D3.Y, 1), S3), N3 - v3i(1, 1, 0), lift_option::Normal, Vol);
    Dims3Stack[L] = N3 - v3i(1, 1, 0); // TODO: or N3?
    D3 = (R3 + 1) / 2;
    R3 = D3 + IsEven(D3);
    S3 = v3i(S3.XY * 2, 1);
    N3 = (N3 + 1) / 2;
  }
  /* extrapolate by inverse transform */
  for (int I = NLevels - 1; I >= 0; --I) {
    S3 = S3 / 2; S3.Z = 1;
    ILiftCdf53Y<t>(grid(v3i(0), Dims3Stack[I], S3), N3 - 1, lift_option::Normal, Vol);
    ILiftCdf53X<t>(grid(v3i(0), Dims3Stack[I], S3), N3 - 1, lift_option::Normal, Vol);
  }
}

void
Extrapolate(v3i D3, volume* Vol) {
  v3i N3 = Dims(*Vol);
  if (NumDims(N3) == 2) {
#define Body(type)\
    Extrapolate2D<type>(D3, Vol);
    idx2_DispatchOnType(Vol->Type)
#undef Body
  } else if (NumDims(D3) == 3) {
#define Body(type)\
    Extrapolate3D<type>(D3, Vol);
    idx2_DispatchOnType(Vol->Type)
#undef Body
  } else {
    idx2_Assert(false);
  }
}

} // namespace idx2
