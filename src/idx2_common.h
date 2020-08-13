#pragma once

//#include <inttypes.h>
#include <float.h>
#include <stdint.h>
#include "idx2_macros.h"
//#include <crtdbg.h>

#if !defined(__cplusplus) || defined(__STDC_FORMAT_MACROS) // [   See footnote 185 at page 198

// The fprintf macros for signed integers are:
#define PRId8       "d"
#define PRIi8       "i"
#define PRIdLEAST8  "d"
#define PRIiLEAST8  "i"
#define PRIdFAST8   "d"
#define PRIiFAST8   "i"

#define PRId16       "hd"
#define PRIi16       "hi"
#define PRIdLEAST16  "hd"
#define PRIiLEAST16  "hi"
#define PRIdFAST16   "hd"
#define PRIiFAST16   "hi"

#define PRId32       "I32d"
#define PRIi32       "I32i"
#define PRIdLEAST32  "I32d"
#define PRIiLEAST32  "I32i"
#define PRIdFAST32   "I32d"
#define PRIiFAST32   "I32i"

#define PRId64       "I64d"
#define PRIi64       "I64i"
#define PRIdLEAST64  "I64d"
#define PRIiLEAST64  "I64i"
#define PRIdFAST64   "I64d"
#define PRIiFAST64   "I64i"

#define PRIdMAX     "I64d"
#define PRIiMAX     "I64i"

#define PRIdPTR     "Id"
#define PRIiPTR     "Ii"

// The fprintf macros for unsigned integers are:
#define PRIo8       "o"
#define PRIu8       "u"
#define PRIx8       "x"
#define PRIX8       "X"
#define PRIoLEAST8  "o"
#define PRIuLEAST8  "u"
#define PRIxLEAST8  "x"
#define PRIXLEAST8  "X"
#define PRIoFAST8   "o"
#define PRIuFAST8   "u"
#define PRIxFAST8   "x"
#define PRIXFAST8   "X"

#define PRIo16       "ho"
#define PRIu16       "hu"
#define PRIx16       "hx"
#define PRIX16       "hX"
#define PRIoLEAST16  "ho"
#define PRIuLEAST16  "hu"
#define PRIxLEAST16  "hx"
#define PRIXLEAST16  "hX"
#define PRIoFAST16   "ho"
#define PRIuFAST16   "hu"
#define PRIxFAST16   "hx"
#define PRIXFAST16   "hX"

#define PRIo32       "I32o"
#define PRIu32       "I32u"
#define PRIx32       "I32x"
#define PRIX32       "I32X"
#define PRIoLEAST32  "I32o"
#define PRIuLEAST32  "I32u"
#define PRIxLEAST32  "I32x"
#define PRIXLEAST32  "I32X"
#define PRIoFAST32   "I32o"
#define PRIuFAST32   "I32u"
#define PRIxFAST32   "I32x"
#define PRIXFAST32   "I32X"

#define PRIo64       "I64o"
#define PRIu64       "I64u"
#define PRIx64       "I64x"
#define PRIX64       "I64X"
#define PRIoLEAST64  "I64o"
#define PRIuLEAST64  "I64u"
#define PRIxLEAST64  "I64x"
#define PRIXLEAST64  "I64X"
#define PRIoFAST64   "I64o"
#define PRIuFAST64   "I64u"
#define PRIxFAST64   "I64x"
#define PRIXFAST64   "I64X"

#define PRIoMAX     "I64o"
#define PRIuMAX     "I64u"
#define PRIxMAX     "I64x"
#define PRIXMAX     "I64X"

#define PRIoPTR     "Io"
#define PRIuPTR     "Iu"
#define PRIxPTR     "Ix"
#define PRIXPTR     "IX"

// The fscanf macros for signed integers are:
#define SCNd8       "d"
#define SCNi8       "i"
#define SCNdLEAST8  "d"
#define SCNiLEAST8  "i"
#define SCNdFAST8   "d"
#define SCNiFAST8   "i"

#define SCNd16       "hd"
#define SCNi16       "hi"
#define SCNdLEAST16  "hd"
#define SCNiLEAST16  "hi"
#define SCNdFAST16   "hd"
#define SCNiFAST16   "hi"

