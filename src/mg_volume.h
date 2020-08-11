#pragma once

#include "mg_common.h"
#include "mg_data_types.h"
#include "mg_error.h"
#include "mg_macros.h"
#include "mg_memory.h"
#include "mg_memory_map.h"

namespace mg {

enum dimension { X, Y, Z };

struct volume;

struct extent {
  u64 From = 0, Dims = 0;
  extent();
  explicit extent(const v3i& Dims3);
  explicit extent(const volume& Vol);
  extent(const v3i& From3, const v3i& Dims3);
  operator bool() const;
  static extent Invalid();
};

struct grid : public extent {
  u64 Strd = 0;
  grid();
  explicit grid(const v3i& Dims3);
  explicit grid(const volume& Vol);
  grid(const v3i& From3, const v3i& Dims3);
  grid(const v3i& From3, const v3i& Dims3, const v3i& Strd3);
  explicit grid(const extent& Ext);
  operator bool() const;
  static grid Invalid();
};

// TODO: do not allocate in the constructor, use Alloc() function
struct volume {
  buffer Buffer = {};
  u64 Dims = 0;
  dtype Type = dtype::__Invalid__;
  volume();
  volume(const buffer& Buf, const v3i& Dims3, dtype TypeIn);
  volume(const v3i& Dims3, dtype TypeIn, allocator* Alloc = &Mallocator());
  mg_T(t) volume(const t* Ptr, i64 Size);
  mg_T(t) volume(const t* Ptr, const v3i& Dims3);
  mg_T(t) explicit volume(const buffer_t<t>& Buf);
  mg_T(t) t& At(const v3i& P) const;
  mg_T(t) t& At(const v3i& From3, const v3i& Strd3, const v3i& P) const;
  mg_T(t) t& At(const extent& Ext, const v3i& P) const;
  mg_T(t) t& At(const grid& Grid, const v3i& P) const;
  mg_T(t) t& At(i64 Idx) const;
  mg_T(t) volume& operator=(t Val);
};


struct mmap_volume {
  volume Vol;
  mmap_file MMap;
};

void Unmap(mmap_volume* Vol);

/* Represent a volume storing samples of a sub-grid to a larger grid */
struct subvol_grid {
  volume Vol;
  grid Grid;
  explicit subvol_grid(const volume& VolIn);
  subvol_grid(const grid& GridIn, const volume& VolIn);
  mg_T(t) t& At(const v3i& P) const;
};

#define mg_PrStrExt "[" mg_PrStrV3i mg_PrStrV3i "]"
#define mg_PrExt(G) mg_PrV3i(From(G)), mg_PrV3i(Dims(G))
#define mg_PrStrGrid "[" mg_PrStrV3i mg_PrStrV3i mg_PrStrV3i "]"
#define mg_PrGrid(G) mg_PrV3i(From(G)), mg_PrV3i(Dims(G)), mg_PrV3i(Strd(G))

bool operator==(const extent& Ext1, const extent& Ext2);
bool operator==(const grid& Ext1, const grid& Ext2);
bool operator==(const volume& V1, const volume& V2);

v3i Dims(const v3i& First, const v3i& Last);
v3i Dims(const v3i& First, const v3i& Last, const v3i& Strd);

v3i From(const extent& Ext);
v3i To(const extent& Ext);
v3i Frst(const extent& Ext);
v3i Last(const extent& Ext);
v3i Dims(const extent& Ext);
v3i Strd(const extent& Ext);
i64 Size(const extent& Ext);
void SetFrom(extent* Ext, const v3i& From3);
void SetDims(extent* Ext, const v3i& Dims3);

v3i From(const grid& Grid);
v3i To(const grid& Grid);
v3i Frst(const grid& Grid);
v3i Last(const grid& Grid);
v3i Dims(const grid& Grid);
v3i Strd(const grid& Grid);
i64 Size(const grid& Grid);
void SetFrom(grid* Grid, const v3i& From3);
void SetDims(grid* Grid, const v3i& Dims3);
void SetStrd(grid* Grid, const v3i& Strd3);

v3i From(const volume& Vol);
v3i To(const volume& Vol);
v3i Frst(const volume& Vol);
v3i Last(const volume& Vol);
v3i Dims(const volume& Vol);
v3i Strd(const volume& Vol);
i64 Size(const volume& Vol);
void SetDims(volume* Vol, const v3i& Dims3);

i64 Row(const v3i& N, const v3i& P);
v3i InvRow(i64 I, const v3i& N);

//grid Intersect();

mg_T(t)
struct volume_iterator {
  t* Ptr = nullptr;
  v3i P = {}, N = {};
  volume_iterator& operator++();
  t& operator*();
  bool operator!=(const volume_iterator& Other) const;
  bool operator==(const volume_iterator& Other) const;
};
mg_T(t) volume_iterator<t> Begin(const volume& Vol);
mg_T(t) volume_iterator<t> End(const volume& Vol);

mg_T(t)
struct extent_iterator {
  t* Ptr = nullptr;
  v3i P = {}, D = {}, N = {};
  extent_iterator& operator++();
  t& operator*();
  bool operator!=(const extent_iterator& Other) const;
  bool operator==(const extent_iterator& Other) const;
};
mg_T(t) extent_iterator<t> Begin(const extent& Ext, const volume& Vol);
mg_T(t) extent_iterator<t> End(const extent& Ext, const volume& Vol);
// TODO: merge grid_iterator and grid_indexer?
// TODO: add extent_iterator and dimension_iterator?

mg_T(t)
struct grid_iterator {
  t* Ptr = nullptr;
  v3i P = {}, D = {}, S = {}, N = {};
  grid_iterator& operator++();
  t& operator*();
  bool operator!=(const grid_iterator& Other) const;
  bool operator==(const grid_iterator& Other) const;
};
mg_T(t) grid_iterator<t> Begin(const grid& Grid, const volume& Vol);
mg_T(t) grid_iterator<t> End(const grid& Grid, const volume& Vol);

/* Read a volume from a file. */
error<> ReadVolume(cstr FileName, const v3i& Dims3, dtype Type, volume* Vol);

/* Memory-map a volume */
error<mmap_err_code> MapVolume(cstr FileName, const v3i& Dims3, dtype DType, mmap_volume* Vol, map_mode Mode);

void Resize(volume* Vol, const v3i& Dims3, allocator* Alloc = &Mallocator());
void Resize(volume* Vol, const v3i& Dims3, dtype Type, allocator* Alloc = &Mallocator());
void Dealloc(volume* Vol);

error<> WriteVolume(FILE* Fp, const volume& Vol, const grid& Grid);
error<> WriteVolume(cstr FileName, const volume& Vol);
error<> WriteVolume(cstr FileName, const volume& Vol, const extent& Grid);

/* Copy a region of the first volume to a region of the second volume */
mg_T(t) void Copy(const t& SGrid, const volume& SVol, volume* DVol);
mg_TT(t1, t2) void Copy(const t1& SGrid, const volume& SVol, const t2& DGrid, volume* DVol);
/* Copy the part of SVol that overlaps with Grid to DVol. All grids are defined
on some "global" coordinate system. */
mg_T(t) void Copy(const t& Grid, const subvol_grid& SVol, subvol_grid* DVol);
/* Similar to copy, but add the source to the destination instead */
mg_TT(t1, t2) void Add(const t1& SGrid, const volume& SVol, const t2& DGrid, volume* DVol);
/* Returns whether Grid1 is a sub-grid of Grid2 */
mg_TT(t1, t2) bool IsSubGrid(const t1& Grid1, const t2& Grid2);
mg_TT(t1, t2) t1 SubGrid(const t1& Grid1, const t2& Grid2);
/* Compute the position of Grid1 relative to Grid2 (Grid1 is a sub-grid of Grid2) */
mg_TT(t1, t2) t1 Relative(const t1& Grid1, const t2& Grid2);
/* "Crop" Grid1 against Grid2 */
mg_TT(t1, t2) t1 Crop(const t1& Grid1, const t2& Grid2);
mg_T(t) bool IsInGrid(const t& Grid, const v3i& Point);
/* Return the bounding box of two extents */
extent BoundingBox(const extent& Ext1, const extent& Ext2);

/* Return a slab (from Grid) of size N in the direction of D. If N is positive,
take from the lower end, otherwise take from the higher end. Useful for e.g.,
taking a boundary face/column/corner of a block. */
mg_T(t) t Slab(const t& Grid, dimension D, int N);
struct grid_split { grid First, Second; };
grid_split Split(const grid& Grid, dimension D, int N);
grid_split SplitAlternate(const grid& Grid, dimension D);
mg_T(t) t Translate(const t& Grid, dimension D, int N);
mg_T(t) struct slabs1 {
  t Slabs[4];
};

mg_T(t) slabs1<t> 
TakeSlabs1(const t& Grid) {
  v3i D3 = Dims(Grid);
  mg_Assert((D3.X < D3.Y && D3.Y == D3.Z) || (D3.Y < D3.Z && D3.Z == D3.X) || (D3.Z < D3.X && D3.X == D3.Y));
  dimension D, E, F;
  int M;
  if (D3.X < D3.Y) {
    D = X; E = Y; F = Z; M = D3.X;
  } else if (D3.Y < D3.Z) {
    D = Y; E = Z; F = X; M = D3.Y;
  } else {
    D = Z; E = X; F = Y; M = D3.Z;
  }
  return slabs1<t>{ Slab(Slab(Grid, E,  M), F,  M),   // MMM
                    Slab(Slab(Grid, E, -1), F,  M),   // MM1
                    Slab(Slab(Grid, F, -1), E,  M),   // M1M
                    Slab(Slab(Grid, E, -1), F, -1) }; // M11
}

void Clone(const volume& Src, volume* Dst, allocator* Alloc = &Mallocator());

/* Return the number of dimensions, given a volume size */
int NumDims(const v3i& N);

#define mg_BeginGridLoop(G, V) // G is a grid and V is a volume
#define mg_EndGridLoop
#define mg_BeginGridLoop2(GI, VI, GJ, VJ) // loop through two grids in lockstep
#define mg_EndGridLoop2

} // namespace mg

