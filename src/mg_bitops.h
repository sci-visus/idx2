#pragma once

#include "mg_macros.h"
#include "mg_common.h"
#if defined(__clang__) || defined(__GNUC__)
#include <x86intrin.h>
#endif

namespace mg {

/* Set the I(th) least significant bit of val to 1. Index starts at 0. */
mg_T(t) t    SetBit  (t Val, int I);
mg_T(t) t    UnsetBit(t Val, int I);
mg_T(t) bool BitSet  (t Val, int I);
mg_T(t) t    FlipBit (t Val, int I);

mg_T(t) t TakeFirstBits(t Val, int NBits);

/*
Return the bit plane of the most significant one-bit. Counting starts from the
least significant bit plane.
Examples: Msb(0) = -1, Msb(2) = 1, Msb(5) = 2, Msb(8) = 3 */
i8 Msb(u32 V, i8 Default = -1);
i8 Msb(u64 V, i8 Default = -1);
i8 Lsb(u32 V, i8 Default = -1);
i8 Lsb(u64 V, i8 Default = -1);
/* Count the number of leading zero bits */
i8 LzCnt(u32 V, i8 Default = -1);
i8 LzCnt(u64 V, i8 Default = -1);
i8 TzCnt(u32 V, i8 Default = -1);
i8 TzCnt(u64 V, i8 Default = -1);

/* Morton encoding/decoding */
v3<u32> DecodeMorton3(u32 Code);
u32 EncodeMorton3(const v3<u32>& Val);
v3<u32> DecodeMorton3(u64 Code);
u64 EncodeMorton3(const v3<u64>& Val);
u32 DecodeMorton2X(u32 Code);
u32 DecodeMorton2Y(u32 Code);

/* Stuff three 21-bit uints into one 64-bit uint */
u64 Pack3i64  (const v3i& V);
v3i Unpack3i64(u64 V);
/* Return the low 32 bits of the input */
u32 LowBits64 (u64 V);
u32 HighBits64(u64 V);

} // namespace mg

#include "mg_assert.h"
#include "mg_macros.h"
#include "mg_common.h"
// #include "bitmap_tables.h"

namespace mg {

mg_Ti(t) t
SetBit(t Val, int I) {
  mg_Assert(I < (int)mg_BitSizeOf(t));
  return Val | t((1ull << I));
}

mg_Ti(t) t
UnsetBit(t Val, int I) {
  mg_Assert(I < (int)mg_BitSizeOf(t));
  return Val & t(~(1ull << I));
}

mg_Ti(t) bool
BitSet(t Val, int I) {
  mg_Assert(I < (int)mg_BitSizeOf(t));
  return 1 & (Val >> I);
}

mg_Ti(t) t
FlipBit(t Val, int I) {
  mg_Assert(I < (int)mg_BitSizeOf(t));
  return Val ^ t(1ull << I);
}

mg_Ti(t) t
TakeFirstBits(t Val, int NBits) {
  int S = mg_BitSizeOf(t);
  mg_Assert(NBits <= S && NBits > 0);
  int Shift = S - NBits ;
  t Mask = t(-1) << Shift;
  using u = typename traits<t>::unsigned_t;
  t Result = u(Val & Mask) >> Shift;
  return Result;
}

mg_Ti(t) t
TakeFirstBitsNoShift(t Val, int NBits) {
  int S = mg_BitSizeOf(t);
  mg_Assert(NBits <= S && NBits > 0);
  t Mask = t(-1) << (S - NBits);
  using u = typename traits<t>::unsigned_t;
  t Result = u(Val & Mask);
  return Result;
}

// TODO: check the return value of these intrinsics
#if defined(__clang__) || defined(__GNUC__)
#include <x86intrin.h>
mg_Inline i8
Msb(u32 V, i8 Default) {
  return (V == 0) ? Default : i8(sizeof(u32) * 8 - 1 - __builtin_clz(V));
}
mg_Inline i8
Msb(u64 V, i8 Default) {
  return (V == 0) ? Default : i8(sizeof(u64) * 8 - 1 - __builtin_clzll(V));
}
mg_Inline i8
Lsb(u32 V, i8 Default) {
  return (V == 0) ? Default : i8(__builtin_ctz(V));
}
mg_Inline i8
Lsb(u64 V, i8 Default) {
  return (V == 0) ? Default : i8(__builtin_ctzll(V));
}
#elif defined(_MSC_VER)
//#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanReverse64)
mg_Inline i8
Msb(u32 V, i8 Default) {
  unsigned long Index = 0;
  unsigned char Ret = _BitScanReverse(&Index, V);
  return Ret ? (i8)Index : Default;
}
mg_Inline i8
Msb(u64 V, i8 Default) {
  unsigned long Index = 0;
  unsigned char Ret = _BitScanReverse64(&Index, V);
  return Ret ? (i8)Index : Default;
}
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanForward64)
mg_Inline i8
Lsb(u32 V, i8 Default) {
  unsigned long Index = 0;
  unsigned char Ret = _BitScanForward(&Index, V);
  return Ret ? (i8)Index : Default;
}
mg_Inline i8
Lsb(u64 V, i8 Default) {
  unsigned long Index = 0;
  unsigned char Ret = _BitScanForward64(&Index, V);
  return Ret ? (i8)Index : Default;
}
#endif