#define SCNd32       "ld"
#define SCNi32       "li"
#define SCNdLEAST32  "ld"
#define SCNiLEAST32  "li"
#define SCNdFAST32   "ld"
#define SCNiFAST32   "li"

#define SCNd64       "I64d"
#define SCNi64       "I64i"
#define SCNdLEAST64  "I64d"
#define SCNiLEAST64  "I64i"
#define SCNdFAST64   "I64d"
#define SCNiFAST64   "I64i"

#define SCNdMAX     "I64d"
#define SCNiMAX     "I64i"

#ifdef _WIN64 // [
#  define SCNdPTR     "I64d"
#  define SCNiPTR     "I64i"
#else  // _WIN64 ][
#  define SCNdPTR     "ld"
#  define SCNiPTR     "li"
#endif  // _WIN64 ]

// The fscanf macros for unsigned integers are:
#define SCNo8       "o"
#define SCNu8       "u"
#define SCNx8       "x"
#define SCNX8       "X"
#define SCNoLEAST8  "o"
#define SCNuLEAST8  "u"
#define SCNxLEAST8  "x"
#define SCNXLEAST8  "X"
#define SCNoFAST8   "o"
#define SCNuFAST8   "u"
#define SCNxFAST8   "x"
#define SCNXFAST8   "X"

#define SCNo16       "ho"
#define SCNu16       "hu"
#define SCNx16       "hx"
#define SCNX16       "hX"
#define SCNoLEAST16  "ho"
#define SCNuLEAST16  "hu"
#define SCNxLEAST16  "hx"
#define SCNXLEAST16  "hX"
#define SCNoFAST16   "ho"
#define SCNuFAST16   "hu"
#define SCNxFAST16   "hx"
#define SCNXFAST16   "hX"

#define SCNo32       "lo"
#define SCNu32       "lu"
#define SCNx32       "lx"
#define SCNX32       "lX"
#define SCNoLEAST32  "lo"
#define SCNuLEAST32  "lu"
#define SCNxLEAST32  "lx"
#define SCNXLEAST32  "lX"
#define SCNoFAST32   "lo"
#define SCNuFAST32   "lu"
#define SCNxFAST32   "lx"
#define SCNXFAST32   "lX"

#define SCNo64       "I64o"
#define SCNu64       "I64u"
#define SCNx64       "I64x"
#define SCNX64       "I64X"
#define SCNoLEAST64  "I64o"
#define SCNuLEAST64  "I64u"
#define SCNxLEAST64  "I64x"
#define SCNXLEAST64  "I64X"
#define SCNoFAST64   "I64o"
#define SCNuFAST64   "I64u"
#define SCNxFAST64   "I64x"
#define SCNXFAST64   "I64X"

#define SCNoMAX     "I64o"
#define SCNuMAX     "I64u"
#define SCNxMAX     "I64x"
#define SCNXMAX     "I64X"

#ifdef _WIN64 // [
#  define SCNoPTR     "I64o"
#  define SCNuPTR     "I64u"
#  define SCNxPTR     "I64x"
#  define SCNXPTR     "I64X"
#else  // _WIN64 ][
#  define SCNoPTR     "lo"
#  define SCNuPTR     "lu"
#  define SCNxPTR     "lx"
#  define SCNXPTR     "lX"
#endif  // _WIN64 ]

#endif // __STDC_FORMAT_MACROS ]

