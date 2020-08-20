#pragma once

/* Avoid compiler warning about unused variable */
#define idx2_Unused(X) do { (void)sizeof(X); } while(0)

/* Return the number of elements in a static array */
#define idx2_ArraySize(X) int(sizeof(X) / sizeof(X[0]))

/* Return the number of comma-separated arguments */
#define idx2_NumArgs(...) (idx2::CountChar(#__VA_ARGS__, ',') + 1)

#define idx2_Expand(...) __VA_ARGS__
/* Macro overloading feature support */
#define idx2_NumArgsUpTo6(...) idx2_Expand(idx2_NumArgsHelper(0, ## __VA_ARGS__, 5,4,3,2,1,0))
#define idx2_NumArgsHelper(_0,_1,_2,_3,_4,_5,N,...) N
#define idx2_OverloadSelect(Name, Num) idx2_Cat(Name ## _, Num)
#define idx2_MacroOverload(Name, ...) idx2_OverloadSelect(Name, idx2_NumArgsUpTo6(__VA_ARGS__))(__VA_ARGS__)
// Examples:
// #define FOO(...)       idx2_MacroOverload(FOO, __VA_ARGS__)
// #define FOO_0()        "Zero"
// #define FOO_1(X)       "One"
// #define FOO_2(X, Y)    "Two"
// #define FOO_3(X, Y, Z) "Three"

/* 2-level stringify */
#define idx2_Str(...) idx2_StrHelper(__VA_ARGS__)
#define idx2_StrHelper(...) #__VA_ARGS__

/* 2-level concat */
#define idx2_Cat(A, ...) idx2_CatHelper(A, __VA_ARGS__)
#define idx2_CatHelper(A, ...) A ## __VA_ARGS__

#define idx2_FPrintHelper(Stream, ...) fprintf(Stream, ##__VA_ARGS__)

#define idx2_SPrintHelper(Buf, L, ...) snprintf(Buf + L, sizeof(Buf) - size_t(L), ##__VA_ARGS__); idx2_Unused(L)

#define idx2_ExtractFirst(X, ...) X

#define idx2_BitSizeOf(X) ((int)sizeof(X) * 8)

#define idx2_Restrict

#define idx2_Inline

/* Short for template <typename ...> which sometimes can get too verbose */
#define idx2_T(...) template <typename __VA_ARGS__>
#define idx2_I(N) template <int N>
#define idx2_Ii(N) idx2_I(N) idx2_Inline
#define idx2_TI(t, N) template <typename t, int N>
#define idx2_TIi(t, N) idx2_TI(t, N) idx2_Inline
#define idx2_TTI(t, u, N) template <typename t, typename u, int N>
#define idx2_TII(t, N, M) template <typename t, int N, int M>
#define idx2_TT(t, u) template <typename t, typename u>
#define idx2_Ti(t) idx2_T(t) idx2_Inline
#define idx2_TTi(t, u) idx2_TT(t, u) idx2_Inline

/* Print binary */
#define idx2_BinPattern8 "%c%c%c%c%c%c%c%c"
#define idx2_BinPattern16 idx2_BinPattern8  idx2_BinPattern8
#define idx2_BinPattern32 idx2_BinPattern16 idx2_BinPattern16
#define idx2_BinPattern64 idx2_BinPattern32 idx2_BinPattern32
#define idx2_BinaryByte(Byte) \
  (Byte & 0x80 ? '1' : '0'),\
  (Byte & 0x40 ? '1' : '0'),\
  (Byte & 0x20 ? '1' : '0'),\
  (Byte & 0x10 ? '1' : '0'),\
  (Byte & 0x08 ? '1' : '0'),\
  (Byte & 0x04 ? '1' : '0'),\
  (Byte & 0x02 ? '1' : '0'),\
  (Byte & 0x01 ? '1' : '0')
#define idx2_BinaryByte64(Val)\
  idx2_BinaryByte((Val) >> 56), idx2_BinaryByte((Val) >> 48),\
  idx2_BinaryByte((Val) >> 40), idx2_BinaryByte((Val) >> 32), idx2_BinaryByte((Val) >> 24),\
  idx2_BinaryByte((Val) >> 16), idx2_BinaryByte((Val) >>  8), idx2_BinaryByte((Val))

namespace idx2 {
/* Count the number of times a character appears in a string. Return -1 for an
 * empty string. */
inline constexpr int
CountChar(const char* str, char c) {
  int count = 0;
  if (!(*str)) // empty string
    return -1;
  while (*str)
    count += (*str++ == c);
  return count;
}
} // namespace idx2

#undef idx2_Restrict
#if defined(__clang__) || defined(__GNUC__)
#define idx2_Restrict __restrict__
#elif defined(_MSC_VER)
#define idx2_Restrict __restrict
#endif

#undef idx2_Inline
#if defined(_MSC_VER)
#define idx2_Inline __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define idx2_Inline inline __attribute__((__always_inline__))
#endif

#define idx2_ForEach(It, Container) for (auto It = Begin(Container); It != End(Container); ++It)
#define idx2_For(Type, It, Begin, End) for (Type It = Begin; It != End; ++It)
#define idx2_InclusiveForBackward(Type, It, Begin, End) for (Type It = Begin; It >= End; --It)
#define idx2_InclusiveFor(Type, It, Begin, End) for (Type It = Begin; It <= End; ++It)
#define idx2_Range(...) idx2_MacroOverload(idx2_Range, __VA_ARGS__)
#define idx2_Range_1(Container) Begin(Container), End(Container)
#define idx2_Range_2(type, Container) Begin<type>(Container), End<type>(Container)
//#define idx2_If(Var, Expr) auto Var = Expr; if (Var)
