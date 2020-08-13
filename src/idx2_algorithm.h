/* Generic algorithms to replace <algorithm> */
#pragma once

#include "idx2_macros.h"

namespace idx2 {

idx2_T(t) t Min(const t& a, const t& b);
idx2_T(t) t Min(const t& a, const t& b, const t& c);
idx2_T(t) t Max(const t& a, const t& b);
idx2_T(t) t Max(const t& a, const t& b, const t& c);

idx2_T(i) i MaxElem(i Beg, i End);
idx2_TT(i, f) i MaxElem(i Beg, i End, const f& Comp);
idx2_T(i) struct min_max { i Min, Max; };
idx2_T(i) min_max<i> MinMaxElem(i Beg, i End);
idx2_TT(i, f) min_max<i> MinMaxElem(i Beg, i End, const f& Comp);

idx2_TT(i, t) i Find(i Beg, i End, const t& Val);
idx2_TT(i, t) i FindLast(i RevBeg, i RevEnd, const t& Val);
idx2_TT(t1, t2) bool Contains(const t1& Collection, const t2& Elem);
idx2_TT(i, f) i FindIf(i Beg, i End, const f& Pred);

idx2_T(i) void InsertionSort(i Beg, i End);

idx2_T(i) bool AreSame(i Beg1, i End1, i Beg2, i End2);

idx2_T(t) constexpr void Swap(t* A, t* B);
idx2_T(it) constexpr void IterSwap(it A, it B);

idx2_TT(i, t) void Fill(i Beg, i End, const t& Val);

/* Only work with random access iterator */
idx2_T(i) void Reverse(i Beg, i End);

idx2_T(i) int FwdDist(i Beg, i End);

} // namespace idx2

namespace idx2 {

idx2_Ti(t) t Min(const t& a, const t& b) { return b < a ? b : a; }
idx2_Ti(t) t Max(const t& a, const t& b) { return a < b ? b : a; }
idx2_Ti(t) t Min(const t& a, const t& b, const t& c) { return a < b ? Min(c, a) : Min(b, c); }
idx2_Ti(t) t Max(const t& a, const t& b, const t& c) { return a < b ? Max(b, c) : Max(a, c); }

idx2_T(i) i
MaxElem(i Beg, i End) {
  auto MaxElem = Beg;
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (*MaxElem < *Pos)
      MaxElem = Pos;
  }
  return MaxElem;
}

idx2_TT(i, f) i
MaxElem(i Beg, i End, f& Comp) {
  auto MaxElem = Beg;
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (Comp(*MaxElem, *Pos))
      MaxElem = Pos;
  }
  return MaxElem;
}

idx2_T(i) min_max<i>
MinMaxElem(i Beg, i End) {
  auto MinElem = Beg;
  auto MaxElem = Beg;
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (*Pos < *MinElem)
      MinElem = Pos;
    else if (*Pos > *MaxElem)
      MaxElem = Pos;
  }
  return min_max<i>{MinElem, MaxElem};
}

idx2_TT(i, f) min_max<i>
MinMaxElem(i Beg, i End, const f& Comp) {
  auto MinElem = Beg;
  auto MaxElem = Beg;
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (Comp(*Pos, *MinElem))
      MinElem = Pos;
    else if (Comp(*MaxElem, *Pos))
      MaxElem = Pos;
  }
  return min_max<i>{ MinElem, MaxElem };
}

idx2_TT(i, t) i
Find(i Beg, i End, const t& Val) {
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (*Pos == Val)
      return Pos;
  }
  return End;
}

idx2_TT(i, t) i
FindLast(i RevBeg, i RevEnd, const t& Val) {
  for (i Pos = RevBeg; Pos != RevEnd; --Pos) {
    if (*Pos == Val)
      return Pos;
  }
  return RevEnd;
}

idx2_TTi(t1, t2) bool
Contains(const t1& Collection, const t2& Elem) {
  return Find(Begin(Collection), End(Collection), Elem) != End(Collection);
}

idx2_TT(i, f) i
FindIf(i Beg, i End, const f& Pred) {
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (Pred(*Pos))
      return Pos;
  }
  return End;
}

/*
Return a position where the value is Val.
If there is no such position, return the first position k such that A[k] > Val */
idx2_TT(t, i) i
BinarySearch(i Beg, i End, const t& Val) {
  while (Beg < End) {
    i Mid = Beg + (End - Beg) / 2;
    if (*Mid < Val) {
      Beg = Mid + 1;
      continue;
    } else if (Val < *Mid) {
      End = Mid;
      continue;
    }
    return Mid;
  }
  return End;
}

idx2_T(i) void
InsertionSort(i Beg, i End) {
  i Last = Beg + 1;
  while (Last != End) {
    i Pos = BinarySearch(Beg, Last, *Last);
    for (i It = Last; It != Pos; --It)
      Swap(It, It - 1);
    ++Last;
  }
}

idx2_T(i) bool 
AreSame(i Beg1, i End1, i Beg2) {
  bool Same = true;
  for (i It1 = Beg1, It2 = Beg2; It1 != End1; ++It1, ++It2) {
    if (!(*It1 == *It2))
      return false;
  }
  return Same;
}

idx2_Ti(t) constexpr void
Swap(t* A, t* idx2_Restrict B) {
  t T = *A;
  *A = *B;
  *B = T;
}

idx2_Ti(it) constexpr void
IterSwap(it A, it B) {
  Swap(&(*A), &(*B));
}

idx2_TT(i, t) void
Fill(i Beg, i End, const t& Val) {
  for (i It = Beg; It != End; ++It)
    *It = Val;
}

idx2_T(i) void
Reverse(i Beg, i End) {
  auto It1 = Beg;
  auto It2 = End - 1;
  while (It1 < It2) {
    Swap(It1, It2);
    ++It1;
    --It2;
  }
}

idx2_T(i) int
FwdDist(i Beg, i End) {
  int Dist = 0;
  while (Beg != End) {
    ++Dist;
    ++Beg;
  }
  return Dist;
}

} // namespace idx2
