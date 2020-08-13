/* Generic algorithms to replace <algorithm> */
#pragma once

#include "mg_macros.h"

namespace idx2 {

mg_T(t) t Min(const t& a, const t& b);
mg_T(t) t Min(const t& a, const t& b, const t& c);
mg_T(t) t Max(const t& a, const t& b);
mg_T(t) t Max(const t& a, const t& b, const t& c);

mg_T(i) i MaxElem(i Beg, i End);
mg_TT(i, f) i MaxElem(i Beg, i End, const f& Comp);
mg_T(i) struct min_max { i Min, Max; };
mg_T(i) min_max<i> MinMaxElem(i Beg, i End);
mg_TT(i, f) min_max<i> MinMaxElem(i Beg, i End, const f& Comp);

mg_TT(i, t) i Find(i Beg, i End, const t& Val);
mg_TT(i, t) i FindLast(i RevBeg, i RevEnd, const t& Val);
mg_TT(t1, t2) bool Contains(const t1& Collection, const t2& Elem);
mg_TT(i, f) i FindIf(i Beg, i End, const f& Pred);

mg_T(i) void InsertionSort(i Beg, i End);

mg_T(i) bool AreSame(i Beg1, i End1, i Beg2, i End2);

mg_T(t) constexpr void Swap(t* A, t* B);
mg_T(it) constexpr void IterSwap(it A, it B);

mg_TT(i, t) void Fill(i Beg, i End, const t& Val);

/* Only work with random access iterator */
mg_T(i) void Reverse(i Beg, i End);

mg_T(i) int FwdDist(i Beg, i End);

} // namespace idx2

namespace idx2 {

mg_Ti(t) t Min(const t& a, const t& b) { return b < a ? b : a; }
mg_Ti(t) t Max(const t& a, const t& b) { return a < b ? b : a; }
mg_Ti(t) t Min(const t& a, const t& b, const t& c) { return a < b ? Min(c, a) : Min(b, c); }
mg_Ti(t) t Max(const t& a, const t& b, const t& c) { return a < b ? Max(b, c) : Max(a, c); }

mg_T(i) i
MaxElem(i Beg, i End) {
  auto MaxElem = Beg;
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (*MaxElem < *Pos)
      MaxElem = Pos;
  }
  return MaxElem;
}

mg_TT(i, f) i
MaxElem(i Beg, i End, f& Comp) {
  auto MaxElem = Beg;
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (Comp(*MaxElem, *Pos))
      MaxElem = Pos;
  }
  return MaxElem;
}

mg_T(i) min_max<i>
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

mg_TT(i, f) min_max<i>
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

mg_TT(i, t) i
Find(i Beg, i End, const t& Val) {
  for (i Pos = Beg; Pos != End; ++Pos) {
    if (*Pos == Val)
      return Pos;
  }
  return End;
}

mg_TT(i, t) i
FindLast(i RevBeg, i RevEnd, const t& Val) {
  for (i Pos = RevBeg; Pos != RevEnd; --Pos) {
    if (*Pos == Val)
      return Pos;
  }
  return RevEnd;
}

mg_TTi(t1, t2) bool
Contains(const t1& Collection, const t2& Elem) {
  return Find(Begin(Collection), End(Collection), Elem) != End(Collection);
}

mg_TT(i, f) i
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
mg_TT(t, i) i
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

mg_T(i) void
InsertionSort(i Beg, i End) {
  i Last = Beg + 1;
  while (Last != End) {
    i Pos = BinarySearch(Beg, Last, *Last);
    for (i It = Last; It != Pos; --It)
      Swap(It, It - 1);
    ++Last;
  }
}

mg_T(i) bool 
AreSame(i Beg1, i End1, i Beg2) {
  bool Same = true;
  for (i It1 = Beg1, It2 = Beg2; It1 != End1; ++It1, ++It2) {
    if (!(*It1 == *It2))
      return false;
  }
  return Same;
}

mg_Ti(t) constexpr void
Swap(t* A, t* mg_Restrict B) {
  t T = *A;
  *A = *B;
  *B = T;
}

mg_Ti(it) constexpr void
IterSwap(it A, it B) {
  Swap(&(*A), &(*B));
}

mg_TT(i, t) void
Fill(i Beg, i End, const t& Val) {
  for (i It = Beg; It != End; ++It)
    *It = Val;
}

mg_T(i) void
Reverse(i Beg, i End) {
  auto It1 = Beg;
  auto It2 = End - 1;
  while (It1 < It2) {
    Swap(It1, It2);
    ++It1;
    --It2;
  }
}

mg_T(i) int
FwdDist(i Beg, i End) {
  int Dist = 0;
  while (Beg != End) {
    ++Dist;
    ++Beg;
  }
  return Dist;
}

} // namespace idx2
