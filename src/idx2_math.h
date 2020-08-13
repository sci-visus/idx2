#pragma once

#include "idx2_common.h"
#include "idx2_macros.h"

namespace idx2 {

constexpr f64 Pi = 3.14159265358979323846;

/* Generate a power table for a particular base and type */
template <int N>
int (&Power(int Base))[N];

bool IsEven(i64 X);
bool IsOdd (i64 X);
v3i  IsEven(const v3i& P);
v3i  IsOdd (const v3i& P);
idx2_T(t) bool IsBetween(t Val, t A, t B);

bool IsPrime(i64 X);
i64 NextPrime(i64 X);

idx2_T(t) int Exponent(t Val);

idx2_TT(u, t = u) t Prod(const v2<u>& Vec);
idx2_TT(u, t = u) t Prod(const v3<u>& Vec);
idx2_TT(u, t = u) t Sum(const v2<u>& Vec);
idx2_TT(u, t = u) t Sum(const v3<u>& Vec);

/* 2D vector math */
idx2_T(t) v2<t> operator+(const v2<t>& Lhs, const v2<t>& Rhs);
idx2_T(t) v2<t> operator-(const v2<t>& Lhs, const v2<t>& Rhs);
idx2_T(t) v2<t> operator/(const v2<t>& Lhs, const v2<t>& Rhs);
idx2_T(t) v2<t> operator*(const v2<t>& Lhs, const v2<t>& Rhs);
idx2_T(t) v2<t> operator+(const v2<t>& Lhs, t Val);
idx2_T(t) v2<t> operator-(const v2<t>& Lhs, t Val);
idx2_T(t) v2<t> operator/(const v2<t>& Lhs, t Rhs);
idx2_T(t) v2<t> operator*(const v2<t>& Lhs, t Val);
idx2_T(t) bool operator>(const v2<t>& Lhs, t Val);
idx2_T(t) bool operator<(const v2<t>& Lhs, t Val);
idx2_T(t) bool operator==(const v2<t>& Lhs, const v2<t>& Rhs);
idx2_T(t) bool operator!=(const v2<t>& Lhs, const v2<t>& Rhs);
idx2_T(t) bool operator<(t Val, const v2<t>& Rhs);
idx2_T(t) bool operator<=(t Val, const v2<t>& Rhs);
idx2_T(t) bool operator<=(const v2<t>& Lhs, t Val);

idx2_T(t) v2<t> Min(const v2<t>& Lhs, const v2<t>& Rhs);
idx2_T(t) v2<t> Max(const v2<t>& Lhs, const v2<t>& Rhs);

// TODO: generalize the t parameter to u for the RHS
/* 3D vector math */
idx2_T(t) v3<t> operator+(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) v3<t> operator-(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) v3<t> operator*(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) v3<t> operator/(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) v3<t> operator&(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) v3<t> operator%(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) v3<t> operator+(const v3<t>& Lhs, t Val);
idx2_T(t) v3<t> operator-(const v3<t>& Lhs, t Val);
idx2_T(t) v3<t> operator-(t Val, const v3<t>& Lhs);
idx2_T(t) v3<t> operator*(const v3<t>& Lhs, t Val);
idx2_T(t) v3<t> operator/(const v3<t>& Lhs, t Val);
idx2_T(t) v3<t> operator&(const v3<t>& Lhs, t Val);
idx2_T(t) v3<t> operator%(const v3<t>& Lhs, t Val);
idx2_T(t) bool  operator==(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) bool  operator!=(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) bool  operator<=(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) bool  operator< (const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) bool  operator> (const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) bool  operator> (const v3<t>& Lhs, t Val);
idx2_T(t) bool  operator>=(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) bool  operator==(const v3<t>& Lhs, t Val);
idx2_T(t) bool  operator!=(const v3<t>& Lhs, t Val);
idx2_T(t) bool  operator<(t Val, const v3<t>& Rhs);
idx2_T(t) bool  operator<(const v3<t>& Lhs, t Val);
idx2_T(t) bool  operator<=(t Val, const v3<t>& Rhs);
idx2_T(t) bool  operator<=(const v3<t>& Lhs, t Val);
idx2_TT(t, u) v3<t> operator>>(const v3<t>& Lhs, const v3<u>& Rhs);
idx2_TT(t, u) v3<t> operator>>(const v3<t>& Lhs, u Val);
idx2_TT(t, u) v3<u> operator>>(u Val, const v3<t>& Rhs);
idx2_TT(t, u) v3<t> operator<<(const v3<t>& Lhs, const v3<u>& Rhs);
idx2_TT(t, u) v3<t> operator<<(const v3<t>& Lhs, u Val);
idx2_TT(t, u) v3<u> operator<<(u Val, const v3<t>& Rhs);

idx2_T(t) v3<t> Min(const v3<t>& Lhs, const v3<t>& Rhs);
idx2_T(t) v3<t> Max(const v3<t>& Lhs, const v3<t>& Rhs);

i8 Log2Floor(i64 Val);
i8 Log2Ceil(i64 Val);
i8 Log8Floor(i64 Val);

idx2_T(t) v3<t> Ceil(const v3<t>& Vec);
idx2_T(t) v2<t> Ceil(const v2<t>& Vec);
i64 RoundDown(i64 Val, i64 M); // Round Val down to a multiple of M
i64 RoundUp(i64 Val, i64 M); // Round Val up to a multiple of M

i64 Pow(i64 Base, int Exp);
i64 NextPow2(i64 Val);

int GeometricSum(int Base, int N);

idx2_T(t) t Lerp(t V0, t V1, f64 T);
// bilinear interpolation
idx2_T(t) t BiLerp(t V00, t V10, t V01, t V11, const v2d& T);
// trilinear interpolation
idx2_T(t) t TriLerp(
  t V000, t V100, t V010, t V110, t V001,
  t V101, t V011, t V111, const v3d& T);

} // namespace idx2