namespace idx2 {

#define idx2_NumberTypes\
  int8, uint8, int16, uint16, int32, uint32, int64, uint64, float32, float64

using uint    = unsigned int;
using byte    = uint8_t;
using int8    = int8_t;
using i8      = int8;
using int16   = int16_t;
using i16     = int16;
using int32   = int32_t;
using i32     = int32;
using int64   = int64_t;
using i64     = int64;
using uint8   = uint8_t;
using u8      = uint8;
using uint16  = uint16_t;
using u16     = uint16;
using uint32  = uint32_t;
using u32     = uint32;
using uint64  = uint64_t;
using u64     = uint64;
using float32 = float;
using f32     = float32;
using float64 = double;
using f64     = float64;
using str     = char*;
using cstr    = const char*;

idx2_T(t)
struct traits {
  // using signed_t =
  // using unsigned_t =
  // using integral_t =
  // static constexpr uint NBinaryMask =
  // static constexpr int ExpBits
  // static constexpr int ExpBias
};

/* type traits stuffs */

struct true_type { static constexpr bool Value = true; };
struct false_type { static constexpr bool Value = false; };
idx2_T(t) struct remove_const { typedef t type; };
idx2_T(t) struct remove_const<const t> { typedef t type; };
idx2_T(t) struct remove_volatile { typedef t type; };
idx2_T(t) struct remove_volatile<volatile t> { typedef t type; };
idx2_T(t) struct remove_cv { typedef typename remove_volatile<typename remove_const<t>::type>::type type; };
idx2_T(t) struct remove_reference { typedef t type; };
idx2_T(t) struct remove_reference<t&> { typedef t type; };
idx2_T(t) struct remove_reference<t&&> { typedef t type; };
idx2_T(t) struct remove_cv_ref { typedef typename remove_cv<typename remove_reference<t>::type>::type type; };
idx2_TT(t1, t2) struct is_same_type : false_type {};
idx2_T(t) struct is_same_type<t, t> : true_type {};
idx2_T(t) struct is_pointer_helper : false_type {};
idx2_T(t) struct is_pointer_helper<t*> : true_type {};
idx2_T(t) struct is_pointer : is_pointer_helper<typename remove_cv<t>::type> {};
idx2_T(t) auto& Value(t&& T);
idx2_T(t) struct is_integral   : false_type {};
template <> struct is_integral<i8>  : true_type  {};
template <> struct is_integral<u8>  : true_type  {};
template <> struct is_integral<i16> : true_type  {};
template <> struct is_integral<u16> : true_type  {};
template <> struct is_integral<i32> : true_type  {};
template <> struct is_integral<u32> : true_type  {};
template <> struct is_integral<i64> : true_type  {};
template <> struct is_integral<u64> : true_type  {};
template <typename t> struct is_signed   : false_type {};
template <> struct is_signed<i8>  : true_type  {};
template <> struct is_signed<i16> : true_type  {};
template <> struct is_signed<i32> : true_type  {};
template <> struct is_signed<i64> : true_type  {};
idx2_T(t) struct is_unsigned : false_type {};
template <> struct is_unsigned<u8>  : true_type {};
template <> struct is_unsigned<u16> : true_type {};
template <> struct is_unsigned<u32> : true_type {};
template <> struct is_unsigned<u64> : true_type {};
idx2_T(t) struct is_floating_point   : false_type {};
template <> struct is_floating_point<f32> : true_type  {};
template <> struct is_floating_point<f64> : true_type  {};

/* Something to replace std::array */
idx2_TI(t, N)
struct stack_array {
  static_assert(N > 0);
  //constexpr stack_array() = default;
  t Arr[N] = {};
  u8 Len = 0;
  t& operator[](int Idx) const;
};
idx2_TI(t, N) t* Begin(const stack_array<t, N>& A);
idx2_TI(t, N) t* End  (const stack_array<t, N>& A);
idx2_TI(t, N) t* RevBegin(const stack_array<t, N>& A);
idx2_TI(t, N) t* RevEnd  (const stack_array<t, N>& A);
idx2_TI(t, N) constexpr int Size(const stack_array<t, N>& A);

idx2_I(N)
struct stack_string {
  char Data[N] = {};
  u8 Len = 0;
  char& operator[](int Idx) const;
};
idx2_I(N) int Size(const stack_string<N>& S);

idx2_TT(t, u)
struct t2 {
  t First;
  u Second;
  idx2_Inline bool operator<(const t2& Other) const { return First < Other.First; }
};

/* Vector in 2D, supports .X, .UV, and [] */
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
template <typename t>
struct v2 {
  union {
    struct { t X, Y; };
    struct { t U, V; };
    struct { t Min, Max; };
    t E[2];
  };
  // inline static constexpr v2<t> Zero = v2<t>(0);
  // inline static constexpr v2<t> One  = v2<t>(1);
  v2();
  explicit constexpr v2(t V);
  constexpr v2(t X_, t Y_);
  template <typename u> v2(const v2<u>& Other);
  t& operator[](int Idx) const;
  template <typename u> v2& operator=(const v2<u>& Rhs);
};
using v2i  = v2<i32>;
using v2u  = v2<u32>;
using v2l  = v2<i64>;
using v2ul = v2<u64>;
using v2f  = v2<f32>;
using v2d  = v2<f64>;

/* Vector in 3D, supports .X, .XY, .UV, .RGB and [] */
idx2_T(t)
struct v3 {
  union {
    struct { t X, Y, Z; };
    struct { t U, V, __; };
    struct { t R, G, B; };
    struct { v2<t> XY; t Ignored0_; };
    struct { t Ignored1_; v2<t> YZ; };
    struct { v2<t> UV; t Ignored2_; };
    struct { t Ignored3_; v2<t> V__; };
    t E[3];
  };
  // inline static const v3 Zero = v3(0);
  // inline static const v3 One = v3(1);
  v3();
  explicit constexpr v3(t V);
  constexpr v3(t X_, t Y_, t Z_);
  v3(const v2<t>& V2, t Z_);
  idx2_T(u) v3(const v3<u>& Other);
  t& operator[](int Idx) const;
  idx2_T(u) v3& operator=(const v3<u>& Rhs);
};
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
using v3i  = v3<i32>;
using v3u  = v3<u32>;
using v3l  = v3<i64>;
using v3ul = v3<u64>;
using v3f  = v3<f32>;
using v3d  = v3<f64>;

#define idx2_PrStrV3i "(%d %d %d)"
#define idx2_PrV3i(P) (P).X, (P).Y, (P).Z

/* 3-level nested for loop */
#define idx2_BeginFor3(Counter, Begin, End, Step)
#define idx2_EndFor3
#define idx2_BeginFor3Lockstep(C1, B1, E1, S1, C2, B2, E2, S2)

} // namespace idx2