#include "mg_assert.h"
#include "mg_bitops.h"
#include "mg_math.h"

namespace mg {

mg_Inline extent::
extent() = default;

mg_Inline extent::
extent(const v3i& Dims3)
  : From(0)
  , Dims(Pack3i64(Dims3)) {}

mg_Inline extent::
extent(const volume& Vol)
  : From(0)
  , Dims(Vol.Dims) {}

mg_Inline extent::
extent(const v3i& From3, const v3i& Dims3)
  : From(Pack3i64(From3))
  , Dims(Pack3i64(Dims3)) {}

mg_Inline extent::
operator bool() const {
  return Unpack3i64(Dims) > v3i::Zero;
}

mg_Inline extent extent::
Invalid() {
  return extent(v3i::Zero, v3i::Zero);
}

mg_Inline grid::
grid() = default;

mg_Inline grid::
grid(const v3i& Dims3)
  : extent(Dims3)
  , Strd(Pack3i64(v3i::One)) {}

mg_Inline grid::
grid(const v3i& From3, const v3i& Dims3)
  : extent(From3, Dims3)
  , Strd(Pack3i64(v3i::One)) {}

mg_Inline grid::
grid(const v3i& From3, const v3i& Dims3, const v3i& Strd3)
  : extent(From3, Dims3)
  , Strd(Pack3i64(Strd3)) {}

mg_Inline grid::
grid(const extent& Ext)
  : extent(Ext)
  , Strd(Pack3i64(v3i::One)) {}

mg_Inline grid::
grid(const volume& Vol)
  : extent(Vol)
  , Strd(Pack3i64(v3i::One)) {}

mg_Inline grid::
operator bool() const {
  return Unpack3i64(Dims) > v3i::Zero;
}

mg_Inline grid grid::
Invalid() {
  return grid(v3i::Zero, v3i::Zero, v3i::Zero);
}

mg_Inline volume::
volume() = default;

mg_Inline volume::
volume(const buffer& Buf, const v3i& Dims3, dtype TypeIn)
  : Buffer(Buf)
  , Dims(Pack3i64(Dims3))
  , Type(TypeIn) {}

mg_Inline volume::
volume(const v3i& Dims3, dtype TypeIn, allocator* Alloc)
  : Buffer()
  , Dims(Pack3i64(Dims3))
  , Type(TypeIn) { AllocBuf(&Buffer, SizeOf(TypeIn) * Prod<i64>(Dims3), Alloc); }

mg_Ti(t) volume::
volume(const t* Ptr, i64 Size)
  : volume(Ptr, v3i(Size, 1, 1)) { mg_Assert(Size <= (i64)traits<i32>::Max); }

mg_Ti(t) volume::
volume(const t* Ptr, const v3i& Dims3)
  : Buffer((byte*)const_cast<t*>(Ptr), Prod<i64>(Dims3) * sizeof(t))
  , Dims(Pack3i64(Dims3))
  , Type(dtype_traits<t>::Type) {}

mg_Ti(t) volume::
volume(const buffer_t<t>& Buf)
  : volume(Buf.Data, Buf.Size) {}

mg_T(t) volume& volume::
operator=(t Val) {
  mg_Assert(dtype_traits<t>::Type == Type);
  Fill(Begin<t>(*this), End<t>(*this), Val);
  return *this;
}

mg_Inline subvol_grid::
subvol_grid(const volume& VolIn)
  : Vol(VolIn)
  , Grid(VolIn) {}

mg_Inline subvol_grid::
subvol_grid(const grid& GridIn, const volume& VolIn)
  : Vol(VolIn)
  , Grid(GridIn) {}

mg_Ti(t) t& volume::
At(const v3i& P) const {
  v3i D3 = mg::Dims(*this);
  mg_Assert(P < D3);
  return (const_cast<t*>((const t*)Buffer.Data))[Row(D3, P)];
}

mg_Ti(t) t& volume::
At(const v3i& From3, const v3i& Strd3, const v3i& P) const {
  return At<t>(From3 + P * Strd3);
}

mg_Ti(t) t& volume::
At(const extent& Ext, const v3i& P) const {
  return At<t>(From(Ext) + P);
}

mg_Ti(t) t& volume::
At(const grid& Grid, const v3i& P) const {
  return At<t>(From(Grid) + P * Strd(Grid));
}

mg_Ti(t) t& volume::
At(i64 Idx) const {
  // TODO: add a bound check in here
  return (const_cast<t*>((const t*)Buffer.Data))[Idx];
}

mg_Inline bool
operator==(const extent& E1, const extent& E2) {
  return E1.Dims == E2.Dims && E1.From == E2.From;
}

mg_Inline bool
operator==(const grid& G1, const grid& G2) {
  return G1.Dims == G2.Dims && G1.From == G2.From && G1.Strd == G2.Strd;
}

mg_Inline bool
operator==(const volume& V1, const volume& V2) {
  return V1.Buffer == V2.Buffer && V1.Dims == V2.Dims && V1.Type == V2.Type;
}

mg_Inline v3i Dims(const v3i& Frst, const v3i& Last) { return Last - Frst + 1; }
mg_Inline v3i Dims(const v3i& Frst, const v3i& Last, const v3i& Strd) { return (Last - Frst) / Strd + 1; }

mg_Inline v3i  From(const extent& Ext) { return Unpack3i64(Ext.From); }
mg_Inline v3i  To(const extent& Ext) { return From(Ext) + Dims(Ext); }
mg_Inline v3i  Frst(const extent& Ext) { return From(Ext); }
mg_Inline v3i  Last(const extent& Ext) { return To(Ext) - 1; }
mg_Inline v3i  Dims(const extent& Ext) { return Unpack3i64(Ext.Dims); }
mg_Inline v3i  Strd(const extent& Ext) { (void)Ext; return v3i::One; }
mg_Inline i64  Size(const extent& Ext) { return Prod<i64>(Dims(Ext)); }
mg_Inline void SetFrom(extent* Ext, const v3i& From3) { Ext->From = Pack3i64(From3); }
mg_Inline void SetDims(extent* Ext, const v3i& Dims3) { Ext->Dims = Pack3i64(Dims3); }

mg_Inline v3i  From(const grid& Grid) { return Unpack3i64(Grid.From); }
mg_Inline v3i  To(const grid& Grid) { return From(Grid) + Dims(Grid) * Strd(Grid); }
mg_Inline v3i  Frst(const grid& Grid) { return From(Grid); }
mg_Inline v3i  Last(const grid& Grid) { return To(Grid) - Strd(Grid); }
mg_Inline v3i  Dims(const grid& Grid) { return Unpack3i64(Grid.Dims); }
mg_Inline v3i  Strd(const grid& Grid) { return Unpack3i64(Grid.Strd); }
mg_Inline i64  Size(const grid& Grid) { return Prod<i64>(Dims(Grid)); };
mg_Inline void SetFrom(grid* Grid, const v3i& From3) { Grid->From = Pack3i64(From3); }
mg_Inline void SetDims(grid* Grid, const v3i& Dims3) { Grid->Dims = Pack3i64(Dims3); }
mg_Inline void SetStrd(grid* Grid, const v3i& Strd3) { Grid->Strd = Pack3i64(Strd3); }

mg_Inline v3i  From(const volume& Vol) { (void)Vol; return v3i::Zero; }
mg_Inline v3i  To(const volume& Vol) { return Dims(Vol); }
mg_Inline v3i  Frst(const volume& Vol) { return From(Vol); }
mg_Inline v3i  Last(const volume& Vol) { return Dims(Vol) - 1; }
mg_Inline v3i  Dims(const volume& Vol) { return Unpack3i64(Vol.Dims); }
mg_Inline i64  Size(const volume& Vol) { return Prod<i64>(Dims(Vol)); }
mg_Inline void SetDims(volume* Vol, const v3i& Dims3) { Vol->Dims = Pack3i64(Dims3); }

mg_Ti(t) volume_iterator<t>
Begin(const volume& Vol) {
  volume_iterator<t> Iter;
  Iter.P = v3i::Zero; Iter.N = Dims(Vol);
  Iter.Ptr = (t*)const_cast<byte*>(Vol.Buffer.Data);
  return Iter;
}

mg_Ti(t) volume_iterator<t>
End(const volume& Vol) {
  volume_iterator<t> Iter;
  v3i To3(0, 0, Dims(Vol).Z);
  Iter.Ptr = (t*)const_cast<byte*>(Vol.Buffer.Data) + Row(Dims(Vol), To3);
  return Iter;
}

mg_Ti(t) volume_iterator<t>& volume_iterator<t>::
operator++() {
  ++Ptr;
  if (++P.X >= N.X) {
    P.X = 0;
    if (++P.Y >= N.Y) {
      P.Y = 0;
      ++P.Z;
    }
  }
  return *this;
}

mg_Ti(t) t& volume_iterator<t>::
operator*() { return *Ptr; }

mg_Ti(t) bool volume_iterator<t>::
operator!=(const volume_iterator<t>& Other) const { return Ptr != Other.Ptr; }

mg_Ti(t) bool volume_iterator<t>::
operator==(const volume_iterator<t>& Other) const { return Ptr == Other.Ptr; }

mg_Ti(t) extent_iterator<t>
Begin(const extent& Ext, const volume& Vol) {
  extent_iterator<t> Iter;
  Iter.D = Dims(Ext); Iter.P = v3i(0); Iter.N = Dims(Vol);
  Iter.Ptr = (t*)const_cast<byte*>(Vol.Buffer.Data) + Row(Iter.N, From(Ext));
  return Iter;
}

mg_Ti(t) extent_iterator<t>
End(const extent& Ext, const volume& Vol) {
  extent_iterator<t> Iter;
  v3i To3 = From(Ext) + v3i(0, 0, Dims(Ext).Z);
  Iter.Ptr = (t*)const_cast<byte*>(Vol.Buffer.Data) + Row(Dims(Vol), To3);
  return Iter;
}

mg_Ti(t) extent_iterator<t>& extent_iterator<t>::
operator++() {
  ++P.X;
  ++Ptr;
  if (P.X >= D.X) {
    P.X = 0;
    ++P.Y;
    Ptr = Ptr - D.X + N.X;
    if (P.Y >= D.Y) {
      P.Y = 0;
      ++P.Z;
      Ptr = Ptr - D.Y * N.X + N.X * N.Y;
    }
  }
  return *this;
}

mg_Ti(t) t& extent_iterator<t>::
operator*() { return *Ptr; }

mg_Ti(t) bool extent_iterator<t>::
operator!=(const extent_iterator<t>& Other) const { return Ptr != Other.Ptr; }

mg_Ti(t) bool extent_iterator<t>::
operator==(const extent_iterator<t>& Other) const { return Ptr == Other.Ptr; }

mg_T(t) grid_iterator<t>
Begin(const grid& Grid, const volume& Vol) {
  grid_iterator<t> Iter;
  Iter.S = Strd(Grid); Iter.D = Dims(Grid) * Iter.S; Iter.P = v3i(0); Iter.N = Dims(Vol);
  Iter.Ptr = (t*)const_cast<byte*>(Vol.Buffer.Data) + Row(Iter.N, From(Grid));
  return Iter;
}

mg_T(t) grid_iterator<t>
End(const grid& Grid, const volume& Vol) {
  grid_iterator<t> Iter;
  v3i To3 = From(Grid) + v3i(0, 0, Dims(Grid).Z * Strd(Grid).Z);
  Iter.Ptr = (t*)const_cast<byte*>(Vol.Buffer.Data) + Row(Dims(Vol), To3);
  return Iter;
}

mg_T(t) grid_iterator<t>& grid_iterator<t>::
operator++() {
  P.X += S.X;
  Ptr += S.X;
  if (P.X >= D.X) {
    P.X = 0;
    P.Y += S.Y;
    Ptr = Ptr - D.X + (N.X * S.Y);
    if (P.Y >= D.Y) {
      P.Y = 0;
      P.Z += S.Z;
      Ptr = Ptr - D.Y * i64(N.X) + S.Z * i64(N.X) * N.Y;
    }
  }
  return *this;
}

mg_Ti(t) t& grid_iterator<t>::
operator*() { return *Ptr; }

mg_Ti(t) bool grid_iterator<t>::
operator!=(const grid_iterator<t>& Other) const { return Ptr != Other.Ptr; }

mg_Ti(t) bool grid_iterator<t>::
operator==(const grid_iterator<t>& Other) const { return Ptr == Other.Ptr; }

// TODO: change this function name to something more descriptive
mg_Inline i64
Row(const v3i& N, const v3i& P) { return i64(P.Z) * N.X * N.Y + i64(P.Y) * N.X + P.X; }

mg_Inline v3i
InvRow(i64 I, const v3i& N) {
  i32 Z = i32(I / (N.X * N.Y));
  i32 XY = i32(I % (N.X * N.Y));
  return v3i(XY % N.X, XY / N.X, Z);
}

mg_Inline int
NumDims(const v3i& N) { return (N.X > 1) + (N.Y > 1) + (N.Z > 1); }

#undef mg_BeginGridLoop2
#define mg_BeginGridLoop2(GI, VI, GJ, VJ) /* GridI, VolumeI, GridJ, VolumeJ */\
  {\
    mg_Assert(Dims(GI) == Dims(GJ));\
    v3i Pos;\
    v3i FromI = From(GI), FromJ = From(GJ);\
    v3i Dims3 = Dims(GI), DimsI = Dims(VI), DimsJ = Dims(VJ);\
    v3i StrdI = Strd(GI), StrdJ = Strd(GJ);\
    mg_BeginFor3(Pos, v3i::Zero, Dims3, v3i::One) {\
      i64 I = Row(DimsI, FromI + Pos * StrdI);\
      i64 J = Row(DimsJ, FromJ + Pos * StrdJ);\

#undef mg_EndGridLoop2
#define mg_EndGridLoop2 }}}}

#undef mg_BeginGridLoop
#define mg_BeginGridLoop(G, V)\
  {\
    v3i Pos;\
    v3i From3 = From(G), Dims3 = Dims(G), Strd3 = Strd(G);\
    v3i DimsB = Dims(V);\
    mg_BeginFor3(Pos, From3, Dims3, Strd3) {\
      i64 I = Row(DimsB, Pos);\

#undef mg_EndGridLoop
#define mg_EndGridLoop }}}}

mg_T(t) void
Copy(const grid& SGrid, const volume& SVol, volume* DVol) {
  mg_Assert(Dims(SGrid) <= Dims(*DVol));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  mg_Assert(SVol.Type == DVol->Type);
  auto SIt = Begin<t>(SGrid, SVol), SEnd = End<t>(SGrid, SVol);
  auto DIt = Begin<t>(SGrid, *DVol);
  for (; SIt != SEnd; ++SIt, ++DIt)
    *DIt = *SIt;
}

mg_TT(stype, dtype) void
Copy(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  mg_Assert(Dims(SGrid) == Dims(DGrid));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(Dims(DGrid) <= Dims(*DVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  auto SIt = Begin<stype>(SGrid, SVol), SEnd = End<stype>(SGrid, SVol);
  auto DIt = Begin<dtype>(DGrid, *DVol);
  for (; SIt != SEnd; ++SIt, ++DIt)
    *DIt = *SIt;
}

//i64 CopyGridGridCountZeroes(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol);

mg_TT(stype, dtype) v2d
CopyExtentExtentMinMax(const extent& SGrid, const volume& SVol, const extent& DGrid, volume* DVol) {
  v2d MinMax = v2d(traits<f64>::Max, traits<f64>::Min);
  mg_Assert(Dims(SGrid) == Dims(DGrid));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(Dims(DGrid) <= Dims(*DVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  v3i SrcFrom3 = From(SGrid), SrcTo3 = To(SGrid);
  v3i DstFrom3 = From(DGrid), DstTo3 = To(DGrid);
  v3i SrcDims3 = Dims(SVol);
  v3i DstDims3 = Dims(*DVol);
  const stype* mg_Restrict SrcPtr = (const stype*)SVol.Buffer.Data;
  dtype* mg_Restrict DstPtr = (dtype*)DVol->Buffer.Data;
  v3i S3, D3;
  mg_BeginFor3Lockstep(S3, SrcFrom3, SrcTo3, v3i::One, D3, DstFrom3, DstTo3, v3i::One) {
    f64 V = (f64)SrcPtr[Row(SrcDims3, S3)];
    DstPtr[Row(DstDims3, D3)] = (dtype)V;
    MinMax.Min = Min(MinMax.Min, V);
    MinMax.Max = Max(MinMax.Min, V);
  } mg_EndFor3
  return MinMax;
}

mg_TT(stype, dtype) void
CopyExtentGrid(const extent& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  mg_Assert(Dims(SGrid) == Dims(DGrid));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(Dims(DGrid) <= Dims(*DVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  v3i SrcFrom3 = From(SGrid), SrcTo3 = To(SGrid);
  v3i DstFrom3 = From(DGrid), DstTo3 = To(DGrid), DstStrd3 = Strd(DGrid);
  v3i SrcDims3 = Dims(SVol);
  v3i DstDims3 = Dims(*DVol);
  const stype* mg_Restrict SrcPtr = (const stype*)SVol.Buffer.Data;
  dtype* mg_Restrict DstPtr = (dtype*)DVol->Buffer.Data;
  v3i S3, D3;
  mg_BeginFor3Lockstep(S3, SrcFrom3, SrcTo3, v3i::One, D3, DstFrom3, DstTo3, DstStrd3) {
    DstPtr[Row(DstDims3, D3)] = (dtype)SrcPtr[Row(SrcDims3, S3)];
  } mg_EndFor3
}

mg_TT(stype, dtype) void
CopyGridExtent(const grid& SGrid, const volume& SVol, const extent& DGrid, volume* DVol) {
  mg_Assert(Dims(SGrid) == Dims(DGrid));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(Dims(DGrid) <= Dims(*DVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  v3i SrcFrom3 = From(SGrid), SrcTo3 = To(SGrid), SrcStrd3 = Strd(SGrid);
  v3i DstFrom3 = From(DGrid), DstTo3 = To(DGrid);
  v3i SrcDims3 = Dims(SVol);
  v3i DstDims3 = Dims(*DVol);
  const stype* mg_Restrict SrcPtr = (const stype*)SVol.Buffer.Data;
  dtype* mg_Restrict DstPtr = (dtype*)DVol->Buffer.Data;
  v3i S3, D3;
  mg_BeginFor3Lockstep(S3, SrcFrom3, SrcTo3, SrcStrd3, D3, DstFrom3, DstTo3, v3i::One) {
    DstPtr[Row(DstDims3, D3)] = (dtype)SrcPtr[Row(SrcDims3, S3)];
  } mg_EndFor3
}

mg_TT(stype, dtype) void
CopyGridGrid(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  mg_Assert(Dims(SGrid) == Dims(DGrid));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(Dims(DGrid) <= Dims(*DVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  v3i SrcFrom3 = From(SGrid), SrcTo3 = To(SGrid), SrcStrd3 = Strd(SGrid);
  v3i DstFrom3 = From(DGrid), DstTo3 = To(DGrid), DstStrd3 = Strd(DGrid);
  v3i SrcDims3 = Dims(SVol);
  v3i DstDims3 = Dims(*DVol);
  const stype* mg_Restrict SrcPtr = (const stype*)SVol.Buffer.Data;
  dtype* mg_Restrict DstPtr = (dtype*)DVol->Buffer.Data;
  v3i S3, D3;
  mg_BeginFor3Lockstep(S3, SrcFrom3, SrcTo3, SrcStrd3, D3, DstFrom3, DstTo3, DstStrd3) {
    DstPtr[Row(DstDims3, D3)] = (dtype)SrcPtr[Row(SrcDims3, S3)];
  } mg_EndFor3
}

mg_TT(stype, dtype) void
CopyExtentExtent(const extent& SGrid, const volume& SVol, const extent& DGrid, volume* DVol) {
  mg_Assert(Dims(SGrid) == Dims(DGrid));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(Dims(DGrid) <= Dims(*DVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  v3i SrcFrom3 = From(SGrid), SrcTo3 = To(SGrid);
  v3i DstFrom3 = From(DGrid), DstTo3 = To(DGrid);
  v3i SrcDims3 = Dims(SVol);
  v3i DstDims3 = Dims(*DVol);
  const stype* mg_Restrict SrcPtr = (const stype*)SVol.Buffer.Data;
  dtype* mg_Restrict DstPtr = (dtype*)DVol->Buffer.Data;
  v3i S3, D3;
  mg_BeginFor3Lockstep(S3, SrcFrom3, SrcTo3, v3i::One, D3, DstFrom3, DstTo3, v3i::One) {
    DstPtr[Row(DstDims3, D3)] = (dtype)SrcPtr[Row(SrcDims3, S3)];
  } mg_EndFor3
}

mg_TT(stype, dtype) i64
CopyGridGridCountZeroes(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  i64 Count = 0;
  mg_Assert(Dims(SGrid) == Dims(DGrid));
  mg_Assert(Dims(SGrid) <= Dims(SVol));
  mg_Assert(Dims(DGrid) <= Dims(*DVol));
  mg_Assert(DVol->Buffer && SVol.Buffer);
  v3i SrcFrom3 = From(SGrid), SrcTo3 = To(SGrid), SrcStrd3 = Strd(SGrid);
  v3i DstFrom3 = From(DGrid), DstTo3 = To(DGrid), DstStrd3 = Strd(DGrid);
  v3i SrcDims3 = Dims(SVol);
  v3i DstDims3 = Dims(*DVol);
  const stype* mg_Restrict SrcPtr = (const stype*)SVol.Buffer.Data;
  dtype* mg_Restrict DstPtr = (dtype*)DVol->Buffer.Data;
  v3i S3, D3;
  mg_BeginFor3Lockstep(S3, SrcFrom3, SrcTo3, SrcStrd3, D3, DstFrom3, DstTo3, DstStrd3) {
    DstPtr[Row(DstDims3, D3)] = (dtype)SrcPtr[Row(SrcDims3, S3)];
    Count += (dtype)SrcPtr[Row(SrcDims3, S3)] == 0;
  } mg_EndFor3
  return Count;
}


mg_T(t) void
Copy(const t& Grid, const subvol_grid& SVol, subvol_grid* DVol) {
  mg_Assert(Strd(SVol.Grid) % Strd(Grid) == 0);
  mg_Assert(Strd(DVol->Grid) % Strd(Grid) == 0);
  t Crop1 = Crop(Grid, SVol.Grid);
  if (Crop1) {
    t Crop2 = Crop(Crop1, DVol->Grid);
    if (Crop2) {
      grid SGrid = Relative(Crop2, SVol.Grid);
      grid DGrid = Relative(Crop2, DVol->Grid);
      Copy(SGrid, SVol.Vol, DGrid, &DVol->Vol);
    }
  }
}

mg_TT(t1, t2) void
Add(const t1& SGrid, const volume& SVol, const t2& DGrid, volume* DVol) {
#define Body(type)\
  mg_Assert(Dims(SGrid) == Dims(DGrid));\
  mg_Assert(Dims(SGrid) <= Dims(SVol));\
  mg_Assert(Dims(DGrid) <= Dims(*DVol));\
  mg_Assert(DVol->Buffer && SVol.Buffer);\
  mg_Assert(SVol.Type == DVol->Type);\
  auto SIt = Begin<type>(SGrid, SVol), SEnd = End<type>(SGrid, SVol);\
  auto DIt = Begin<type>(DGrid, *DVol);\
  for (; SIt != SEnd; ++SIt, ++DIt)\
    *DIt += *SIt;

  mg_DispatchOnType(SVol.Type);
#undef Body
}

// TODO: what is this
mg_TT(t1, t2) void
Add2(const t1& SGrid, const volume& SVol, const t2& DGrid, volume* DVol) {
  mg_Assert(Dims(SGrid) == Dims(DGrid));\
  mg_Assert(Dims(SGrid) <= Dims(SVol));\
  mg_Assert(Dims(DGrid) <= Dims(*DVol));\
  mg_Assert(DVol->Buffer && SVol.Buffer);\
  mg_Assert(SVol.Type == DVol->Type);\
  auto SBeg = Begin<f64>(SGrid, SVol);
  auto SIt = Begin<f64>(SGrid, SVol), SEnd = End<f64>(SGrid, SVol);\
  auto DIt = Begin<f64>(DGrid, *DVol), DEnd = End<f64>(DGrid, *DVol);\
  for (; SIt != SEnd; ++SIt, ++DIt)\
    *DIt += *SIt;
}

mg_TT(t1, t2) bool
IsSubGrid(const t1& Grid1, const t2& Grid2) {
  if (!(From(Grid1) >= From(Grid2)))
    return false;
  if (!(Last(Grid1) <= Last(Grid2)))
    return false;
  if ((Strd(Grid1) % Strd(Grid2)) != 0)
    return false;
  if ((From(Grid1) - From(Grid2)) % Strd(Grid2) != 0)
    return false;
  return true;
}

mg_TT(t1, t2) t1
SubGrid(const t1& Grid1, const t2& Grid2) {
  return t1(From(Grid1) + Strd(Grid1) * From(Grid2), Dims(Grid2), Strd(Grid1) * Strd(Grid2));
}

mg_TT(t1, t2) t1
Relative(const t1& Grid1, const t2& Grid2) {
  //printf("");
  mg_Assert(IsSubGrid(Grid1, Grid2));
  v3i From3 = (From(Grid1) - From(Grid2)) / Strd(Grid2);
  return grid(From3, Dims(Grid1), Strd(Grid1) / Strd(Grid2));
}

mg_TT(t1, t2) t1
Crop(const t1& Grid1, const t2& Grid2) {
  v3i Strd3 = Strd(Grid1);
  v3i Grid1Frst3 = Frst(Grid1);
  v3i Frst3 = Max(Grid1Frst3, Frst(Grid2));
  v3i Last3 = Min(Last(Grid1), Last(Grid2));
  Frst3 = ((Frst3 - Grid1Frst3 + Strd3 - 1) / Strd3) * Strd3 + Grid1Frst3;
  Last3 = ((Last3 - Grid1Frst3) / Strd3) * Strd3 + Grid1Frst3;
  t1 OutGrid = Grid1;
  SetFrom(&OutGrid, Frst3);
  SetDims(&OutGrid, Frst3 <= Last3 ? (Last3 - Frst3) / Strd3 + 1 : v3i::Zero);
  return OutGrid;
}

mg_Ti(t) bool
IsInGrid(const t& Grid, const v3i& Point) {
  return (Point - From(Grid)) % Strd(Grid) == v3i::Zero;
}

// TODO: this can be turned into a slice function ala Python[start:stop]
mg_T(t) t
Slab(const t& Grid, dimension D, int N) {
  v3i Dims3 = Dims(Grid);
  mg_Assert(abs(N) <= Dims3[D] && N != 0);
  t Slab = Grid;
  if (N < 0) {
    v3i From3 = From(Grid);
    v3i Strd3 = Strd(Grid);
    From3[D] += (Dims3[D] + N) * Strd3[D];
    SetFrom(&Slab, From3);
  }
  Dims3[D] = abs(N);
  SetDims(&Slab, Dims3);
  return Slab;
}

mg_T(t) t
Translate(const t& Grid, dimension D, int N) {
  v3i From3 = From(Grid);
  From3[D] += N;
  t Slab = Grid;
  SetFrom(&Slab, From3);
  return Slab;
}

mg_Inline bool
Contain(const extent& Ext, const v3i& P) {
  return From(Ext) <= P && P < To(Ext);
}

} // namespace mg
