#pragma once

#include "idx2_common.h"
#include "idx2_macros.h"
#include "idx2_memory.h"

namespace idx2 {

idx2_T(t)
struct list_node {
  t Payload;
  list_node* Next = nullptr;
};

idx2_T(t)
struct list {
  list_node<t>* Head = nullptr;
  allocator* Alloc = nullptr;
  i64 Size = 0;
  list(allocator* Alloc = &Mallocator());
};

#define idx2_Li list_iterator<t>

idx2_T(t)
struct list_iterator {
  list_node<t>* Node = nullptr;
  list_iterator& operator++();
  list_node<t>* operator->() const;
  t& operator*() const;
  bool operator!=(const list_iterator& Other);
  bool operator==(const list_iterator& Other);
};

idx2_T(t) idx2_Li Begin(const list<t>& List);
idx2_T(t) idx2_Li End  (const list<t>& List);
idx2_T(t) idx2_Li Insert(list<t>* List, const idx2_Li& Where, const t& Payload);
idx2_T(t) idx2_Li PushBack(list<t>* List, const t& Payload);
idx2_T(t) void Dealloc(list<t>* List);
idx2_T(t) i64 Size(const list<t>& List);

} // namespace idx2

#include "idx2_assert.h"

namespace idx2 {

idx2_Ti(t) list<t>::
list(allocator* Alloc) : Alloc(Alloc) {}

idx2_T(t) idx2_Li
Insert(list<t>* List, const list_iterator<t>& Where, const t& Payload) {
  buffer Buf;
  List->Alloc->Alloc(&Buf, sizeof(list_node<t>));
  list_node<t>* NewNode = (list_node<t>*)Buf.Data;
  NewNode->Payload = Payload;
  NewNode->Next = nullptr;
  if (Where.Node) {
    NewNode->Next = Where->Next;
    Where->Next = NewNode;
  }
  ++List->Size;
  return list_iterator<t>{NewNode};
}

idx2_T(t) idx2_Li
PushBack(list<t>* List, const t& Payload) {
  auto Node = List->Head;
  list_node<t>* Prev = nullptr;
  while (Node) {
    Prev = Node;
    Node = Node->Next;
  }
  auto NewNode = Insert(List, list_iterator<t>{Prev}, Payload);
  if (!Prev) // this new node is the first node in the list
    List->Head = NewNode.Node;
  return NewNode;
}

idx2_T(t) void
Dealloc(list<t>* List) {
  auto Node = List->Head;
  while (Node) {
    buffer Buf((byte*)Node, sizeof(list_node<t>), List->Alloc);
    Node = Node->Next;
    List->Alloc->Dealloc(&Buf);
  }
  List->Head = nullptr;
  List->Size = 0;
}

idx2_Ti(t) i64
Size(const list<t>& List) { return List.Size; }

idx2_Ti(t) list_iterator<t>&
list_iterator<t>::operator++() {
  idx2_Assert(Node);
  Node = Node->Next;
  return *this;
}

idx2_Ti(t) list_node<t>*
list_iterator<t>::operator->() const {
  idx2_Assert(Node);
  return const_cast<list_node<t>*>(Node);
}

idx2_Ti(t) t& idx2_Li::
operator*() const {
  idx2_Assert(Node);
  return const_cast<t&>(Node->Payload);
}

idx2_Ti(t) bool idx2_Li::
operator!=(const list_iterator<t>& Other) { return Node != Other.Node; }

idx2_Ti(t) bool idx2_Li::
operator==(const list_iterator<t>& Other) { return Node == Other.Node; }

idx2_Ti(t) idx2_Li
Begin(const list<t>& List) { return list_iterator<t>{List.Head}; }

idx2_Ti(t) idx2_Li
End(const list<t>& List) { (void)List; return list_iterator<t>(); }

#undef idx2_Li

} // namespace idx2


