#pragma once

#include "mg_common.h"
#include "mg_algorithm.h"
#include "math.h"

namespace mg {

struct stat {
  f64 MinV = traits<f64>::Max;
  f64 MaxV = traits<f64>::Min;
  f64 S = 0;
  i64 N = 0;
  f64 SSq = 0;

  mg_Inline void Add(f64 Val) {
    MinV = Min(MinV, Val);
    MaxV = Max(MaxV, Val);
    S += Val;
    ++N;
    SSq += Val * Val;
  }

  mg_Inline f64 GetMin() const { return MinV; }
  mg_Inline f64 GetMax() const { return MaxV; }
  mg_Inline f64 Sum() const { return S; }
  mg_Inline i64 Count() const { return N; }
  mg_Inline f64 SumSq() const { return SSq; }
  mg_Inline f64 Avg() const { return S / N; }
  mg_Inline f64 AvgSq() const { return SSq / N; }
  mg_Inline f64 SqAvg() const { f64 A = Avg(); return A * A; }
  mg_Inline f64 Var() const { return AvgSq() - SqAvg(); }
  mg_Inline f64 StdDev() const { return sqrt(Var()); }
};

}