#include <math.h>
#include "idx2_algorithm.h"
#include "idx2_assert.h"
#include "idx2_bitops.h"
#include "idx2_common.h"

namespace idx2 {

idx2_Inline bool
IsEven(i64 X) { return (X & 1) == 0; }
idx2_Inline bool
IsOdd(i64 X) { return (X & 1) != 0; }
idx2_Inline v3i
IsEven(const v3i& P) { return v3i(IsEven(P.X), IsEven(P.Y), IsEven(P.Z)); }
idx2_Inline v3i
IsOdd(const v3i& P) { return v3i(IsOdd(P.X), IsOdd(P.Y), IsOdd(P.Z)); }
idx2_Inline int
Max(const v3i& P) { return Max(Max(P.X, P.Y), P.Z); }
idx2_Inline v3i
Abs(const v3i& P) { return v3i(abs(P.X), abs(P.Y), abs(P.Z)); }
idx2_Inline v3i
Equals(const v3i& P, const v3i& Q) { return v3i(P.X == Q.X, P.Y == Q.Y, P.Z == Q.Z); }
idx2_Inline v3i
NotEquals(const v3i& P, const v3i& Q) { return v3i(P.X != Q.X, P.Y != Q.Y, P.Z != Q.Z); }

idx2_Ti(t) bool
IsBetween(t Val, t A, t B) {
  return (A <= Val && Val <= B) || (B <= Val && Val <= A);
}

idx2_Inline bool
IsPow2(int X) {
  idx2_Assert(X > 0);
  return X && !(X & (X - 1));
}

idx2_Inline bool
IsPrime(i64 X) {
  i64 S = (i64)sqrt((f64)X);
  for (i64 I = 2; I <= S; ++I) {
    if (X % I == 0)
      return false;
  }
  return true;
}

idx2_Inline i64
NextPrime(i64 X) {
  while (IsPrime(X)) { ++X; }
  return X;
}

idx2_Inline constexpr int
LogFloor(i64 Base, i64 Val) {
  int Log = 0;
  i64 S = Base;
  while (S <= Val) {
    ++Log;
    S *= Base;
  }
  return Log;
}

idx2_TI(t, N)
struct power {
  static inline const stack_array<t, LogFloor(N, traits<t>::Max)> Table = []() {
    stack_array<t, LogFloor(N, traits<t>::Max)> Result;
    t Base = N;
    t Pow = 1;
    for (int I = 0; I < Size(Result); ++I) {
      Result[I] = Pow;
      Pow *= Base;
    }
    return Result;
  }();
  t operator[](int I) const { return Table[I]; }
};


/*
For double-precision, the returned exponent is between [-1023, 1024] (-1023 if 0, -1022 if denormal, the bias is 1023)
For single-precision, the returned exponent is between [-127, 128] (-127 if 0, -126 if denormal, the bias is 127) */
idx2_Ti(t) int
Exponent(t Val) {
  if (Val != 0) {
    int E;
    frexp(Val, &E);
    /* clamp exponent in case Val is denormal */
    return Max(E, 1 - traits<t>::ExpBias);
  }
  return -traits<t>::ExpBias;
}

idx2_Ti(t) v2<t>
operator+(const v2<t>& Lhs, const v2<t>& Rhs) {
  return v2<t>(Lhs.X + Rhs.X, Lhs.Y + Rhs.Y);
}
idx2_Ti(t) v2<t> operator+(const v2<t>& Lhs, t Val) {
  return v2<t>(Lhs.X + Val, Lhs.Y + Val);
}
idx2_Ti(t) v2<t>
operator-(const v2<t>& Lhs, const v2<t>& Rhs) {
  return v2<t>(Lhs.X - Rhs.X, Lhs.Y - Rhs.Y);
}
idx2_Ti(t) v2<t> operator-(const v2<t>& Lhs, t Val) {
  return v2<t>(Lhs.X - Val, Lhs.Y - Val);
}
idx2_Ti(t) v2<t>
operator*(const v2<t>& Lhs, const v2<t>& Rhs) {
  return v2<t>(Lhs.X * Rhs.X, Lhs.Y * Rhs.Y);
}
idx2_Ti(t) v2<t> operator*(const v2<t>& Lhs, t Val) {
  return v2<t>(Lhs.X * Val, Lhs.Y * Val);
}
idx2_Ti(t) v2<t>
operator/(const v2<t>& Lhs, const v2<t>& Rhs) {
  return v2<t>(Lhs.X / Rhs.X, Lhs.Y / Rhs.Y);
}
idx2_Ti(t) v2<t> operator/(const v2<t>& Lhs, t Val) {
  return v2<t>(Lhs.X / Val, Lhs.Y / Val);
}
idx2_Ti(t) bool
operator==(const v2<t>& Lhs, const v2<t>& Rhs) {
  return Lhs.X == Rhs.X && Lhs.Y == Rhs.Y;
}
idx2_Ti(t) bool
operator!=(const v2<t>& Lhs, const v2<t>& Rhs) {
  return Lhs.X != Rhs.X || Lhs.Y != Rhs.Y;
}
idx2_T(t) bool operator>(const v2<t>& Lhs, t Val) {
  return Lhs.X > Val && Lhs.Y > Val;
}
idx2_T(t) bool operator<(t Val, const v2<t>& Rhs) {
  return Val < Rhs.X && Val < Rhs.Y;
}
idx2_T(t) bool operator<(const v2<t>& Lhs, t Val) {
  return Lhs.X < Val && Lhs.Y < Val;
}
idx2_T(t) bool operator<=(t Val, const v2<t>& Rhs) {
  return Val <= Rhs.X && Val <= Rhs.Y;
}
idx2_T(t) bool operator<=(const v2<t>& Lhs, t Val) {
  return Lhs.X <= Val && Lhs.Y <= Val;
}
idx2_Ti(t) v2<t>
Min(const v2<t>& Lhs, const v2<t>& Rhs) {
  return v2<t>(Min(Lhs.X, Rhs.X), Min(Lhs.Y, Rhs.Y));
}
idx2_Ti(t) v2<t>
Max(const v2<t>& Lhs, const v2<t>& Rhs) {
  return v2<t>(Max(Lhs.X, Rhs.X), Max(Lhs.Y, Rhs.Y));
}

idx2_TTi(u, t) t
Prod(const v2<u>& Vec) { return t(Vec.X) * t(Vec.Y); }

idx2_TTi(u, t) t
Prod(const v3<u>& Vec) { return t(Vec.X) * t(Vec.Y) * t(Vec.Z); }

idx2_TTi(u, t) t
Sum(const v2<u>& Vec) { return t(Vec.X) + t(Vec.Y); }

idx2_TTi(u, t) t
Sum(const v3<u>& Vec) { return t(Vec.X) + t(Vec.Y) + t(Vec.Z); }

idx2_Ti(t) v3<t>
operator+(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Lhs.X + Rhs.X, Lhs.Y + Rhs.Y, Lhs.Z + Rhs.Z);
}
idx2_Ti(t) v3<t>
operator+(const v3<t>& Lhs, t Val) {
  return v3<t>(Lhs.X + Val, Lhs.Y + Val, Lhs.Z + Val);
}
idx2_Ti(t) v3<t>
operator-(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Lhs.X - Rhs.X, Lhs.Y - Rhs.Y, Lhs.Z - Rhs.Z);
}
idx2_Ti(t) v3<t>
operator-(const v3<t>& Lhs, t Val) {
  return v3<t>(Lhs.X - Val, Lhs.Y - Val, Lhs.Z - Val);
}
idx2_Ti(t) v3<t>
operator-(t Val, const v3<t>& Lhs) {
  return v3<t>(Val - Lhs.X, Val - Lhs.Y, Val - Lhs.Z);
}
idx2_Ti(t) v3<t>
operator*(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Lhs.X * Rhs.X, Lhs.Y * Rhs.Y, Lhs.Z * Rhs.Z);
}
idx2_Ti(t) v3<t>
operator*(const v3<t>& Lhs, t Val) {
  return v3<t>(Lhs.X * Val, Lhs.Y * Val, Lhs.Z * Val);
}
idx2_Ti(t) v3<t>
operator/(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Lhs.X / Rhs.X, Lhs.Y / Rhs.Y, Lhs.Z / Rhs.Z);
}
idx2_Ti(t) v3<t>
operator/(const v3<t>& Lhs, t Val) {
  return v3<t>(Lhs.X / Val, Lhs.Y / Val, Lhs.Z / Val);
}
idx2_Ti(t) v3<t>
operator&(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Lhs.X & Rhs.X, Lhs.Y & Rhs.Y, Lhs.Z & Rhs.Z);
}
idx2_Ti(t) v3<t>
operator&(const v3<t>& Lhs, t Val) {
  return v3<t>(Lhs.X & Val, Lhs.Y & Val, Lhs.Z & Val);
}
idx2_Ti(t) v3<t>
operator%(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Lhs.X % Rhs.X, Lhs.Y % Rhs.Y, Lhs.Z % Rhs.Z);
}
idx2_Ti(t) v3<t>
operator%(const v3<t>& Lhs, t Val) {
  return v3<t>(Lhs.X % Val, Lhs.Y % Val, Lhs.Z % Val);
}
idx2_Ti(t) bool
operator==(const v3<t>& Lhs, const v3<t>& Rhs) {
  return Lhs.X == Rhs.X && Lhs.Y == Rhs.Y && Lhs.Z == Rhs.Z;
}
idx2_Ti(t) bool
operator!=(const v3<t>& Lhs, const v3<t>& Rhs) {
  return Lhs.X != Rhs.X || Lhs.Y != Rhs.Y || Lhs.Z != Rhs.Z;
}
idx2_Ti(t) bool
operator==(const v3<t>& Lhs, t Val) {
  return Lhs.X == Val && Lhs.Y == Val && Lhs.Z == Val;
}
idx2_Ti(t) bool
operator!=(const v3<t>& Lhs, t Val) {
  return Lhs.X != Val || Lhs.Y != Val || Lhs.Z != Val;
}
idx2_Ti(t) bool
operator<=(const v3<t>& Lhs, const v3<t>& Rhs) {
  return Lhs.X <= Rhs.X && Lhs.Y <= Rhs.Y && Lhs.Z <= Rhs.Z;
}
idx2_Ti(t) bool
operator<(const v3<t>& Lhs, const v3<t>& Rhs) {
  return Lhs.X < Rhs.X && Lhs.Y < Rhs.Y && Lhs.Z < Rhs.Z;
}
idx2_Ti(t) bool
operator>(const v3<t>& Lhs, const v3<t>& Rhs) {
  return Lhs.X > Rhs.X && Lhs.Y > Rhs.Y && Lhs.Z > Rhs.Z;
}
idx2_Ti(t) bool
operator>(const v3<t>& Lhs, t Val) {
  return Lhs.X > Val && Lhs.Y > Val && Lhs.Z > Val;
}
idx2_Ti(t) bool
operator>=(const v3<t>& Lhs, const v3<t>& Rhs) {
  return Lhs.X >= Rhs.X && Lhs.Y >= Rhs.Y && Lhs.Z >= Rhs.Z;
}
idx2_T(t) bool  operator<(t Val, const v3<t>& Rhs) {
  return Val < Rhs.X && Val < Rhs.Y && Val < Rhs.Z;
}
idx2_T(t) bool operator<(const v3<t>& Lhs, t Val) {
  return Lhs.X < Val && Lhs.Y < Val && Lhs.Z < Val;
}
idx2_T(t) bool operator<=(const v3<t>& Lhs, t Val) {
  return Lhs.X <= Val && Lhs.Y <= Val && Lhs.Z <= Val;
}
idx2_T(t) bool operator<=(t Val, const v3<t>& Rhs) {
  return Val <= Rhs.X && Val <= Rhs.Y && Val <= Rhs.Z;
}
idx2_TTi(t, u) v3<t>
operator>>(const v3<t>& Lhs, const v3<u>& Rhs) {
  return v3<t>(Lhs.X >> Rhs.X, Lhs.Y >> Rhs.Y, Lhs.Z >> Rhs.Z);
}
idx2_TTi(t, u) v3<u>
operator>>(u Val, const v3<t>& Lhs) {
  return v3<u>(Val >> Lhs.X, Val >> Lhs.Y, Val >> Lhs.Z);
}
idx2_TTi(t, u) v3<t>
operator>>(const v3<t>& Lhs, u Val) {
  return v3<t>(Lhs.X >> Val, Lhs.Y >> Val, Lhs.Z >> Val);
}
idx2_TTi(t, u) v3<t>
operator<<(const v3<t>& Lhs, const v3<u>& Rhs) {
  return v3<t>(Lhs.X << Rhs.X, Lhs.Y << Rhs.Y, Lhs.Z << Rhs.Z);
}
idx2_TTi(t, u) v3<t>
operator<<(const v3<t>& Lhs, u Val) {
  return v3<t>(Lhs.X << Val, Lhs.Y << Val, Lhs.Z << Val);
}
idx2_TTi(t, u) v3<u>
operator<<(u Val, const v3<t>& Lhs) {
  return v3<u>(Val << Lhs.X, Val << Lhs.Y, Val << Lhs.Z);
}
idx2_Ti(t) v3<t>
Min(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Min(Lhs.X, Rhs.X), Min(Lhs.Y, Rhs.Y), Min(Lhs.Z, Rhs.Z));
}
idx2_Ti(t) v3<t>
Max(const v3<t>& Lhs, const v3<t>& Rhs) {
  return v3<t>(Max(Lhs.X, Rhs.X), Max(Lhs.Y, Rhs.Y), Max(Lhs.Z, Rhs.Z));
}

