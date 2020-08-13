#pragma once

// TODO: think about thread safety
// TODO: (double-ended) StackAllocator
// TODO: aligned allocation
// TODO: pool allocator
// TODO: add asserts

#include <stdlib.h>
#include "mg_common.h"
#include "mg_macros.h"

namespace idx2 {

/* General purpose buffer for string-related operations */
inline thread_local char ScratchBuf[1024];

#define mg_RAII(...) mg_MacroOverload(mg_RAII, __VA_ARGS__)
#define mg_RAII_3(Type, Var, Init) Type Var; mg_CleanUp_1(Dealloc(&Var)); Init;
#define mg_RAII_4(Type, Var, Init, Clean) Type Var; mg_CleanUp_1(Clean); Init;

/*
Quickly declare a heap-allocated array (typed_buffer) which deallocates itself
at the end of scope. */
#define mg_MallocArray (Name, Type, Size)
#define mg_CallocArray(Name, Type, Size) // initialize elements to 0
#define mg_ArrayOfMallocArrays(Name, Type, SizeOuter, SizeInner)
#define mg_MallocArrayOfArrays(Name, Type, SizeOuter, SizeInner)

struct buffer;

struct allocator {
  virtual bool Alloc(buffer* Buf, i64 Bytes) = 0;
  virtual void Dealloc(buffer* Buf) = 0;
  virtual void DeallocAll() = 0;
  virtual ~allocator() {}
};

/* Allocators that know if they own an address/buffer */
struct owning_allocator : allocator {
  virtual bool Own(const buffer& Buf) const = 0;
};

struct mallocator;
/* A simple allocator that allocates simply by bumping a counter */
struct linear_allocator;
/* A linear allocator that uses stack storage. */
template <int Capacity>
struct stack_linear_allocator;
/*
Whenever an allocation of a size in a specific range is made, return the block
immediately from the head of a linked list. Otherwise forward the allocation to
some Parent allocator. */
struct free_list_allocator;
/* Try to allocate using one allocator first (the Primary), then if that fails,
 * use another allocator (the Secondary). */
struct fallback_allocator;

struct mallocator : public allocator {
  bool Alloc(buffer* Buf, i64 Bytes) override;
  void Dealloc(buffer* Buf) override;
  void DeallocAll() override;
};

static inline mallocator& Mallocator() {
  static mallocator Instance;
  return Instance;
}

mg_T(t) struct buffer_t;

struct buffer {
  byte* Data = nullptr;
  i64 Bytes = 0;
  allocator* Alloc = nullptr;
  buffer(allocator* AllocIn = &Mallocator());
  template <typename t, int N> buffer(t (&Arr)[N]);
  buffer(const byte* DataIn, i64 BytesIn, allocator* AllocIn = nullptr);
  mg_T(t) buffer(const buffer_t<t>& Buf);
  byte& operator[](i64 Idx) const;
  explicit operator bool() const;
  bool operator!=(const buffer& Other) const;
};

struct linear_allocator : public owning_allocator {
  buffer Block;
  i64 CurrentBytes = 0;
  linear_allocator();
  linear_allocator(const buffer& Buf);
  bool Alloc(buffer* Buf, i64 Bytes) override;
  void Dealloc(buffer* Buf) override;
  void DeallocAll() override;
  bool Own(const buffer& Buf) const override;
};

template <int Capacity>
struct stack_linear_allocator : public linear_allocator {
  stack_array<byte, Capacity> Storage;
  stack_linear_allocator() : linear_allocator(buffer{Storage.Arr, Capacity}) {}
};

struct free_list_allocator : public allocator {
  struct node { node* Next = nullptr; };
  node* Head = nullptr;
  i64 MinBytes = 0;
  i64 MaxBytes = 0;
  allocator* Parent = nullptr;
  free_list_allocator();
  free_list_allocator(i64 MinBytesIn, i64 MaxBytesIn, allocator* ParentIn = &Mallocator());
  free_list_allocator(i64 Bytes, allocator* ParentIn = &Mallocator());
  bool Alloc(buffer* Buf, i64 Bytes) override;
  void Dealloc(buffer* Buf) override;
  void DeallocAll() override;
};

struct fallback_allocator : public allocator {
  owning_allocator* Primary = nullptr;
  allocator* Secondary = nullptr;
  fallback_allocator();
  fallback_allocator(owning_allocator* PrimaryIn, allocator* SecondaryIn);
  bool Alloc(buffer* Buf, i64 Bytes) override;
  void Dealloc(buffer* Buf) override;
  void DeallocAll() override;
};

void Clone(const buffer& Src, buffer* Dst, allocator* Alloc = &Mallocator());

/* Abstract away memory allocations/deallocations */
mg_T(t) void AllocPtr  (t* Ptr, i64 Size, allocator* Alloc = &Mallocator());
mg_T(t) void CallocPtr (t* Ptr, i64 Size, allocator* Alloc = &Mallocator());
mg_T(t) void DeallocPtr(t* Ptr);

void AllocBuf  (buffer* Buf, i64 Bytes, allocator* Alloc = &Mallocator());
void CallocBuf (buffer* Buf, i64 Bytes, allocator* Alloc = &Mallocator());
void DeallocBuf(buffer* Buf);

mg_T(t) void AllocBufT  (buffer_t<t>* Buf, i64 Size, allocator* Alloc = &Mallocator());
mg_T(t) void CallocBufT (buffer_t<t>* Buf, i64 Size, allocator* Alloc = &Mallocator());
mg_T(t) void DeallocBufT(buffer_t<t>* Buf);

/* Zero out a buffer */
// TODO: replace with the call to Fill, or Memset
void ZeroBuf(buffer* Buf);
mg_T(t) void ZeroBufT(buffer_t<t>* Buf);

/* Copy one buffer to another. Here the order of arguments are the reverse of memcpy. */
i64 MemCopy(const buffer& Src, buffer* Dst);
i64 MemCopy(const buffer& Src, buffer* Dst, u64 Bytes);

i64 Size(const buffer& Buf);
void Resize(buffer* Buf, i64 Size, allocator* Alloc = &Mallocator());

bool operator==(const buffer& Buf1, const buffer& Buf2);
buffer operator+(const buffer& Buf, i64 Bytes);

mg_T(t)
struct buffer_t {
  t* Data = nullptr;
  i64 Size = 0;
  allocator* Alloc = nullptr;
  buffer_t();
  template <int N>
  buffer_t(t (&Arr)[N]);
  buffer_t(const t* DataIn, i64 SizeIn, allocator* AllocIn = nullptr);
  buffer_t(const buffer& Buf);
  t& operator[](i64 Idx) const;
  explicit operator bool() const;
};

mg_T(t) i64 Size(const buffer_t<t>& Buf);
mg_T(t) i64 Bytes(const buffer_t<t>& Buf);

} // namespace idx2

