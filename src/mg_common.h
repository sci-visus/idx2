#pragma once

#include <inttypes.h>
#include <float.h>
#include <stdint.h>
#include "mg_macros.h"
//#include <crtdbg.h>
namespace mg {

#define mg_NumberTypes\
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

mg_T(t)
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
mg_T(t) struct remove_const { typedef t type; };
mg_T(t) struct remove_const<const t> { typedef t type; };
mg_T(t) struct remove_volatile { typedef t type; };
mg_T(t) struct remove_volatile<volatile t> { typedef t type; };
mg_T(t) struct remove_cv { typedef typename remove_volatile<typename remove_const<t>::type>::type type; };
mg_T(t) struct remove_reference { typedef t type; };
mg_T(t) struct remove_reference<t&> { typedef t type; };
mg_T(t) struct remove_reference<t&&> { typedef t type; };
mg_T(t) struct remove_cv_ref { typedef typename remove_cv<typename remove_reference<t>::type>::type type; };
mg_TT(t1, t2) struct is_same_type : false_type {};
mg_T(t) struct is_same_type<t, t> : true_type {};
mg_T(t) struct is_pointer_helper : false_type {};
mg_T(t) struct is_pointer_helper<t*> : true_type {};
mg_T(t) struct is_pointer : is_pointer_helper<typename remove_cv<t>::type> {};
mg_T(t) auto& Value(t&& T);
mg_T(t) struct is_integral   : false_type {};
mg_T() struct is_integral<i8>  : true_type  {};
mg_T() struct is_integral<u8>  : true_type  {};
mg_T() struct is_integral<i16> : true_type  {};
mg_T() struct is_integral<u16> : true_type  {};
mg_T() struct is_integral<i32> : true_type  {};
mg_T() struct is_integral<u32> : true_type  {};
mg_T() struct is_integral<i64> : true_type  {};
mg_T() struct is_integral<u64> : true_type  {};
mg_T(t) struct is_signed   : false_type {};
mg_T() struct is_signed<i8>  : true_type  {};
mg_T() struct is_signed<i16> : true_type  {};
mg_T() struct is_signed<i32> : true_type  {};
mg_T() struct is_signed<i64> : true_type  {};
mg_T(t) struct is_unsigned : false_type {};
mg_T() struct is_unsigned<u8>  : true_type {};
mg_T() struct is_unsigned<u16> : true_type {};
mg_T() struct is_unsigned<u32> : true_type {};
mg_T() struct is_unsigned<u64> : true_type {};
mg_T(t) struct is_floating_point   : false_type {};
mg_T() struct is_floating_point<f32> : true_type  {};
mg_T() struct is_floating_point<f64> : true_type  {};

/* Something to replace std::array */
mg_TI(t, N)
struct stack_array {
  static_assert(N > 0);
  //constexpr stack_array() = default;
  t Arr[N] = {};
  u8 Len = 0;
  t& operator[](int Idx) const;
};
mg_TI(t, N) t* Begin(const stack_array<t, N>& A);
mg_TI(t, N) t* End  (const stack_array<t, N>& A);
mg_TI(t, N) t* RevBegin(const stack_array<t, N>& A);
mg_TI(t, N) t* RevEnd  (const stack_array<t, N>& A);
mg_TI(t, N) constexpr int Size(const stack_array<t, N>& A);

mg_I(N)
struct stack_string {
  char Data[N] = {};
  u8 Len = 0;
  char& operator[](int Idx) const;
};
mg_I(N) int Size(const stack_string<N>& S);

mg_TT(t, u)
struct t2 {
  t First;
  u Second;
  mg_Inline bool operator<(const t2& Other) const { return First < Other.First; }
};

/* Vector in 2D, supports .X, .UV, and [] */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
mg_T(t)
struct v2 {
  union {
    struct { t X, Y; };
    struct { t U, V; };
    struct { t Min, Max; };
    t E[2];
  };
  inline static const v2 Zero = v2(0);
  inline static const v2 One = v2(1);
  v2();
  explicit constexpr v2(t V);
  constexpr v2(t X_, t Y_);
  mg_T(u) v2(const v2<u>& Other);
  t& operator[](int Idx) const;
  mg_T(u) v2& operator=(const v2<u>& Rhs);
};
using v2i  = v2<i32>;
using v2u  = v2<u32>;
using v2l  = v2<i64>;
using v2ul = v2<u64>;
using v2f  = v2<f32>;
using v2d  = v2<f64>;

/* Vector in 3D, supports .X, .XY, .UV, .RGB and [] */
mg_T(t)
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
  inline static const v3 Zero = v3(0);
  inline static const v3 One = v3(1);
  v3();
  explicit constexpr v3(t V);
  constexpr v3(t X_, t Y_, t Z_);
  v3(const v2<t>& V2, t Z_);
  mg_T(u) v3(const v3<u>& Other);
  t& operator[](int Idx) const;
  mg_T(u) v3& operator=(const v3<u>& Rhs);
};
#pragma GCC diagnostic pop
using v3i  = v3<i32>;
using v3u  = v3<u32>;
using v3l  = v3<i64>;
using v3ul = v3<u64>;
using v3f  = v3<f32>;
using v3d  = v3<f64>;

