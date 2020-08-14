#pragma once

#include "idx2_common.h"
#include "idx2_array.h"
#include "idx2_volume.h"

namespace idx2 {

struct wavelet_block {
  v3i Levels;
  grid Grid;
  volume Volume;
  bool IsPacked = false; // if the Volume stores data just for this block
};

struct wav_basis_norms {
  array<f64> ScalNorms; // scaling function norms
  array<f64> WaveNorms; // wavelet function norms
};
wav_basis_norms GetCdf53Norms(int NLevels);
void Dealloc(wav_basis_norms* WbN);

idx2_I(N) struct wav_basis_norms_static {
  stack_array<f64, N> ScalNorms;
  stack_array<f64, N> WaveNorms;
};
idx2_I(N) wav_basis_norms_static<N> GetCdf53NormsFast();

struct subband {
  grid Grid;
  grid AccumGrid; // accumulative grid (with the coarsest same-level subband)
  v3<i8> Level3; // convention: 0 is the coarsest
  v3<i8> Level3Rev; // convention: 0 is the finest
  v3<i8> LowHigh3;
  i8 Level = 0;
};

struct transform_details {
  wav_basis_norms_static<16> BasisNorms;
  stack_array<grid, 32> StackGrids;
  stack_array<int, 32> StackAxes;
  u64 TformOrder;
  int StackSize;
  int NPasses;
};
void ComputeTransformDetails(transform_details* Td, const v3i& Dims3, int NLevels, u64 TformOrder);

/* Normal lifting which uses mirroring at the boundary */
idx2_T(t) void FLiftCdf53OldX(t* F, const v3i& N, const v3i& L);
idx2_T(t) void FLiftCdf53OldY(t* F, const v3i& N, const v3i& L);
idx2_T(t) void FLiftCdf53OldZ(t* F, const v3i& N, const v3i& L);
idx2_T(t) void ILiftCdf53OldX(t* F, const v3i& N, const v3i& L);
idx2_T(t) void ILiftCdf53OldY(t* F, const v3i& N, const v3i& L);
idx2_T(t) void ILiftCdf53OldZ(t* F, const v3i& N, const v3i& L);
/* Do constant extrapolation (the last value is extrapolated if the signal is odd-length) */
idx2_T(t) void FLiftCdf53ConstX(t* F, const v3i& N, const v3i& L);
idx2_T(t) void FLiftCdf53ConstY(t* F, const v3i& N, const v3i& L);
idx2_T(t) void FLiftCdf53ConstZ(t* F, const v3i& N, const v3i& L);
idx2_T(t) void ILiftCdf53ConstX(t* F, const v3i& N, const v3i& L);
idx2_T(t) void ILiftCdf53ConstY(t* F, const v3i& N, const v3i& L);
idx2_T(t) void ILiftCdf53ConstZ(t* F, const v3i& N, const v3i& L);

/* New set of lifting functions. We assume the volume where we want to transform
to happen (M) is contained within a bigger volume (Vol). When Dims(Grid) is even,
extrapolation will happen, in a way that the last (odd) wavelet coefficient is 0.
We assume the storage at index M3.(x/y/z) is available to store the extrapolated
values if necessary. We could always store the extrapolated value at the correct
position, but storing it at M3.(x/y/z) allows us to avoid having to actually use
extra storage (which are mostly used to store 0 wavelet coefficients). */
enum lift_option { Normal, PartialUpdateLast, NoUpdateLast, NoUpdate };
idx2_T(t) void FLiftCdf53X(const grid& Grid, const v3i& M3, lift_option Opt, volume* Vol);
idx2_T(t) void FLiftCdf53Y(const grid& Grid, const v3i& M3, lift_option Opt, volume* Vol);
idx2_T(t) void FLiftCdf53Z(const grid& Grid, const v3i& M3, lift_option Opt, volume* Vol);
/* The inverse lifting functions can be used to extrapolate the volume */
idx2_T(t) void ILiftCdf53X(const grid& Grid, const v3i& M3, lift_option Opt, volume* Vol);
idx2_T(t) void ILiftCdf53Y(const grid& Grid, const v3i& M3, lift_option Opt, volume* Vol);
idx2_T(t) void ILiftCdf53Z(const grid& Grid, const v3i& M3, lift_option Opt, volume* Vol);

/* "In-place" extrapolate a volume to size 2^L+1, which is assumed to be the
dims of Vol. The original volume is stored in the D3 sub-volume of Vol. */
void Extrapolate(v3i D3, volume* Vol);

/* Lifting with extrapolation */
// idx2_T(t) void FLiftExtCdf53X(t* F, const v3i& N, const v3i& NBig, const v3i& L);
// idx2_T(t) void FLiftExtCdf53Y(t* F, const v3i& N, const v3i& NBig, const v3i& L);
// idx2_T(t) void FLiftExtCdf53Z(t* F, const v3i& N, const v3i& NBig, const v3i& L);
// idx2_T(t) void ILiftExtCdf53X(t* F, const v3i& N, const v3i& NBig, const v3i& L);
// idx2_T(t) void ILiftExtCdf53Y(t* F, const v3i& N, const v3i& NBig, const v3i& L);
// idx2_T(t) void ILiftExtCdf53Z(t* F, const v3i& N, const v3i& NBig, const v3i& L);

//#define idx2_Cdf53TileDebug

//void ForwardCdf53Tile(int NLvls, const v3i& TDims3, const volume& Vol
//#if defined(idx2_Cdf53TileDebug)
//  , volume* OutVol
//#endif
//);
void ForwardCdf53(const extent& Ext, int NLevels, volume* Vol);
void InverseCdf53(const extent& Ext, int NLevels, volume* Vol);
void ExtrapolateCdf53(const v3i& Dims3, u64 TransformOrder, volume* Vol);
void ExtrapolateCdf53(const transform_details& Td, volume* Vol);
void ForwardCdf53(const v3i& Dims3, const v3i& M3, int Iter, int NLevels, u64 TformOrder, volume* Vol, bool Normalize = false);
void InverseCdf53(const v3i& Dims3, const v3i& M3, int Iter, int NLevels, u64 TformOrder, volume* Vol, bool Normalize = false);
void ForwardCdf53(const v3i& M3, int Iter, const array<subband>& Subbands, const transform_details& Td, volume* Vol, bool Normalize = false);
void InverseCdf53(const v3i& M3, int Iter, const array<subband>& Subbands, const transform_details& Td, volume* Vol, bool Normalize = false);
void ForwardCdf53Old(volume* Vol, int NLevels);
void InverseCdf53Old(volume* Vol, int NLevels);
void ForwardCdf53Ext(const extent& Ext, volume* Vol);
void InverseCdf53Ext(const extent& Ext, volume* Vol);

idx2_T(t) struct array;
void BuildSubbands(const v3i& N3, int NLevels, array<extent>* Subbands);
void BuildSubbands(const v3i& N3, int NLevels, array<grid>* Subbands);
u64 EncodeTransformOrder(const stref& TransformOrder);
void DecodeTransformOrder(u64 Input, str Output);
i8 DecodeTransformOrder(u64 Input, v3i N3, str Output);
i8 DecodeTransformOrder(u64 Input, int Passes, str Output);
void BuildSubbands(const v3i& N3, int NLevels, u64 TransformOrder, array<subband>* Subbands);
void BuildLevelGrids(const v3i& N3, int NLevels, u64 TransformOrder, array<grid>* LevelGrids);
grid MergeSubbandGrids(const grid& Sb1, const grid& Sb2);

/* Copy samples from Src so that in Dst, samples are organized into subbands */
void FormSubbands(int NLevels, const grid& SrcGrid, const volume& SrcVol,
                  const grid& DstGrid, volume* DstVol);
/* Assume the wavelet transform is done in X, then Y, then Z */
int LevelToSubband(const v3i& Lvl3);
v3i ExpandDomain(const v3i& N, int NLevels);

/* If Norm(alize), the return levels are either 0 or 1 */
v3i SubbandToLevel(int NDims, int Sb, bool Norm = false);

struct wav_grids {
  grid WavGrid; // grid of wavelet coefficients to copy
  grid ValGrid; // the output grid of values
  grid WrkGrid; // determined using the WavGrid
};
wav_grids ComputeWavGrids(int NDims, int Sb, const extent& ValExt, const grid& WavGrid, const v3i& ValStrd);

/* Return the footprint (influence range) of a block of wavelet coefficients */
extent WavFootprint(int NDims, int Sb, const grid& WavGrid);

} // namespace idx2