#include <assert.h>
#include <stdio.h>

namespace idx2 {

idx2_Ti(t) auto&
Value(t&& T) {
  if constexpr (is_pointer<typename remove_reference<t>::type>::Value)
    return *T;
  else
    return T;
}

template <>
struct traits<i8> {
  using signed_t   = i8;
  using unsigned_t = u8;
  static constexpr u8 NBinaryMask = 0xaa;
  static constexpr i8 Min = -(1 << 7);
  static constexpr i8 Max = (1 << 7) - 1;
};

template <>
struct traits<u8> {
  using signed_t   = i8;
  using unsigned_t = u8;
  static constexpr u8 NBinaryMask = 0xaa;
  static constexpr u8 Min = 0;
  static constexpr u8 Max = (1 << 8) - 1;
};

template <>
struct traits<i16> {
  using signed_t   = i16;
  using unsigned_t = u16;
  static constexpr u16 NBinaryMask = 0xaaaa;
  static constexpr i16 Min = -(1 << 15);
  static constexpr i16 Max = (1 << 15) - 1;
};

template <>
struct traits<u16> {
  using signed_t   = i16;
  using unsigned_t = u16;
  static constexpr u16 NBinaryMask = 0xaaaa;
  static constexpr u16 Min = 0;
  static constexpr u16 Max = (1 << 16) - 1;
};

template <>
struct traits<i32> {
  using signed_t   = i32;
  using unsigned_t = u32;
  using floating_t = f32;
  static constexpr u32 NBinaryMask = 0xaaaaaaaa;
  static constexpr i32 Min = i32(0x80000000);
  static constexpr i32 Max = 0x7fffffff;
};

template <>
struct traits<u32> {
  using signed_t   = i32;
  using unsigned_t = u32;
  static constexpr u32 NBinaryMask = 0xaaaaaaaa;
  static constexpr u32 Min = 0;
  static constexpr u32 Max = 0xffffffff;
};

template <>
struct traits<i64> {
  using signed_t   = i64;
  using unsigned_t = u64;
  using floating_t = f64;
  static constexpr u64 NBinaryMask = 0xaaaaaaaaaaaaaaaaULL;
  static constexpr i64 Min = 0x8000000000000000ll;
  static constexpr i64 Max = 0x7fffffffffffffffull;
};

template <>
struct traits<u64> {
  using signed_t   = i64;
  using unsigned_t = u64;
  static constexpr u64 NBinaryMask = 0xaaaaaaaaaaaaaaaaULL;
  static constexpr u64 Min = 0;
  static constexpr u64 Max = 0xffffffffffffffffull;
};

template <>
struct traits<f32> {
  using integral_t = i32;
  static constexpr int ExpBits = 8;
  static constexpr int ExpBias = (1 << (ExpBits - 1)) - 1;
  static constexpr f32 Min = -FLT_MAX;
  static constexpr f32 Max =  FLT_MAX;
};

template <>
struct traits<f64> {
  using integral_t = i64;
  static constexpr int ExpBits = 11;
  static constexpr int ExpBias = (1 << (ExpBits - 1)) - 1;
  static constexpr f64 Min = -DBL_MAX;
  static constexpr f64 Max =  DBL_MAX;
};

idx2_TIi(t, N) t& stack_array<t, N>::
operator[](int Idx) const {
  assert(Idx < N);
  return const_cast<t&>(Arr[Idx]);
}
idx2_TIi(t, N) t* Begin   (const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]); }
idx2_TIi(t, N) t* End     (const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]) + N; }
idx2_TIi(t, N) t* RevBegin(const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]) + (N - 1); }
idx2_TIi(t, N) t* RevEnd  (const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]) - 1; }
idx2_TIi(t, N) constexpr int Size(const stack_array<t, N>&) { return N; }