#include "mg_scopeguard.h"

namespace idx2 {

mg_T(t) void
AllocBufT(buffer_t<t>* Buf, i64 Size, allocator* Alloc) {
  buffer RawBuf;
  AllocBuf(&RawBuf, i64(Size * sizeof(t)), Alloc);
  Buf->Data = (t*)RawBuf.Data;
  Buf->Size = Size;
  Buf->Alloc = Alloc;
}

mg_T(t) void
CallocBufT(buffer_t<t>* Buf, i64 Size, allocator* Alloc) {
  buffer RawBuf;
  CallocBuf(&RawBuf, i64(Size * sizeof(t)), Alloc);
  Buf->Data = (t*)RawBuf.Data;
  Buf->Size = Size;
  Buf->Alloc = Alloc;
}

mg_T(t) void
DeallocBufT(buffer_t<t>* Buf) {
  buffer RawBuf{(byte*)Buf->Data, i64(Buf->Size * sizeof(t)), Buf->Alloc};
  DeallocBuf(&RawBuf);
  Buf->Data  = nullptr;
  Buf->Size  = 0;
  Buf->Alloc = nullptr;
}

mg_T(t) void
AllocPtr(t** Ptr, i64 Size, allocator* Alloc = &Mallocator()) {
  buffer RawBuf;
  AllocBuf(&RawBuf, i64(Size * sizeof(t)), Alloc);
  *Ptr = (t*)RawBuf.Data;
}

mg_T(t) void
CallocPtr(t** Ptr, i64 Size, allocator* Alloc = &Mallocator()) {
  buffer RawBuf;
  CallocBuf(&RawBuf, i64(Size * sizeof(t)), Alloc);
  *Ptr = (t*)RawBuf.Data;
}

mg_T(t) void
DeallocPtr(t** Ptr, allocator* Alloc = &Mallocator()) {
  buffer RawBuf{(byte*)(*Ptr), 1, Alloc};
  DeallocBuf(&RawBuf);
}

mg_Inline buffer::
buffer(allocator* AllocIn)
  : Alloc(AllocIn) {}

mg_Inline buffer::
buffer(const byte* DataIn, i64 BytesIn, allocator* AllocIn)
  : Data(const_cast<byte*>(DataIn)), Bytes(BytesIn), Alloc(AllocIn) {}

mg_TIi(t, N) buffer::
buffer(t (&Arr)[N])
  : Data((byte*)const_cast<t*>(&Arr[0])), Bytes(sizeof(Arr)) {}

mg_Ti(t) buffer::
buffer(const buffer_t<t>& Buf)
  : Data((byte*)const_cast<t*>(Buf.Data))
  , Bytes(Buf.Size * sizeof(t)), Alloc(Buf.Alloc) {}

mg_Inline byte& buffer::
operator[](i64 Idx) const {
  assert(Idx < Bytes);
  return const_cast<byte&>(Data[Idx]);
}

mg_Inline bool buffer::
operator!=(const buffer& Other) const {
  return Data != Other.Data || Bytes != Other.Bytes;
}

mg_Inline buffer::
operator bool() const { return this->Data && this->Bytes; }

mg_Inline bool
operator==(const buffer& Buf1, const buffer& Buf2) {
  return Buf1.Data == Buf2.Data && Buf1.Bytes == Buf2.Bytes;
}

mg_Inline i64
Size(const buffer& Buf) { return Buf.Bytes; }

mg_Inline void
Resize(buffer* Buf, i64 NewSize, allocator* Alloc) {
  if (Size(*Buf) < NewSize) {
    if (Size(*Buf) > 0)
      DeallocBuf(Buf);
    AllocBuf(Buf, NewSize, Alloc);
  }
}

/* typed_buffer stuffs */
mg_Ti(t) buffer_t<t>::
buffer_t() = default;

mg_T(t) template <int N> mg_Inline buffer_t<t>::
buffer_t(t (&Arr)[N])
  : Data(&Arr[0]), Size(N) {}

mg_Ti(t) buffer_t<t>::
buffer_t(const t* DataIn, i64 SizeIn, allocator* AllocIn)
  : Data(const_cast<t*>(DataIn)), Size(SizeIn), Alloc(AllocIn) {}

mg_Ti(t) buffer_t<t>::
buffer_t(const buffer& Buf)
  : Data((t*)const_cast<byte*>(Buf.Data))
  , Size(Buf.Bytes / sizeof(t)), Alloc(Buf.Alloc) {}

mg_Ti(t) t& buffer_t<t>::
operator[](i64 Idx) const {
  assert(Idx < Size);
  return const_cast<t&>(Data[Idx]);
}

mg_Ti(t) i64
Size(const buffer_t<t>& Buf) { return Buf.Size; }

mg_Ti(t) i64
Bytes(const buffer_t<t>& Buf) { return Buf.Size * sizeof(t); }

mg_Ti(t) buffer_t<t>::
operator bool() const { return Data && Size; }

} // namespace idx2