idx2_Ti(t) v3<t>
Ceil(const v3<t>& Vec) {
  return v3<t>(ceil(Vec.X), ceil(Vec.Y), ceil(Vec.Z));
}

idx2_Ti(t) v2<t>
Ceil(const v2<t>& Vec) {
  return v2<t>(ceil(Vec.X), ceil(Vec.Y));
}

idx2_Inline i64
RoundDown(i64 Val, i64 M) {
  return (Val / M) * M;
}

idx2_Inline i64
RoundUp(i64 Val, i64 M) {
  return ((Val + M - 1) / M) * M;
}

idx2_Inline i8
Log2Floor(i64 Val) {
  idx2_Assert(Val > 0);
  return Msb((u64)Val);
}

idx2_Inline i8
Log2Ceil(i64 Val) {
  idx2_Assert(Val > 0);
  auto I = Msb((u64)Val);
  return I + i8(Val != (1ll << I));
}

idx2_Inline i8
Log8Floor(i64 Val) {
  idx2_Assert(Val > 0);
  return Log2Floor(Val) / 3;
}

idx2_Inline int
GeometricSum(int Base, int N) {
  idx2_Assert(N >= 0);
  return int((Pow(Base, N + 1) - 1) / (Base - 1));
}

// TODO: when n is already a power of two plus one, do not increase n
idx2_Inline i64
NextPow2(i64 Val) {
  idx2_Assert(Val >= 0);
  if (Val == 0)
    return 1;
  return 1ll << (Msb((u64)(Val - 1)) + 1);
}

