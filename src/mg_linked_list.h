#pragma once

#include "mg_common.h"
#include "mg_macros.h"
#include "mg_memory.h"

namespace idx2 {

mg_T(t)
struct list_node {
  t Payload;
  list_node* Next = nullptr;
};

mg_T(t)
struct list {
  list_node<t>* Head = nullptr;
  allocator* Alloc = nullptr;
  i64 Size = 0;
  list(allocator* Alloc = &Mallocator());
};

#define mg_Li list_iterator<t>

mg_T(t)
struct list_iterator {
  list_node<t>* Node = nullptr;
  list_iterator& operator++();
  list_node<t>* operator->() const;
  t& operator*() const;
  bool operator!=(const list_iterator& Other);
  bool operator==(const list_iterator& Other);
};

mg_T(t) mg_Li Begin(const list<t>& List);
mg_T(t) mg_Li End  (const list<t>& List);
mg_T(t) mg_Li Insert(list<t>* List, const mg_Li& Where, const t& Payload);
mg_T(t) mg_Li PushBack(list<t>* List, const t& Payload);
mg_T(t) void Dealloc(list<t>* List);
mg_T(t) i64 Size(const list<t>& List);

} // namespace idx2

#include "mg_assert.h"

namespace idx2 {

mg_Ti(t) list<t>::
list(allocator* Alloc) : Alloc(Alloc) {}

mg_T(t) mg_Li
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

mg_T(t) mg_Li
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

mg_T(t) void
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

mg_Ti(t) i64
Size(const list<t>& List) { return List.Size; }

mg_Ti(t) list_iterator<t>&
list_iterator<t>::operator++() {
  mg_Assert(Node);
  Node = Node->Next;
  return *this;
}

mg_Ti(t) list_node<t>*
list_iterator<t>::operator->() const {
  mg_Assert(Node);
  return const_cast<list_node<t>*>(Node);
}

mg_Ti(t) t& mg_Li::
operator*() const {
  mg_Assert(Node);
  return const_cast<t&>(Node->Payload);
}

mg_Ti(t) bool mg_Li::
operator!=(const list_iterator<t>& Other) { return Node != Other.Node; }

mg_Ti(t) bool mg_Li::
operator==(const list_iterator<t>& Other) { return Node == Other.Node; }

mg_Ti(t) mg_Li
Begin(const list<t>& List) { return list_iterator<t>{List.Head}; }

mg_Ti(t) mg_Li
End(const list<t>& List) { (void)List; return list_iterator<t>(); }

#undef mg_Li

} // namespace idx2