#undef mg_MallocArray
#define mg_MallocArray(Name, Type, Size)\
  using namespace idx2;\
  buffer_t<Type> Name;\
  AllocBufT(&Name, (Size));\
  mg_CleanUp(__LINE__, DeallocBufT(&Name))

#undef mg_CallocArray
#define mg_CallocArray(Name, Type, Size)\
  using namespace idx2;\
  buffer_t<Type> Name;\
  CallocBufT(&Name, (Size));\
  mg_CleanUp(__LINE__, DeallocBufT(&Name))

#undef mg_ArrayOfMallocArrays
#define mg_ArrayOfMallocArrays(Name, Type, SizeOuter, SizeInner)\
  using namespace idx2;\
  buffer_t<Type> Name[SizeOuter] = {}; \
  for (int I = 0; I < (SizeOuter); ++I)\
    AllocBufT(&Name[I], (SizeInner));\
  mg_CleanUp(__LINE__, {\
    for (int I = 0; I < (SizeOuter); ++I) DeallocBufT(&Name[I]); })

#undef mg_MallocArrayOfArrays
#define mg_MallocArrayOfArrays(Name, Type, SizeOuter, SizeInner)\
  using namespace idx2;\
  buffer_t<buffer_t<Type>> Name;\
  AllocBufT(&Name, (SizeOuter));\
  for (int I = 0; I < (SizeOuter); ++I) \
    AllocBufT(&Name[I], (SizeInner));\
  mg_CleanUp(__LINE__, {\
    for (int I = 0; I < (SizeOuter); ++I) DeallocBufT(&Name[I]);\
    DeallocBufT(&Name);\
  })