idx2_Ii(N) char& stack_string<N>::
operator[](int Idx) const {
  assert(Idx < N);
  return const_cast<char&>(Data[Idx]);
}
idx2_Ii(N) int Size(const stack_string<N>& S) { return S.Len; }

/* v2 stuffs */
idx2_Ti(t) v2<t>::v2() {}
idx2_Ti(t) constexpr v2<t>::v2(t V): X(V), Y(V) {}
idx2_Ti(t) constexpr v2<t>::v2(t X_, t Y_): X(X_), Y(Y_) {}
idx2_T(t) idx2_Ti(u) v2<t>::v2(const v2<u>& Other) : X(Other.X), Y(Other.Y) {}
idx2_Ti(t) t& v2<t>::operator[](int Idx) const { assert(Idx < 2); return const_cast<t&>(E[Idx]); }
idx2_T(t) idx2_Ti(u) v2<t>& v2<t>::operator=(const v2<u>& other) { X = other.X; Y = other.Y; return *this; }

/* v3 stuffs */
idx2_Ti(t) v3<t>::v3() {}
idx2_Ti(t) constexpr v3<t>::v3(t V): X(V), Y(V), Z(V) {}
idx2_Ti(t) constexpr v3<t>::v3(t X_, t Y_, t Z_): X(X_), Y(Y_), Z(Z_) {}
idx2_Ti(t) v3<t>::v3(const v2<t>& V2, t Z_) : X(V2.X), Y(V2.Y), Z(Z_) {}
idx2_T(t) idx2_Ti(u) v3<t>::v3(const v3<u>& Other) : X(Other.X), Y(Other.Y), Z(Other.Z) {}
idx2_Ti(t) t& v3<t>::operator[](int Idx) const { assert(Idx < 3); return const_cast<t&>(E[Idx]); }
idx2_T(t) idx2_Ti(u) v3<t>& v3<t>::operator=(const v3<u>& Rhs) { X = Rhs.X; Y = Rhs.Y; Z = Rhs.Z; return *this; }

// TODO: move the following to idx2_macros.h?
#undef idx2_BeginFor3
#define idx2_BeginFor3(Counter, Begin, End, Step)\
  for (Counter.Z = (Begin).Z; Counter.Z < (End).Z; Counter.Z += (Step).Z) {\
  for (Counter.Y = (Begin).Y; Counter.Y < (End).Y; Counter.Y += (Step).Y) {\
  for (Counter.X = (Begin).X; Counter.X < (End).X; Counter.X += (Step).X)

#undef idx2_EndFor3
#define idx2_EndFor3 }}

#undef idx2_BeginFor3Lockstep
#define idx2_BeginFor3Lockstep(C1, B1, E1, S1, C2, B2, E2, S2)\
  (void)E2;\
  for (C1.Z = (B1).Z, C2.Z = (B2).Z; C1.Z < (E1).Z; C1.Z += (S1).Z, C2.Z += (S2).Z) {\
  for (C1.Y = (B1).Y, C2.Y = (B2).Y; C1.Y < (E1).Y; C1.Y += (S1).Y, C2.Y += (S2).Y) {\
  for (C1.X = (B1).X, C2.X = (B2).X; C1.X < (E1).X; C1.X += (S1).X, C2.X += (S2).X)

} // namespace idx2