// TODO: the following clashes with stlab which brings in MSVC's intrin.h
//#if defined(__BMI2__)
//#if defined(__clang__) || defined(__GNUC__)
//#include <intrin.h>
//#include <mmintrin.h>
//#include <x86intrin.h>
//mg_Inline i8
//LzCnt(u32 V, i8 Default) { return V ? (i8)_lzcnt_u32(V) : Default; }
//mg_Inline i8
//LzCnt(u64 V, i8 Default) { return V ? (i8)_lzcnt_u64(V) : Default; }
//mg_Inline i8
//TzCnt(u32 V, i8 Default) { return V ? (i8)_tzcnt_u32(V) : Default; }
//mg_Inline i8
//TzCnt(u64 V, i8 Default) { return V ? (i8)_tzcnt_u64(V) : Default; }
//#elif defined(_MSC_VER)
//#include <intrin.h>
//mg_Inline i8
//LzCnt(u32 V, i8 Default) { return V ? (i8)__lzcnt(V) : Default; }
//mg_Inline i8
//LzCnt(u64 V, i8 Default) { return V ? (i8)__lzcnt64(V) : Default; }
//mg_Inline i8
//TzCnt(u32 V, i8 Default) { return V ? (i8)__tzcnt(V) : Default; }
//mg_Inline i8
//TzCnt(u64 V, i8 Default) { return V ? (i8)__tzcnt64(V) : Default; }
//#endif
//#endif

/* Reverse the operation that inserts two 0 bits after every bit of x */
mg_Inline u32
CompactBy2(u32 X) {
  X &= 0x09249249;                  // X = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
  X = (X ^ (X >>  2)) & 0x030c30c3; // X = ---- --98 ---- 76-- --54 ---- 32-- --10
  X = (X ^ (X >>  4)) & 0x0300f00f; // X = ---- --98 ---- ---- 7654 ---- ---- 3210
  X = (X ^ (X >>  8)) & 0x030000ff; // X = ---- --98 ---- ---- ---- ---- 7654 3210
  X = (X ^ (X >> 16)) & 0x000003ff; // X = ---- ---- ---- ---- ---- --98 7654 3210
  return X;
}

mg_Inline u32
CompactBy1(u32 X) {
  X &= 0x55555555;                 // X = -5-4 -3-2 -1-0 -9-8 -7-6 -5-4 -3-2 -1-0
  X = (X ^ (X >> 1)) & 0x33333333; // X = --54 --32 --10 --98 --76 --54 --32 --10
  X = (X ^ (X >> 2)) & 0x0f0f0f0f; // X = ---- 5432 ---- 1098 ---- 7654 ---- 3210
  X = (X ^ (X >> 4)) & 0x00ff00ff; // X = ---- ---- 5432 1098 ---- ---- 7654 3210
  X = (X ^ (X >> 8)) & 0x0000ffff; // X = ---- ---- ---- ---- 5432 1098 7654 3210
  return X;
}

/* Morton decoding */
mg_Inline v3<u32>
DecodeMorton3(u32 Code) {
  return v3<u32>(CompactBy2(Code >> 0), CompactBy2(Code >> 1), CompactBy2(Code >> 2));
}

mg_Inline u32
DecodeMorton2X(u32 Code) { return CompactBy1(Code >> 0); }
mg_Inline u32
DecodeMorton2Y(u32 Code) { return CompactBy1(Code >> 1); }

mg_Inline u32
SplitBy2(u32 X) {
  X &=                  0x000003ff; // X = ---- ---- ---- ---- ---- --98 7654 3210
  X = (X ^ (X << 16)) & 0x030000ff; // X = ---- --98 ---- ---- ---- ---- 7654 3210
  X = (X ^ (X <<  8)) & 0x0300f00f; // X = ---- --98 ---- ---- 7654 ---- ---- 3210
  X = (X ^ (X <<  4)) & 0x030c30c3; // X = ---- --98 ---- 76-- --54 ---- 32-- --10
  X = (X ^ (X <<  2)) & 0x09249249; // X = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
  return X;
}