#include "idx2_algorithm.h"
#include "idx2_assert.h"
#include "idx2_bitops.h"
#include "idx2_math.h"
#include "idx2_memory.h"
#include "idx2_volume.h"
//#include <stlab/concurrency/future.hpp>

#define idx2_RowX(x, y, z, N) i64(z) * N.X * N.Y + i64(y) * N.X + (x)
#define idx2_RowY(y, x, z, N) i64(z) * N.X * N.Y + i64(y) * N.X + (x)
#define idx2_RowZ(z, x, y, N) i64(z) * N.X * N.Y + i64(y) * N.X + (x)

/* Forward lifting */
#define idx2_FLiftCdf53(z, y, x)\
idx2_T(t) void \
FLiftCdf53##x(const grid& Grid, const v3i& M, lift_option Opt, volume* Vol) {\
  v3i P = From(Grid), D = Dims(Grid), S = Strd(Grid), N = Dims(*Vol);\
  if (D.x == 1) return;\
  idx2_Assert(M.x <= N.x);\
  idx2_Assert(IsPow2(S.X) && IsPow2(S.Y) && IsPow2(S.Z));\
  idx2_Assert(D.x >= 2);\
  idx2_Assert(IsEven(P.x));\
  idx2_Assert(P.x + S.x * (D.x - 2) < M.x);\
  buffer_t<t> F(Vol->Buffer);\
  int x0 = Min(P.x + S.x * D.x, M.x); /* extrapolated position */\
  int x1 = Min(P.x + S.x * (D.x - 1), M.x); /* last position */\
  int x2 = P.x + S.x * (D.x - 2); /* second last position */\
  int x3 = P.x + S.x * (D.x - 3); /* third last position */\
  bool Ext = IsEven(D.x);\
  for (int z = P.z; z < P.z + S.z * D.z; z += S.z) {\
    int zz = Min(z, M.z);\
    for (int y = P.y; y < P.y + S.y * D.y; y += S.y) {\
      int yy = Min(y, M.y);\
      if (Ext) {\
        idx2_Assert(M.x < N.x);\
        t A = F[idx2_Row##x(x2, yy, zz, N)]; /* 2nd last (even) */\
        t B = F[idx2_Row##x(x1, yy, zz, N)]; /* last (odd) */\
        /* store the extrapolated value at the boundary position */\
        F[idx2_Row##x(x0, yy, zz, N)] = 2 * B - A;\
      }\
      /* predict (excluding last odd position) */\
      for (int x = P.x + S.x; x < P.x + S.x * (D.x - 2); x += 2 * S.x) {\
        t & Val = F[idx2_Row##x(x, yy, zz, N)];\
        Val -= (F[idx2_Row##x(x - S.x, yy, zz, N)] + F[idx2_Row##x(x + S.x, yy, zz, N)]) / 2;\
      }\
      if (!Ext) { /* no extrapolation, predict at the last odd position */\
        t & Val = F[idx2_Row##x(x2, yy, zz, N)];\
        Val -= (F[idx2_Row##x(x1, yy, zz, N)] + F[idx2_Row##x(x3, yy, zz, N)]) / 2;\
      } else if (x1 < M.x) {\
        F[idx2_Row##x(x1, yy, zz, N)] = 0;\
      }\
      /* update (excluding last odd position) */\
      if (Opt != lift_option::NoUpdate) {\
        for (int x = P.x + S.x; x < P.x + S.x * (D.x - 2); x += 2 * S.x) {\
          t Val = F[idx2_Row##x(x, yy, zz, N)];\
          F[idx2_Row##x(x - S.x, yy, zz, N)] += Val / 4;\
          F[idx2_Row##x(x + S.x, yy, zz, N)] += Val / 4;\
        }\
        if (!Ext) { /* no extrapolation, update at the last odd position */\
          t Val = F[idx2_Row##x(x2, yy, zz, N)];\
          F[idx2_Row##x(x3, yy, zz, N)] += Val / 4;\
          if (Opt == lift_option::Normal)\
            F[idx2_Row##x(x1, yy, zz, N)] += Val / 4;\
          else if (Opt == lift_option::PartialUpdateLast)\
            F[idx2_Row##x(x1, yy, zz, N)] = Val / 4;\
        }\
      }\
    }\
  }\
}

// TODO: this function does not make use of PartialUpdateLast
#define idx2_ILiftCdf53(z, y, x)\
idx2_T(t) void \
ILiftCdf53##x(const grid& Grid, const v3i& M, lift_option Opt, volume* Vol) {\
  v3i P = From(Grid), D = Dims(Grid), S = Strd(Grid), N = Dims(*Vol);\
  if (D.x == 1) return;\
  idx2_Assert(M.x <= N.x);\
  idx2_Assert(IsPow2(S.X) && IsPow2(S.Y) && IsPow2(S.Z));\
  idx2_Assert(D.x >= 2);\
  idx2_Assert(IsEven(P.x));\
  idx2_Assert(P.x + S.x * (D.x - 2) < M.x);\
  buffer_t<t> F(Vol->Buffer);\
  int x0 = Min(P.x + S.x * D.x, M.x); /* extrapolated position */\
  int x1 = Min(P.x + S.x * (D.x - 1), M.x); /* last position */\
  int x2 = P.x + S.x * (D.x - 2); /* second last position */\
  int x3 = P.x + S.x * (D.x - 3); /* third last position */\
  bool Ext = IsEven(D.x);\
  for (int z = P.z; z < P.z + S.z * D.z; z += S.z) {\
    int zz = Min(z, M.z);\
    for (int y = P.y; y < P.y + S.y * D.y; y += S.y) {\
      int yy = Min(y, M.y);\
      /* inverse update (excluding last odd position) */\
      if (Opt != lift_option::NoUpdate) {\
        for (int x = P.x + S.x; x < P.x + S.x * (D.x - 2); x += 2 * S.x) {\
          t Val = F[idx2_Row##x(x, yy, zz, N)];\
          F[idx2_Row##x(x - S.x, yy, zz, N)] -= Val / 4;\
          F[idx2_Row##x(x + S.x, yy, zz, N)] -= Val / 4;\
        }\
        if (!Ext) { /* no extrapolation, inverse update at the last odd position */\
          t Val = F[idx2_Row##x(x2, yy, zz, N)];\
          F[idx2_Row##x(x3, yy, zz, N)] -= Val / 4;\
          if (Opt == lift_option::Normal)\
            F[idx2_Row##x(x1, yy, zz, N)] -= Val / 4;\
        } else { /* extrapolation, need to "fix" the last position (odd) */\
          t A = F[idx2_Row##x(x0, yy, zz, N)];\
          t B = F[idx2_Row##x(x2, yy, zz, N)];\
          F[idx2_Row##x(x1, yy, zz, N)] = (A + B) / 2;\
        }\
      }\
      /* inverse predict (excluding last odd position) */\
      for (int x = P.x + S.x; x < P.x + S.x * (D.x - 2); x += 2 * S.x) {\
        t & Val = F[idx2_Row##x(x, yy, zz, N)];\
        Val += (F[idx2_Row##x(x - S.x, yy, zz, N)] + F[idx2_Row##x(x + S.x, yy, zz, N)])  / 2;\
      }\
      if (!Ext) { /* no extrapolation, inverse predict at the last odd position */\
        t & Val = F[idx2_Row##x(x2, yy, zz, N)];\
        Val += (F[idx2_Row##x(x1, yy, zz, N)] + F[idx2_Row##x(x3, yy, zz, N)]) / 2;\
      }\
    }\
  }\
}

/* Forward x lifting */
// TODO: merge the first two loops
#define idx2_FLiftCdf53Old(z, y, x)\
idx2_T(t) void \
FLiftCdf53Old##x(t* F, const v3i& N, const v3i& L) {\
  v3i P(1 << L.X, 1 << L.Y, 1 << L.Z);\
  v3i M = (N + P - 1) / P;\
  if (M.x <= 1)\
    return;\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x - 1;\
    t & Val = F[idx2_Row##x(x, y, z, N)];\
    Val -= F[idx2_Row##x(XLeft, y, z, N)] / 2;\
    Val -= F[idx2_Row##x(XRight, y, z, N)] / 2;\
  }}}\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x - 1;\
    t Val = F[idx2_Row##x(x, y, z, N)];\
    F[idx2_Row##x(XLeft, y, z, N)] += Val / 4;\
    F[idx2_Row##x(XRight, y, z, N)] += Val / 4;\
  }}}\
  idx2_MallocArray(Temp, t, M.x / 2);\
  int S##x = (M.x + 1) / 2;\
  for (int z = 0; z < M.z; ++z) {\
  for (int y = 0; y < M.y; ++y) {\
    for (int x = 1; x < M.x; x += 2) {\
      Temp[x / 2] = F[idx2_Row##x(x    , y, z, N)];\
      F[idx2_Row##x(x / 2, y, z, N)] = F[idx2_Row##x(x - 1, y, z, N)];\
    }\
    if (IsOdd(M.x))\
      F[idx2_Row##x(M.x / 2, y, z, N)] = F[idx2_Row##x(M.x - 1, y, z, N)];\
    for (int x = 0; x < (M.x / 2); ++x)\
      F[idx2_Row##x(S##x + x, y, z, N)] = Temp[x];\
  }}\
}

// TODO: merge two loops
#define idx2_ILiftCdf53Old(z, y, x)\
idx2_T(t) void \
ILiftCdf53Old##x(t* F, const v3i& N, const v3i& L) {\
  v3i P(1 << L.X, 1 << L.Y, 1 << L.Z);\
  v3i M = (N + P - 1) / P;\
  if (M.x <= 1)\
    return;\
  idx2_MallocArray(Temp, t, M.x / 2);\
  int S##x = (M.x + 1) >> 1;\
  for (int z = 0; z < M.z; ++z) {\
  for (int y = 0; y < M.y; ++y) {\
    for (int x = 0; x < (M.x / 2); ++x)\
      Temp[x] = F[idx2_Row##x(S##x + x, y, z, N)];\
    if (IsOdd(M.x))\
      F[idx2_Row##x(M.x - 1, y, z, N)] = F[idx2_Row##x(M.x >> 1, y, z, N)];\
    for (int x = (M.x / 2) * 2 - 1; x >= 1; x -= 2) {\
      F[idx2_Row##x(x - 1, y, z, N)] = F[idx2_Row##x(x >> 1, y, z, N)];\
      F[idx2_Row##x(x    , y, z, N)] = Temp[x / 2];\
    }\
  }}\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x - 1;\
    t Val = F[idx2_Row##x(x, y, z, N)];\
    F[idx2_Row##x(XLeft, y, z, N)] -= Val / 4;\
    F[idx2_Row##x(XRight, y, z, N)] -= Val / 4;\
  }}}\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x - 1;\
    t & Val = F[idx2_Row##x(x, y, z, N)];\
    Val += F[idx2_Row##x(XLeft, y, z, N)] / 2;\
    Val += F[idx2_Row##x(XRight, y, z, N)] / 2;\
  }}}\
}

#define idx2_FLiftCdf53Const(z, y, x)\
idx2_T(t) void \
FLiftCdf53Const##x(t* F, const v3i& N, const v3i& L) {\
  v3i P(1 << L.X, 1 << L.Y, 1 << L.Z);\
  v3i M = (N + P - 1) / P;\
  if (M.x <= 1)\
    return;\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x;\
    t & Val = F[idx2_Row##x(x, y, z, N)];\
    Val -= (F[idx2_Row##x(XLeft , y, z, N)] + F[idx2_Row##x(XRight , y, z, N)]) / 2;\
  }}}\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x - 1;\
    t Val = F[idx2_Row##x(x, y, z, N)];\
    F[idx2_Row##x(XLeft, y, z, N)] += Val / 4;\
    F[idx2_Row##x(XRight, y, z, N)] += Val / 4;\
  }}}\
  idx2_MallocArray(Temp, t, M.x / 2);\
  int S##x = (M.x + 1) / 2;\
  for (int z = 0; z < M.z; ++z) {\
  for (int y = 0; y < M.y; ++y) {\
    for (int x = 1; x < M.x; x += 2) {\
      Temp[x / 2] = F[idx2_Row##x(x    , y, z, N)];\
      F[idx2_Row##x(x / 2, y, z, N)] = F[idx2_Row##x(x - 1, y, z, N)];\
    }\
    if (IsOdd(M.x))\
      F[idx2_Row##x(M.x / 2, y, z, N)] = F[idx2_Row##x(M.x - 1, y, z, N)];\
    for (int x = 0; x < (M.x / 2); ++x)\
      F[idx2_Row##x(S##x + x, y, z, N)] = Temp[x];\
  }}\
}

// TODO: merge two loops
#define idx2_ILiftCdf53Const(z, y, x)\
idx2_T(t) void \
ILiftCdf53Const##x(t* F, const v3i& N, const v3i& L) {\
  v3i P(1 << L.X, 1 << L.Y, 1 << L.Z);\
  v3i M = (N + P - 1) / P;\
  if (M.x <= 1)\
    return;\
  idx2_MallocArray(Temp, t, M.x / 2);\
  int S##x = (M.x + 1) >> 1;\
  for (int z = 0; z < M.z; ++z) {\
  for (int y = 0; y < M.y; ++y) {\
    for (int x = 0; x < (M.x / 2); ++x)\
      Temp[x] = F[idx2_Row##x(S##x + x, y, z, N)];\
    if (IsOdd(M.x))\
      F[idx2_Row##x(M.x - 1, y, z, N)] = F[idx2_Row##x(M.x >> 1, y, z, N)];\
    for (int x = (M.x / 2) * 2 - 1; x >= 1; x -= 2) {\
      F[idx2_Row##x(x - 1, y, z, N)] = F[idx2_Row##x(x >> 1, y, z, N)];\
      F[idx2_Row##x(x    , y, z, N)] = Temp[x / 2];\
    }\
  }}\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x - 1;\
    t Val = F[idx2_Row##x(x, y, z, N)];\
    F[idx2_Row##x(XLeft , y, z, N)] -= Val / 4;\
    F[idx2_Row##x(XRight, y, z, N)] -= Val / 4;\
  }}}\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < M.x; x += 2) {\
    int XLeft = x - 1;\
    int XRight = x < M.x - 1 ? x + 1 : x;\
    t & Val = F[idx2_Row##x(x, y, z, N)];\
    if (x < M.x - 1) {\
      Val += F[idx2_Row##x(XLeft , y, z, N)] / 2;\
      Val += F[idx2_Row##x(XRight, y, z, N)] / 2;\
    } else {\
      Val += F[idx2_Row##x(XLeft , y, z, N)] + F[idx2_Row##x(XRight, y, z, N)];\
    }\
  }}}\
}

#define idx2_FLiftExtCdf53(z, y, x)\
idx2_T(t) void \
FLiftExtCdf53##x(t* F, const v3i& N, const v3i& NBig, const v3i& L) {\
  idx2_Assert(L.X == L.Y && L.Y == L.Z);\
  auto D = DimsAtLevel(N, L.x);\
  /* linearly extrapolate */\
  if (D[0].x < D[1].x) {\
    idx2_Assert(D[0].x + 1 == D[1].x);\
    /*_Pragma("omp parallel for")*/\
    for (int z = 0; z < D[1].z; ++z) {\
    for (int y = 0; y < D[1].y; ++y) {\
      t A = F[idx2_Row##x(D[0].x - 2, y, z, NBig)];\
      t B = F[idx2_Row##x(D[0].x - 1, y, z, NBig)];\
      F[idx2_Row##x(D[0].x, y, z, NBig)] = 2 * B - A;\
    }}\
  }\
  v3i P(1 << L.X, 1 << L.Y, 1 << L.Z);\
  v3i M = (NBig + P - 1) / P;\
  if (M.x <= 1)\
    return;\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < D[1].x; x += 2) {\
    t & Val = F[idx2_Row##x(x, y, z, NBig)];\
    Val -= F[idx2_Row##x(x - 1, y, z, NBig)] / 2;\
    Val -= F[idx2_Row##x(x + 1, y, z, NBig)] / 2;\
  }}}\
  /*_Pragma("omp parallel for collapse(2)")*/\
  for (int z = 0; z < M.z; ++z   ) {\
  for (int y = 0; y < M.y; ++y   ) {\
  for (int x = 1; x < D[1].x; x += 2) {\
    t Val = F[idx2_Row##x(x, y, z, NBig)];\
    F[idx2_Row##x(x - 1, y, z, NBig)] += Val / 4;\
    F[idx2_Row##x(x + 1, y, z, NBig)] += Val / 4;\
  }}}\
  idx2_MallocArray(Temp, t, M.x / 2);\
  int S##x = (M.x + 1) / 2;\
  for (int z = 0; z < M.z; ++z) {\
  for (int y = 0; y < M.y; ++y) {\
    for (int x = 1; x < M.x; x += 2) {\
      Temp[x / 2] = F[idx2_Row##x(x, y, z, NBig)];\
      F[idx2_Row##x(x / 2, y, z, NBig)] = F[idx2_Row##x(x - 1, y, z, NBig)];\
    }\
    if (IsOdd(M.x))\
      F[idx2_Row##x(M.x / 2, y, z, NBig)] = F[idx2_Row##x(M.x - 1, y, z, NBig)];\
    for (int x = 0; x < (M.x / 2); ++x)\
      F[idx2_Row##x(S##x + x, y, z, NBig)] = Temp[x];\
  }}\
}

#define idx2_ILiftExtCdf53(z, y, x)\
idx2_T(t) void \
ILiftExtCdf53##x(t* F, const v3i& N, const v3i& NBig, const v3i& L) {\
  (void)N;\
  idx2_Assert(L.X == L.Y && L.Y == L.Z);\
  return ILiftCdf53Old##x(F, NBig, L);\
}

namespace idx2 {

inline stack_array<v3i, 2>
DimsAtLevel(v3i N, int L) {
  for (int I = 0; I < L; ++I) {
    N = ((N / 2) * 2) + 1;
    N = (N + 1) / 2;
  }
  return stack_array<v3i,2>{N, (N / 2) * 2 + 1};
}

idx2_FLiftCdf53(Z, Y, X) // X forward lifting
idx2_FLiftCdf53(Z, X, Y) // Y forward lifting
idx2_FLiftCdf53(Y, X, Z) // Z forward lifting
idx2_ILiftCdf53(Z, Y, X) // X inverse lifting
idx2_ILiftCdf53(Z, X, Y) // Y inverse lifting
idx2_ILiftCdf53(Y, X, Z) // Z inverse lifting

idx2_FLiftCdf53Old(Z, Y, X) // X forward lifting
idx2_FLiftCdf53Old(Z, X, Y) // Y forward lifting
idx2_FLiftCdf53Old(Y, X, Z) // Z forward lifting
idx2_ILiftCdf53Old(Z, Y, X) // X inverse lifting
idx2_ILiftCdf53Old(Z, X, Y) // Y inverse lifting
idx2_ILiftCdf53Old(Y, X, Z) // Z inverse lifting

idx2_FLiftCdf53Const(Z, Y, X) // X forward lifting
idx2_FLiftCdf53Const(Z, X, Y) // Y forward lifting
idx2_FLiftCdf53Const(Y, X, Z) // Z forward lifting
idx2_ILiftCdf53Const(Z, Y, X) // X inverse lifting
idx2_ILiftCdf53Const(Z, X, Y) // Y inverse lifting
idx2_ILiftCdf53Const(Y, X, Z) // Z inverse lifting

// idx2_FLiftExtCdf53(Z, Y, X) // X forward lifting
// idx2_FLiftExtCdf53(Z, X, Y) // Y forward lifting
// idx2_FLiftExtCdf53(Y, X, Z) // Z forward lifting
// idx2_ILiftExtCdf53(Z, Y, X) // X forward lifting
// idx2_ILiftExtCdf53(Z, X, Y) // Y forward lifting
// idx2_ILiftExtCdf53(Y, X, Z) // Z forward lifting

} // namespace idx2

#undef idx2_FLiftCdf53Old
#undef idx2_ILiftCdf53Old
#undef idx2_FLiftCdf53
#undef idx2_ILiftCdf53
#undef idx2_FLiftExtCdf53
#undef idx2_ILiftExtCdf53
#undef idx2_RowX
#undef idx2_RowY
#undef idx2_RowZ

