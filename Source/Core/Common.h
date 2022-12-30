#pragma once

#include <float.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
//#include <crtdbg.h>

#define idx2_Var(x, _) x


/* Avoid compiler warning about unused variable */
#define idx2_Unused(X)                                                                             \
  do                                                                                               \
  {                                                                                                \
    (void)sizeof(X);                                                                               \
  } while (0)


/* Return the number of elements in a static array */
#define idx2_ArraySize(X) int(sizeof(X) / sizeof(X[0]))


/* Return the number of comma-separated arguments */
#define idx2_NumArgs(...) (idx2::CountChar(#__VA_ARGS__, ',') + 1)


#define idx2_Expand(...) __VA_ARGS__
/* Macro overloading feature support */
#define idx2_NumArgsUpTo6(...) idx2_Expand(idx2_NumArgsHelper(0, ##__VA_ARGS__, 5, 4, 3, 2, 1, 0))
#define idx2_NumArgsHelper(_0, _1, _2, _3, _4, _5, N, ...) N
#define idx2_OverloadSelect(Name, Num) idx2_Cat(Name##_, Num)
#define idx2_MacroOverload(Name, ...)                                                              \
  idx2_OverloadSelect(Name, idx2_NumArgsUpTo6(__VA_ARGS__))(__VA_ARGS__)
// Examples:
// #define FOO(...)        idx2_MacroOverload(FOO, __VA_ARGS__)
// #define FOO_0()         "Zero"
// #define FOO_1(X)        "One"
// #define FOO_2(X, Y)     "Two"
// #define FOO_3(X, Y, Z)  "Three"


/* 2-level stringify */
#define idx2_Str(...) idx2_StrHelper(__VA_ARGS__)
#define idx2_StrHelper(...) #__VA_ARGS__


/* 2-level concat */
#define idx2_Cat(A, ...) idx2_CatHelper(A, __VA_ARGS__)
#define idx2_CatHelper(A, ...) A##__VA_ARGS__


#define idx2_FPrintHelper(Stream, ...) fprintf(Stream, ##__VA_ARGS__)


#define idx2_SPrintHelper(Buf, L, ...)                                                             \
  snprintf(Buf + L, sizeof(Buf) - size_t(L), ##__VA_ARGS__);                                       \
  idx2_Unused(L)


#define idx2_ExtractFirst(X, ...) X


#define idx2_BitSizeOf(X) ((int)sizeof(X) * 8)


/* Print binary */
#define idx2_BinPattern8 "%c%c%c%c%c%c%c%c"
#define idx2_BinPattern16 idx2_BinPattern8 idx2_BinPattern8
#define idx2_BinPattern32 idx2_BinPattern16 idx2_BinPattern16
#define idx2_BinPattern64 idx2_BinPattern32 idx2_BinPattern32
#define idx2_BinaryByte(Byte)                                                                      \
  (Byte & 0x80 ? '1' : '0'), (Byte & 0x40 ? '1' : '0'), (Byte & 0x20 ? '1' : '0'),                 \
    (Byte & 0x10 ? '1' : '0'), (Byte & 0x08 ? '1' : '0'), (Byte & 0x04 ? '1' : '0'),               \
    (Byte & 0x02 ? '1' : '0'), (Byte & 0x01 ? '1' : '0')
#define idx2_BinaryByte64(Val)                                                                     \
  idx2_BinaryByte((Val) >> 56), idx2_BinaryByte((Val) >> 48), idx2_BinaryByte((Val) >> 40),        \
    idx2_BinaryByte((Val) >> 32), idx2_BinaryByte((Val) >> 24), idx2_BinaryByte((Val) >> 16),      \
    idx2_BinaryByte((Val) >> 8), idx2_BinaryByte((Val))


#if defined(__clang__) || defined(__GNUC__)
#define idx2_Restrict __restrict__
#elif defined(_MSC_VER)
#define idx2_Restrict __restrict
#endif


#if defined(_MSC_VER)
#define idx2_Inline __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define idx2_Inline inline __attribute__((__always_inline__))
#endif


namespace idx2
{


/* Count the number of times a character appears in a string. Return -1 for an empty string. */
inline constexpr int
CountChar(const char* str, char c)
{
  int Count = 0;
  if (!(*str)) // empty string
    return -1;
  while (*str)
    Count += (*str++ == c);
  return Count;
}


} // namespace idx2


#define idx2_ForEach(It, Container) for (auto It = Begin(Container); It != End(Container); ++It)
#define idx2_For(Type, It, Begin, End) for (Type It = Begin; It != End; ++It)
#define idx2_InclusiveForBackward(Type, It, Begin, End) for (Type It = Begin; It >= End; --It)
#define idx2_InclusiveFor(Type, It, Begin, End) for (Type It = Begin; It <= End; ++It)
#define idx2_Range(...) idx2_MacroOverload(idx2_Range, __VA_ARGS__)
#define idx2_Range_1(Container) Begin(Container), End(Container)
#define idx2_Range_2(type, Container) Begin<type>(Container), End<type>(Container)
// #define idx2_If(Var, Expr) auto Var = Expr; if (Var)


/*
Macros to swap bytes in a multi-byte value, to convert from big-endian data to little-endian data
and vice versa. These are taken from the Boost library.
*/
#ifndef __has_builtin
#define __has_builtin(x) 0 // Compatibility with non-clang compilers
#endif
#if defined(_MSC_VER)
#include <cstdlib>
#define idx2_ByteSwap2(x) _byteswap_ushort(x)
#define idx2_ByteSwap4(x) _byteswap_ulong(x)
#define idx2_ByteSwap8(x) _byteswap_uint64(x)
#elif (defined(__clang__) && __has_builtin(__builtin_bswap32) &&                                   \
       __has_builtin(__builtin_bswap64)) ||                                                        \
  (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
#if (defined(__clang__) && __has_builtin(__builtin_bswap16)) ||                                    \
  (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))