#define mg_PrStrV3i "(%d %d %d)"
#define mg_PrV3i(P) (P).X, (P).Y, (P).Z

/* 3-level nested for loop */
#define mg_BeginFor3(Counter, Begin, End, Step)
#define mg_EndFor3
#define mg_BeginFor3Lockstep(C1, B1, E1, S1, C2, B2, E2, S2)

} // namespace mg

#include <assert.h>
#include <stdio.h>

namespace mg {

mg_Ti(t) auto&
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

mg_TIi(t, N) t& stack_array<t, N>::
operator[](int Idx) const {
  assert(Idx < N);
  return const_cast<t&>(Arr[Idx]);
}
mg_TIi(t, N) t* Begin   (const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]); }
mg_TIi(t, N) t* End     (const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]) + N; }
mg_TIi(t, N) t* RevBegin(const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]) + (N - 1); }
mg_TIi(t, N) t* RevEnd  (const stack_array<t, N>& A) { return const_cast<t*>(&A.Arr[0]) - 1; }
mg_TIi(t, N) constexpr int Size(const stack_array<t, N>&) { return N; }

mg_Ii(N) char& stack_string<N>::
operator[](int Idx) const {
  assert(Idx < N);
  return const_cast<char&>(Data[Idx]);
}
mg_Ii(N) int Size(const stack_string<N>& S) { return S.Len; }

/* v2 stuffs */
mg_Ti(t) v2<t>::v2() {}
mg_Ti(t) constexpr v2<t>::v2(t V): X(V), Y(V) {}
mg_Ti(t) constexpr v2<t>::v2(t X_, t Y_): X(X_), Y(Y_) {}
mg_T(t) mg_Ti(u) v2<t>::v2(const v2<u>& Other) : X(Other.X), Y(Other.Y) {}
mg_Ti(t) t& v2<t>::operator[](int Idx) const { assert(Idx < 2); return const_cast<t&>(E[Idx]); }
mg_T(t) mg_Ti(u) v2<t>& v2<t>::operator=(const v2<u>& other) { X = other.X; Y = other.Y; return *this; }

/* v3 stuffs */
mg_Ti(t) v3<t>::v3() {}
mg_Ti(t) constexpr v3<t>::v3(t V): X(V), Y(V), Z(V) {}
mg_Ti(t) constexpr v3<t>::v3(t X_, t Y_, t Z_): X(X_), Y(Y_), Z(Z_) {}
mg_Ti(t) v3<t>::v3(const v2<t>& V2, t Z_) : X(V2.X), Y(V2.Y), Z(Z_) {}
mg_T(t) mg_Ti(u) v3<t>::v3(const v3<u>& Other) : X(Other.X), Y(Other.Y), Z(Other.Z) {}
mg_Ti(t) t& v3<t>::operator[](int Idx) const { assert(Idx < 3); return const_cast<t&>(E[Idx]); }
mg_T(t) mg_Ti(u) v3<t>& v3<t>::operator=(const v3<u>& Rhs) { X = Rhs.X; Y = Rhs.Y; Z = Rhs.Z; return *this; }

// TODO: move the following to mg_macros.h?
#undef mg_BeginFor3
#define mg_BeginFor3(Counter, Begin, End, Step)\
  for (Counter.Z = (Begin).Z; Counter.Z < (End).Z; Counter.Z += (Step).Z) {\
  for (Counter.Y = (Begin).Y; Counter.Y < (End).Y; Counter.Y += (Step).Y) {\
  for (Counter.X = (Begin).X; Counter.X < (End).X; Counter.X += (Step).X)

#undef mg_EndFor3
#define mg_EndFor3 }}

#undef mg_BeginFor3Lockstep
#define mg_BeginFor3Lockstep(C1, B1, E1, S1, C2, B2, E2, S2)\
  (void)E2;\
  for (C1.Z = (B1).Z, C2.Z = (B2).Z; C1.Z < (E1).Z; C1.Z += (S1).Z, C2.Z += (S2).Z) {\
  for (C1.Y = (B1).Y, C2.Y = (B2).Y; C1.Y < (E1).Y; C1.Y += (S1).Y, C2.Y += (S2).Y) {\
  for (C1.X = (B1).X, C2.X = (B2).X; C1.X < (E1).X; C1.X += (S1).X, C2.X += (S2).X)

} // namespace mg


