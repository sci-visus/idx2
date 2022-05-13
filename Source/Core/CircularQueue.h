#pragma once

#include "Common.h"

namespace idx2 {

idx2_TI(t, N)
struct circular_queue {
  static_assert((N & (N - 1)) == 0);
  stack_array<t, N> Buffer;
  i16 Start = 0;
  i16 End = 0; // exclusive
  t& operator[](i16 Idx) {
    idx2_Assert(Idx < Size(*this));
    return Buffer[(Start + Idx) & (N - 1)];
  }
};

idx2_TI(t, N) i16
Size(const circular_queue<t, N>& Queue) {
  return (Queue.Start <= Queue.End) ? (Queue.End - Queue.Start) : (N - (Queue.Start - Queue.End));
}

idx2_TI(t, N) bool
IsFull(const circular_queue<t, N>& Queue) {
  return Size(Queue) == N - 1;
}

/* Push a value to the back of the queue.
 * Return the absolute array index of that element, so that it can be later accessed directly. */
idx2_TI(t, N) i16
PushBack(circular_queue<t, N>* Queue, const t& Val) {
  idx2_Assert(!IsFull(*Queue) || false);
  Queue->Buffer[Queue->End++] = Val;
  i16 Pos = Queue->End;
  Queue->End &= N - 1;
  return Pos;
}

/* Pop values from the front of the queue.
 * Return the first value popped. */
idx2_TI(t, N) t
PopFront(circular_queue<t, N>* Queue, i16 Count = 1) {
  idx2_Assert(Count <= Size(*Queue));
  i16 Pos = Queue->Start;
  Queue->Start += Count;
  Queue->Start &= N - 1;
  return Queue->Buffer[Pos];
}

/* Pop values from the back of the queue.
 * Return the first value popped. */
idx2_TI(t, N) t
PopBack(circular_queue<t, N>* Queue, i16 Count = 1) {
  idx2_Assert(Count <= Size(*Queue));
  i16 Pos = Queue->Start;
  Queue->End -= Count;
  Queue->End &= N - 1;
  return Queue->Buffer[Pos];
}

idx2_TI(t, N) void
Clear(circular_queue<t, N>* Queue) {
  Queue->Start = Queue->End = 0;
}

// Commented out since we can easily traverse a circular queue using indices from 0 to Size(Queue) and the [] operator
///* Traverse a circular queue:
// * Pos  = name of the current index
// * Body = the code to apply during traversal */
//#undef idx2_TraverseCircularQueue
//#define idx2_TraverseCircularQueue(Queue, Pos, Body) \
//  i16 Pos = Queue.Start;                             \
//  while (Pos != Queue.End) {                         \
//    { Body; }                                        \
//    Pos = (Pos + 1) & (N - 1);                       \
//  }

} // namespace idx2