idx2_Inline i64
Pow(i64 Base, int Exp) {
  idx2_Assert(Exp >= 0);
  i64 Result = 1;
  for (int I = 0; I < Exp; ++I)
    Result *= Base;
  return Result;
}

idx2_Inline v3i
Pow(const v3i& Base, int Exp) {
  idx2_Assert(Exp >= 0);
  v3i Result(1);
  for (int I = 0; I < Exp; ++I)
    Result = Result * Base;
  return Result;
}

idx2_Ti(t) t
Lerp(t V0, t V1, f64 T) {
  idx2_Assert(0 <= T && T <= 1);
  return V0 + (V1 - V0) * T;
}

idx2_Ti(t) t
BiLerp(t V00, t V10, t V01, t V11, const v2d& T) {
  idx2_Assert(0.0 <= T && T <= 1.0);
  t V0 = Lerp(V00.X, V10.X, T.X);
  t V1 = Lerp(V01.X, V11.X, T.X);
  return Lerp(V0, V1, T.Y);
}

idx2_T(t) t TriLerp(
  t V000, t V100, t V010, t V110,
  t V001, t V101, t V011, t V111, const v3d& T)
{
  idx2_Assert(0.0 <= T && T <= 1.0);
  t V0 = BiLerp(V000, V100, V010, V110, T.XY);
  t V1 = BiLerp(V001, V101, V011, V111, T.XY);
  return Lerp(V0, V1, T.Z);
}

} // namespace idx2