#define idx2_ByteSwap2(x) __builtin_bswap16(x)
#else
#define idx2_ByteSwap2(x) __builtin_bswap32((x) << 16)
#endif
#define idx2_ByteSwap4(x) __builtin_bswap32(x)
#define idx2_ByteSwap8(x) __builtin_bswap64(x)
#elif defined(__linux__)
#include <byteswap.h>
#define idx2_ByteSwap2(x) bswap_16(x)
#define idx2_ByteSwap4(x) bswap_32(x)
#define idx2_ByteSwap8(x) bswap_64(x)
#endif


namespace idx2
{


using uint = unsigned int;
using byte = uint8_t;
using int8 = int8_t;
using i8 = int8;
using int16 = int16_t;
using i16 = int16;
using int32 = int32_t;
using i32 = int32;
using int64 = int64_t;
using i64 = int64;
using uint8 = uint8_t;
using u8 = uint8;
using uint16 = uint16_t;
using u16 = uint16;
using uint32 = uint32_t;
using u32 = uint32;
using uint64 = uint64_t;
using u64 = uint64;
using float32 = float;
using f32 = float32;
using float64 = double;
using f64 = float64;
using str = char*;
using cstr = const char*;

template <typename t> struct traits
{
  // using signed_t =
  // using unsigned_t =
  // using integral_t =
  // static constexpr uint NBinaryMask =
  // static constexpr int ExpBits
  // static constexpr int ExpBias
};

/* type traits stuffs */

struct true_type
{
  static constexpr bool Value = true;
};

struct false_type
{
  static constexpr bool Value = false;
};


template <typename t1, typename t2> struct is_same_type : false_type
{
};

template <typename t> struct is_same_type<t, t> : true_type
{
};


template <typename t> auto& Value(t&& T);

template <typename t> struct is_integral : false_type
{
};

template <> struct is_integral<i8> : true_type
{
};

template <> struct is_integral<u8> : true_type
{
};

template <> struct is_integral<i16> : true_type
{
};

template <> struct is_integral<u16> : true_type
{
};

template <> struct is_integral<i32> : true_type
{
};

template <> struct is_integral<u32> : true_type
{
};

template <> struct is_integral<i64> : true_type
{
};

template <> struct is_integral<u64> : true_type
{
};

template <typename t> struct is_signed : false_type
{
};

template <> struct is_signed<i8> : true_type
{
};

template <> struct is_signed<i16> : true_type
{
};

template <> struct is_signed<i32> : true_type
{
};

template <> struct is_signed<i64> : true_type
{
};

template <typename t> struct is_unsigned : false_type
{
};

template <> struct is_unsigned<u8> : true_type
{
};

template <> struct is_unsigned<u16> : true_type
{
};

template <> struct is_unsigned<u32> : true_type
{
};

template <> struct is_unsigned<u64> : true_type
{
};

template <typename t> struct is_floating_point : false_type
{
};

template <> struct is_floating_point<f32> : true_type
{
};

template <> struct is_floating_point<f64> : true_type
{
};

/* Something to replace std::array */
template <typename t, int N> struct stack_array
{
  static_assert(N > 0);
  // constexpr stack_array() = default;
  t Arr[N] = {};
  i8 Size = 0;
  t& operator[](int Idx) const;

  idx2_Inline static constexpr int Capacity() { return N; }
};

template <typename t, int N> t* Begin(const stack_array<t, N>& A);
template <typename t, int N> t* End(const stack_array<t, N>& A);
template <typename t, int N> t* RevBegin(const stack_array<t, N>& A);
template <typename t, int N> t* RevEnd(const stack_array<t, N>& A);

template <typename t, int N> void
PushBack(stack_array<t, N>* A, const t& Item)
{
  //idx2_Assert(A->Size < A->Capacity()); // TODO NEXT
  A->Arr[A->Size++] = Item;
}

template<typename t, int N> void
Resize(stack_array<t, N>* A, u8 Size)
{
  A->Size = Size;
}

template <i8 N> struct stack_string
{
  char Data[N] = {};
  i8 Size = 0;
  char& operator[](i8 Idx) const;
  stack_string<N>& operator=(const stack_string<N>& Other);
  idx2_Inline static constexpr i8 Capacity(){ return N; };
};

template <i8 N> int Size(const stack_string<N>& S);


template <i8 N> stack_string<N>&
stack_string<N>::operator=(const stack_string<N>& Other)
{
  this->Size = Other.Size;
  memcpy(this->Data, Other.Data, this->Size);
  return *this;
}

template <typename t, typename u> struct t2
{
  t First;
  u Second;
  idx2_Inline bool
  operator<(const t2& Other) const
  {
    return First < Other.First;
  }
};

/* Vector in 2D, supports .X, .UV, and [] */
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

template <typename t> struct v2
{
  union
  {
    struct
    {
      t X, Y;
    };
    struct
    {
      t U, V;
    };
    struct
    {
      t Min, Max;
    };
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

using v2i = v2<i32>;
using v2u = v2<u32>;
using v2l = v2<i64>;
using v2ul = v2<u64>;
using v2f = v2<f32>;
using v2d = v2<f64>;

/* Vector in 3D, supports .X, .XY, .UV, .RGB and [] */
template <typename t> struct v3
{
  union
  {
    struct
    {
      t X, Y, Z;
    };
    struct
    {
      t U, V, __;
    };
    struct
    {
      t R, G, B;
    };
    struct
    {
      v2<t> XY;
      t Ignored0_;
    };
    struct
    {
      t Ignored1_;
      v2<t> YZ;
    };
    struct
    {
      v2<t> UV;
      t Ignored2_;
    };
    struct
    {
      t Ignored3_;
      v2<t> V__;
    };
    t E[3];
  };
  // inline static const v3 Zero = v3(0);
  // inline static const v3 One = v3(1);
  v3();
  explicit constexpr v3(t V);
  constexpr v3(t X_, t Y_, t Z_);
  v3(const v2<t>& V2, t Z_);
  template <typename u> v3(const v3<u>& Other);
  t& operator[](int Idx) const;
  template <typename u> v3& operator=(const v3<u>& Rhs);
};

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

using v3i = v3<i32>;
using v3u = v3<u32>;
using v3l = v3<i64>;
using v3ul = v3<u64>;
using v3f = v3<f32>;
using v3d = v3<f64>;

#define idx2_PrStrV3i "(%d %d %d)"
#define idx2_PrV3i(P) (P).X, (P).Y, (P).Z

/* 3-level nested for loop */
#define idx2_BeginFor3(Counter, Begin, End, Step)
#define idx2_EndFor3
#define idx2_BeginFor3Lockstep(C1, B1, E1, S1, C2, B2, E2, S2)


template <typename t> struct v6
{
  union
  {
    struct
    {
      v3<t> XYZ;
      v3<t> UVW;
    };
    t E[6];
  };
  v6();
  explicit constexpr v6(t V);
  constexpr v6(t X_, t Y_, t Z_, t U_, t V_, t W_);
  template <typename u> v6(const v3<u>& P1, const v3<u>& P2);
  template <typename u> v6(const v6<u>& Other);
  t& operator[](int Idx) const;
  template <typename u> v6& operator=(const v6<u>& Rhs);
  idx2_Inline static i8 constexpr Size() { return sizeof(E) / sizeof(E[0]); }
};

using v6i = v6<i32>;
using v6u = v6<u32>;
using v6l = v6<i64>;
using v6ul = v6<u64>;
using v6f = v6<f32>;
using v6d = v6<f64>;

using nd_string = v6<u8>;
using nd_size = v6i;


} // namespace idx2


#include <assert.h>
#include <stdio.h>


namespace idx2
{


template <typename t> idx2_Inline auto&
Value(t&& T)
{
  if constexpr (is_pointer<typename remove_reference<t>::type>::Value)
    return *T;
  else
    return T;
}


template <> struct traits<i8>
{
  using signed_t = i8;
  using unsigned_t = u8;
  static constexpr u8 NBinaryMask = 0xaa;
  static constexpr i8 Min = -(1 << 7);
  static constexpr i8 Max = (1 << 7) - 1;
};


template <> struct traits<u8>
{
  using signed_t = i8;
  using unsigned_t = u8;
  static constexpr u8 NBinaryMask = 0xaa;
  static constexpr u8 Min = 0;
  static constexpr u8 Max = (1 << 8) - 1;
};


template <> struct traits<i16>
{
  using signed_t = i16;
  using unsigned_t = u16;
  static constexpr u16 NBinaryMask = 0xaaaa;
  static constexpr i16 Min = -(1 << 15);
  static constexpr i16 Max = (1 << 15) - 1;
};


template <> struct traits<u16>
{
  using signed_t = i16;
  using unsigned_t = u16;
  static constexpr u16 NBinaryMask = 0xaaaa;
  static constexpr u16 Min = 0;
  static constexpr u16 Max = (1 << 16) - 1;
};


template <> struct traits<i32>
{
  using signed_t = i32;
  using unsigned_t = u32;
  using floating_t = f32;
  static constexpr u32 NBinaryMask = 0xaaaaaaaa;
  static constexpr i32 Min = i32(0x80000000);
  static constexpr i32 Max = 0x7fffffff;
};


template <> struct traits<u32>
{
  using signed_t = i32;
  using unsigned_t = u32;
  static constexpr u32 NBinaryMask = 0xaaaaaaaa;
  static constexpr u32 Min = 0;
  static constexpr u32 Max = 0xffffffff;
};


template <> struct traits<i64>
{
  using signed_t = i64;
  using unsigned_t = u64;
  using floating_t = f64;
  static constexpr u64 NBinaryMask = 0xaaaaaaaaaaaaaaaaULL;
  static constexpr i64 Min = 0x8000000000000000ll;
  static constexpr i64 Max = 0x7fffffffffffffffull;
};


template <> struct traits<u64>
{
  using signed_t = i64;
  using unsigned_t = u64;
  static constexpr u64 NBinaryMask = 0xaaaaaaaaaaaaaaaaULL;
  static constexpr u64 Min = 0;
  static constexpr u64 Max = 0xffffffffffffffffull;
};


template <> struct traits<f32>
{
  using integral_t = i32;
  static constexpr int ExpBits = 8;
  static constexpr int ExpBias = (1 << (ExpBits - 1)) - 1;
  static constexpr f32 Min = -FLT_MAX;
  static constexpr f32 Max = FLT_MAX;
};


template <> struct traits<f64>
{
  using integral_t = i64;
  static constexpr int ExpBits = 11;
  static constexpr int ExpBias = (1 << (ExpBits - 1)) - 1;
  static constexpr f64 Min = -DBL_MAX;
  static constexpr f64 Max = DBL_MAX;
};


template <typename t, int N> idx2_Inline t&
stack_array<t, N>::operator[](int Idx) const
{
  assert(Idx < N);
  return const_cast<t&>(Arr[Idx]);
}


template <typename t, int N> idx2_Inline t*
Begin(const stack_array<t, N>& A)
{
  return const_cast<t*>(&A.Arr[0]);
}


template <typename t, int N> idx2_Inline t*
End(const stack_array<t, N>& A)
{
  return const_cast<t*>(&A.Arr[0]) + N;
}


template <typename t, int N> idx2_Inline t*
RevBegin(const stack_array<t, N>& A)
{
  return const_cast<t*>(&A.Arr[0]) + (N - 1);
}


template <typename t, int N> idx2_Inline t*
RevEnd(const stack_array<t, N>& A)
{
  return const_cast<t*>(&A.Arr[0]) - 1;
}


template <typename t, int N> idx2_Inline i8
Size(const stack_array<t, N>& A)
{
  return A.Size;
}


template <i8 N> idx2_Inline char&
stack_string<N>::operator[](i8 Idx) const
{
  assert(Idx < N);
  return const_cast<char&>(Data[Idx]);
}


template <i8 N> idx2_Inline int
Size(const stack_string<N>& S)
{
  return S.Size;
}


/* v2 stuffs */
template <typename t> idx2_Inline
v2<t>::v2()
{
}


template <typename t> idx2_Inline constexpr v2<t>::v2(t V)
  : X(V)
  , Y(V)
{
}


template <typename t> idx2_Inline constexpr v2<t>::v2(t X_, t Y_)
  : X(X_)
  , Y(Y_)
{
}


template <typename t> template <typename u> idx2_Inline
v2<t>::v2(const v2<u>& Other)
  : X(Other.X)
  , Y(Other.Y)
{
}


template <typename t> idx2_Inline t&
v2<t>::operator[](int Idx) const
{
  assert(Idx < 2);
  return const_cast<t&>(E[Idx]);
}


template <typename t> template <typename u> idx2_Inline v2<t>&
v2<t>::operator=(const v2<u>& other)
{
  X = other.X;
  Y = other.Y;
  return *this;
}


/* v3 stuffs */
template <typename t> idx2_Inline
v3<t>::v3()
{
}


template <typename t> idx2_Inline constexpr v3<t>::v3(t V)
  : X(V)
  , Y(V)
  , Z(V)
{
}


template <typename t> idx2_Inline constexpr v3<t>::v3(t X_, t Y_, t Z_)
  : X(X_)
  , Y(Y_)
  , Z(Z_)
{
}


template <typename t> idx2_Inline
v3<t>::v3(const v2<t>& V2, t Z_)
  : X(V2.X)
  , Y(V2.Y)
  , Z(Z_)
{
}


template <typename t> template <typename u> idx2_Inline
v3<t>::v3(const v3<u>& Other)
  : X(Other.X)
  , Y(Other.Y)
  , Z(Other.Z)
{
}


template <typename t> idx2_Inline t&
v3<t>::operator[](int Idx) const
{
  assert(Idx < 3);
  return const_cast<t&>(E[Idx]);
}


template <typename t> template <typename u> idx2_Inline v3<t>&
v3<t>::operator=(const v3<u>& Rhs)
{
  X = Rhs.X;
  Y = Rhs.Y;
  Z = Rhs.Z;
  return *this;
}


/* v6 stuffs*/
template <typename t> idx2_Inline
v6<t>::v6()
  : XYZ(1)
  , UVW(1)
{
}


template <typename t> idx2_Inline constexpr v6<t>::v6(t V)
  : XYZ(V)
  , UVW(V)
{
}


template <typename t> idx2_Inline constexpr v6<t>::v6(t X_, t Y_, t Z_, t U_, t V_, t W_)
  : XYZ(X_, Y_, Z_)
  , UVW(U_, V_, W_)
{
}


template <typename t> template <typename u> idx2_Inline
v6<t>::v6(const v3<u>& XYZ_, const v3<u>& UVW_)
  : XYZ(XYZ_)
  , UVW(UVW_)
{
}


template <typename t> template <typename u> idx2_Inline
v6<t>::v6(const v6<u>& Other)
  : XYZ(Other.XYZ)
  , UVW(Other.UVW)
{
}


template <typename t> idx2_Inline t&
v6<t>::operator[](int Idx) const
{
  assert(Idx < 6);
  return const_cast<t&>(E[Idx]);
}


template <typename t> template <typename u> idx2_Inline v6<t>&
v6<t>::operator=(const v6<u>& Rhs)
{
  XYZ = Rhs.XYZ;
  UVW = Rhs.UVW;
  return *this;
}


template <typename t> idx2_Inline constexpr void
Swap(t* A, t* idx2_Restrict B)
{
  t T = *A;
  *A = *B;
  *B = T;
}


idx2_Inline nd_size
SetDimension(nd_size P, i8 D, i32 Val)
{
  P[D] = Val;
  return P;
}


idx2_Inline i8
EffectiveDims(const nd_size& Dims)
{
  i8 D = Dims.Size() - 1;
  while ((D >= 0) && (Dims[D] == 1))
    --D;
  return D + 1;
}


/* Put dimension D at index 0 so it becomes the fastest varying dimension. */
idx2_Inline nd_size
MakeFastestDimension(nd_size P, i8 D)
{
  while (D > 0)
  {
    Swap(&P[D - 1], &P[D]);
    --D;
  }
  return P;
}


template <typename t> idx2_Inline void
ndLoop(const nd_size& Begin, const nd_size& End, const nd_size& Step, const t& Kernel)
{
  i8 D = EffectiveDims(End - Begin);
  //idx2_Assert(D <= 6);
  int X, Y, Z, U, V, W;
  switch (D)
  {
    case 0:
      //idx2_ExitIf(false, "Zero dimensional input\n");
      break;
    case 1:
      _Pragma("omp parallel for")
      for (X = Begin[0]; X < End[0]; X += Step[0])
        Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 2:
      _Pragma("omp parallel for collapse(2)")
      for (Y = Begin[1]; Y < End[1]; Y += Step[1])
        for (X = Begin[0]; X < End[0]; X += Step[0])
          Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 3:
      _Pragma("omp parallel for collapse(2)")
      for (Z = Begin[2]; Z < End[2]; Z += Step[2])
        for (Y = Begin[1]; Y < End[1]; Y += Step[1])
          for (X = Begin[0]; X < End[0]; X += Step[0])
            Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 4:
      _Pragma("omp parallel for collapse(2)")
      for (U = Begin[3]; U < End[3]; U += Step[3])
        for (Z = Begin[2]; Z < End[2]; Z += Step[2])
          for (Y = Begin[1]; Y < End[1]; Y += Step[1])
            for (X = Begin[0]; X < End[0]; X += Step[0])
              Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 5:
      _Pragma("omp parallel for collapse(2)")
      for (V = Begin[4]; V < End[4]; V += Step[4])
        for (U = Begin[3]; U < End[3]; U += Step[3])
          for (Z = Begin[2]; Z < End[2]; Z += Step[2])
            for (Y = Begin[1]; Y < End[1]; Y += Step[1])
              for (X = Begin[0]; X < End[0]; X += Step[0])
                Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 6:
      _Pragma("omp parallel for collapse(2)")
      for (W = Begin[5]; W < End[5]; W += Step[5])
        for (V = Begin[4]; V < End[4]; V += Step[4])
          for (U = Begin[3]; U < End[3]; U += Step[3])
            for (Z = Begin[2]; Z < End[2]; Z += Step[2])
              for (Y = Begin[1]; Y < End[1]; Y += Step[1])
                for (X = Begin[0]; X < End[0]; X += Step[0])
                  Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    default:
      //idx2_Exit("Effective dimensionality greater than 6\n");
      break;
  };
}


/* Like ndLoop but exclude the fastest varying dimension (0) */
template <typename t> idx2_Inline void
ndOuterLoop(const nd_size& Begin, const nd_size& End, const nd_size& Step, const t& Kernel)
{
  i8 D = EffectiveDims(End);
  //idx2_Assert(D <= 6);
  int X, Y, Z, U, V, W;
  switch (D)
  {
    case 0:
      //idx2_Exit("Zero dimensional input\n");
      break;
    case 1:
      break;
    case 2:
      _Pragma("omp parallel for")
      for (Y = Begin[1]; Y < End[1]; Y += Step[1])
        Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 3:
      _Pragma("omp parallel for collapse(2)")
      for (Z = Begin[2]; Z < End[2]; Z += Step[2])
        for (Y = Begin[1]; Y < End[1]; Y += Step[1])
          Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 4:
      _Pragma("omp parallel for collapse(2)")
      for (U = Begin[3]; U < End[3]; U += Step[3])
        for (Z = Begin[2]; Z < End[2]; Z += Step[2])
          for (Y = Begin[1]; Y < End[1]; Y += Step[1])
            Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 5:
      _Pragma("omp parallel for collapse(2)")
      for (V = Begin[4]; V < End[4]; V += Step[4])
        for (U = Begin[3]; U < End[3]; U += Step[3])
          for (Z = Begin[2]; Z < End[2]; Z += Step[2])
            for (Y = Begin[1]; Y < End[1]; Y += Step[1])
              Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    case 6:
      _Pragma("omp parallel for collapse(2)")
      for (W = Begin[5]; W < End[5]; W += Step[5])
        for (V = Begin[4]; V < End[4]; V += Step[4])
          for (U = Begin[3]; U < End[3]; U += Step[3])
            for (Z = Begin[2]; Z < End[2]; Z += Step[2])
              for (Y = Begin[1]; Y < End[1]; Y += Step[1])
                Kernel(nd_size(X, Y, Z, U, V, W));
      break;
    default:
      //idx2_Exit("Effective dimensionality greater than 6\n");
      break;
  };
}

// TODO: move the following to Macros.h?
#undef idx2_BeginFor3
#define idx2_BeginFor3(Counter, Begin, End, Step)                                                  \
  for (Counter.Z = (Begin).Z; Counter.Z < (End).Z; Counter.Z += (Step).Z)                          \
  {                                                                                                \
    for (Counter.Y = (Begin).Y; Counter.Y < (End).Y; Counter.Y += (Step).Y)                        \
    {                                                                                              \
      for (Counter.X = (Begin).X; Counter.X < (End).X; Counter.X += (Step).X)

#undef idx2_EndFor3
#define idx2_EndFor3                                                                               \
  }                                                                                                \
  }


#undef idx2_BeginFor3Lockstep
#define idx2_BeginFor3Lockstep(C1, B1, E1, S1, C2, B2, E2, S2)                                     \
  (void)E2;                                                                                        \
  for (C1.Z = (B1).Z, C2.Z = (B2).Z; C1.Z < (E1).Z; C1.Z += (S1).Z, C2.Z += (S2).Z)                \
  {                                                                                                \
    for (C1.Y = (B1).Y, C2.Y = (B2).Y; C1.Y < (E1).Y; C1.Y += (S1).Y, C2.Y += (S2).Y)              \
    {                                                                                              \
      for (C1.X = (B1).X, C2.X = (B2).X; C1.X < (E1).X; C1.X += (S1).X, C2.X += (S2).X)


idx2_Inline bool
StrEqual(cstr S, cstr T)
{
  return strcmp(S, T) == 0;
}


idx2_Inline bool
StrEqualOneOf(cstr S, cstr A, cstr B, cstr C = nullptr)
{
  return StrEqual(S, A) || StrEqual(S, B) || StrEqual(S, C);
}


template <i8 N> idx2_Inline bool
FGets(stack_string<N>* Str)
{
  if (fgets(Str->Data, Str->Capacity(), stdin))
  {
    Str->Size = i8(strlen(Str->Data) - 1); // avoid the '\n' at the end
    Str->Data[Str->Size] = 0;
    return true;
  }
  return false;
}

} // namespace idx2