mg_Inline u32
EncodeMorton3(const v3<u32>& Val) {
  return SplitBy2(Val.X) | (SplitBy2(Val.Y) << 1) | (SplitBy2(Val.Z) << 2);
}

mg_Inline u64
SplitBy2(u64 X)  {
  X &=                0x00000000001fffffULL;
  X = (X | X << 32) & 0x001f00000000ffffULL;
  X = (X | X << 16) & 0x001f0000ff0000ffULL;
  X = (X | X <<  8) & 0x100f00f00f00f00fULL;
  X = (X | X <<  4) & 0x10c30c30c30c30c3ULL;
  X = (X | X <<  2) & 0x1249249249249249ULL;
  return X;
}

mg_Inline u64
EncodeMorton3(const v3<u64>& Val) {
  return SplitBy2(Val.X) | (SplitBy2(Val.Y) << 1) | (SplitBy2(Val.Z) << 2);
}

mg_Inline u32
CompactBy2(u64 X) {
  X &=                  0x1249249249249249ULL;
  X = (X ^ (X >>  2)) & 0x10c30c30c30c30c3ULL;
  X = (X ^ (X >>  4)) & 0x100f00f00f00f00fULL;
  X = (X ^ (X >>  8)) & 0x001f0000ff0000ffULL;
  X = (X ^ (X >> 16)) & 0x001f00000000ffffULL;
  X = (X ^ (X >> 32)) & 0x00000000001fffffULL;
  return (u32)X;
}

mg_Inline v3<u32>
DecodeMorton3(u64 Code) {
  return v3<u32>(CompactBy2(Code >> 0), CompactBy2(Code >> 1), CompactBy2(Code >> 2));
}

mg_Inline u32
Pack3i32(const v3i& V) { return u32(V.X & 0x3FF) + (u32(V.Y & 0x3FF) << 10) + (u32(V.Z & 0x3FF) << 20); }
mg_Inline v3i
Unpack3i32(u32 V) {
  return v3i((i32(V & 0x3FF) << 22) >> 22, (i32(V & 0xFFC00) << 12) >> 22, (i32(V & 0x3FFFFC00) << 2) >> 22);
}

mg_Inline u64
Pack3i64(const v3i& V) { return u64(V.X & 0x1FFFFF) + (u64(V.Y & 0x1FFFFF) << 21) + (u64(V.Z & 0x1FFFFF) << 42); }
mg_Inline v3i
Unpack3i64(u64 V) {
  return v3i((i64(V & 0x1FFFFF) << 43) >> 43, (i64(V & 0x3FFFFE00000) << 22) >> 43, (i64(V & 0x7FFFFC0000000000ull) << 1) >> 43);
}

mg_Inline u32
LowBits64(u64 V) { return V & 0xFFFFFFFF; }
mg_Inline u32
HighBits64(u64 V) { return V >> 32; }

// inline int
// DecodeBitmap(u64 Val, int* Out) {
//   int* OutBackup = Out;
//   __m256i BaseVec = _mm256_set1_epi32(-1);
//   __m256i IncVec = _mm256_set1_epi32(64);
//   __m256i Add8 = _mm256_set1_epi32(8);

//   if (Val == 0) {
//     BaseVec = _mm256_add_epi32(BaseVec, IncVec);
//   } else {
//     for (int K = 0; K < 4; ++K) {
//       uint8_t ByteA = (uint8_t)Val;
//       uint8_t ByteB = (uint8_t)(Val >> 8);
//       Val >>= 16;
//       __m256i VecA = _mm256_cvtepu8_epi32(_mm_cvtsi64_si128(*(uint64_t*)(vecDecodeTableByte[ByteA])));
//       __m256i VecB = _mm256_cvtepu8_epi32(_mm_cvtsi64_si128(*(uint64_t*)(vecDecodeTableByte[ByteB])));
//       uint8_t AdvanceA = lengthTable[ByteA];
//       uint8_t AdvanceB = lengthTable[ByteB];
//       VecA = _mm256_add_epi32(BaseVec, VecA);
//       BaseVec = _mm256_add_epi32(BaseVec, Add8);
//       VecB = _mm256_add_epi32(BaseVec, VecB);
//       BaseVec = _mm256_add_epi32(BaseVec, Add8);
//       _mm256_storeu_si256((__m256i*)Out, VecA);
//       Out += AdvanceA;
//       _mm256_storeu_si256((__m256i*)Out, VecB);
//       Out += AdvanceB;
//     }
//   }
//   return Out - OutBackup;
// }
//
} // namespace mg

