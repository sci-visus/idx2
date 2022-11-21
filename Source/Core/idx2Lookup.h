#pragma once

#include "Common.h"
#include "idx2Common.h"


/* ---------------------- MACROS ----------------------*/

#if !defined(idx2_FileTraverse)
#define idx2_FileTraverse(                                                                         \
  Body, StackSize, FileOrderIn, FileFrom3In, FileDims3In, ExtentInFiles, Extent2)                  \
  {                                                                                                \
    file_traverse FileStack[StackSize];                                                            \
    int FileTopIdx = 0;                                                                            \
    v3i FileDims3Ext(                                                                              \
      (int)NextPow2(FileDims3In.X), (int)NextPow2(FileDims3In.Y), (int)NextPow2(FileDims3In.Z));   \
    FileStack[FileTopIdx] =                                                                        \
      file_traverse{ FileOrderIn, FileOrderIn, FileFrom3In, FileFrom3In + FileDims3Ext, u64(0) };  \
    while (FileTopIdx >= 0)                                                                        \
    {                                                                                              \
      file_traverse& FileTop = FileStack[FileTopIdx];                                              \
      int FD = FileTop.FileOrder & 0x3;                                                            \
      FileTop.FileOrder >>= 2;                                                                     \
      if (FD == 3)                                                                                 \
      {                                                                                            \
        if (FileTop.FileOrder == 3)                                                                \
          FileTop.FileOrder = FileTop.PrevOrder;                                                   \
        else                                                                                       \
          FileTop.PrevOrder = FileTop.FileOrder;                                                   \
        continue;                                                                                  \
      }                                                                                            \
      --FileTopIdx;                                                                                \
      if (FileTop.FileTo3 - FileTop.FileFrom3 == 1)                                                \
      {                                                                                            \
        {                                                                                          \
          Body                                                                                     \
        }                                                                                          \
        continue;                                                                                  \
      }                                                                                            \
      file_traverse First = FileTop, Second = FileTop;                                             \
      First.FileTo3[FD] =                                                                          \
        FileTop.FileFrom3[FD] + (FileTop.FileTo3[FD] - FileTop.FileFrom3[FD]) / 2;                 \
      Second.FileFrom3[FD] =                                                                       \
        FileTop.FileFrom3[FD] + (FileTop.FileTo3[FD] - FileTop.FileFrom3[FD]) / 2;                 \
      extent Skip(First.FileFrom3, First.FileTo3 - First.FileFrom3);                               \
      First.Address = FileTop.Address;                                                             \
      Second.Address = FileTop.Address + Prod<u64>(First.FileTo3 - First.FileFrom3);               \
      if (Second.FileFrom3 < To(ExtentInFiles) && From(ExtentInFiles) < Second.FileTo3)            \
        FileStack[++FileTopIdx] = Second;                                                          \
      if (First.FileFrom3 < To(ExtentInFiles) && From(ExtentInFiles) < First.FileTo3)              \
        FileStack[++FileTopIdx] = First;                                                           \
    }                                                                                              \
  }
#endif

#if !defined(idx2ChunkTraverse)
#define idx2_ChunkTraverse(                                                                        \
  Body, StackSize, ChunkOrderIn, ChunkFrom3In, ChunkDims3In, ExtentInChunks, Extent2)              \
  {                                                                                                \
    chunk_traverse ChunkStack[StackSize];                                                          \
    int ChunkTopIdx = 0;                                                                           \
    v3i ChunkDims3Ext((int)NextPow2(ChunkDims3In.X),                                               \
                      (int)NextPow2(ChunkDims3In.Y),                                               \
                      (int)NextPow2(ChunkDims3In.Z));                                              \
    ChunkStack[ChunkTopIdx] = chunk_traverse{                                                      \
      ChunkOrderIn, ChunkOrderIn, ChunkFrom3In, ChunkFrom3In + ChunkDims3Ext, u64(0)               \
    };                                                                                             \
    while (ChunkTopIdx >= 0)                                                                       \
    {                                                                                              \
      chunk_traverse& ChunkTop = ChunkStack[ChunkTopIdx];                                          \
      int CD = ChunkTop.ChunkOrder & 0x3;                                                          \
      ChunkTop.ChunkOrder >>= 2;                                                                   \
      if (CD == 3)                                                                                 \
      {                                                                                            \
        if (ChunkTop.ChunkOrder == 3)                                                              \
          ChunkTop.ChunkOrder = ChunkTop.PrevOrder;                                                \
        else                                                                                       \
          ChunkTop.PrevOrder = ChunkTop.ChunkOrder;                                                \
        continue;                                                                                  \
      }                                                                                            \
      --ChunkTopIdx;                                                                               \
      if (ChunkTop.ChunkTo3 - ChunkTop.ChunkFrom3 == 1)                                            \
      {                                                                                            \
        {                                                                                          \
          Body                                                                                     \
        }                                                                                          \
        continue;                                                                                  \
      }                                                                                            \
      chunk_traverse First = ChunkTop, Second = ChunkTop;                                          \
      First.ChunkTo3[CD] =                                                                         \
        ChunkTop.ChunkFrom3[CD] + (ChunkTop.ChunkTo3[CD] - ChunkTop.ChunkFrom3[CD]) / 2;           \
      Second.ChunkFrom3[CD] =                                                                      \
        ChunkTop.ChunkFrom3[CD] + (ChunkTop.ChunkTo3[CD] - ChunkTop.ChunkFrom3[CD]) / 2;           \
      extent Skip(First.ChunkFrom3, First.ChunkTo3 - First.ChunkFrom3);                            \
      Second.NChunksBefore = First.NChunksBefore + Prod<u64>(Dims(Crop(Skip, ExtentInChunks)));    \
      Second.ChunkInFile = First.ChunkInFile + Prod<i32>(Dims(Crop(Skip, Extent2)));               \
      First.Address = ChunkTop.Address;                                                            \
      Second.Address = ChunkTop.Address + Prod<u64>(First.ChunkTo3 - First.ChunkFrom3);            \
      if (Second.ChunkFrom3 < To(ExtentInChunks) && From(ExtentInChunks) < Second.ChunkTo3)        \
        ChunkStack[++ChunkTopIdx] = Second;                                                        \
      if (First.ChunkFrom3 < To(ExtentInChunks) && From(ExtentInChunks) < First.ChunkTo3)          \
        ChunkStack[++ChunkTopIdx] = First;                                                         \
    }                                                                                              \
  }
#endif

#if !defined(idx2_BrickTraverse)
#define idx2_BrickTraverse(                                                                        \
  Body, StackSize, BrickOrderIn, BrickFrom3In, BrickDims3In, ExtentInBricks, Extent2)              \
  {                                                                                                \
    brick_traverse Stack[StackSize];                                                               \
    int TopIdx = 0;                                                                                \
    v3i BrickDims3Ext((int)NextPow2(BrickDims3In.X),                                               \
                      (int)NextPow2(BrickDims3In.Y),                                               \
                      (int)NextPow2(BrickDims3In.Z));                                              \
    Stack[TopIdx] = brick_traverse{                                                                \
      BrickOrderIn, BrickOrderIn, BrickFrom3In, BrickFrom3In + BrickDims3Ext, u64(0)               \
    };                                                                                             \
    while (TopIdx >= 0)                                                                            \
    {                                                                                              \
      brick_traverse& Top = Stack[TopIdx];                                                         \
      int DD = Top.BrickOrder & 0x3;                                                               \
      Top.BrickOrder >>= 2;                                                                        \
      if (DD == 3)                                                                                 \
      {                                                                                            \
        if (Top.BrickOrder == 3)                                                                   \
          Top.BrickOrder = Top.PrevOrder;                                                          \
        else                                                                                       \
          Top.PrevOrder = Top.BrickOrder;                                                          \
        continue;                                                                                  \
      }                                                                                            \
      --TopIdx;                                                                                    \
      if (Top.BrickTo3 - Top.BrickFrom3 == 1)                                                      \
      {                                                                                            \
        {                                                                                          \
          Body                                                                                     \
        }                                                                                          \
        continue;                                                                                  \
      }                                                                                            \
      brick_traverse First = Top, Second = Top;                                                    \
      First.BrickTo3[DD] = Top.BrickFrom3[DD] + (Top.BrickTo3[DD] - Top.BrickFrom3[DD]) / 2;       \
      Second.BrickFrom3[DD] = Top.BrickFrom3[DD] + (Top.BrickTo3[DD] - Top.BrickFrom3[DD]) / 2;    \
      extent Skip(First.BrickFrom3, First.BrickTo3 - First.BrickFrom3);                            \
      Second.NBricksBefore = First.NBricksBefore + Prod<u64>(Dims(Crop(Skip, ExtentInBricks)));    \
      Second.BrickInChunk = First.BrickInChunk + Prod<i32>(Dims(Crop(Skip, Extent2)));             \
      First.Address = Top.Address;                                                                 \
      Second.Address = Top.Address + Prod<u64>(First.BrickTo3 - First.BrickFrom3);                 \
      if (Second.BrickFrom3 < To(ExtentInBricks) && From(ExtentInBricks) < Second.BrickTo3)        \
        Stack[++TopIdx] = Second;                                                                  \
      if (First.BrickFrom3 < To(ExtentInBricks) && From(ExtentInBricks) < First.BrickTo3)          \
        Stack[++TopIdx] = First;                                                                   \
    }                                                                                              \
  }
#endif



namespace idx2
{


static constexpr i16 ExponentBitPlane_ = -1024;

/* ---------------------- TYPES ----------------------*/
struct brick_traverse
{
  u64 BrickOrder, PrevOrder;
  v3i BrickFrom3, BrickTo3;
  i64 NBricksBefore = 0;
  i32 BrickInChunk = 0;
  u64 Address = 0;
};


struct chunk_traverse
{
  u64 ChunkOrder, PrevOrder;
  v3i ChunkFrom3, ChunkTo3;
  i64 NChunksBefore = 0;
  i32 ChunkInFile = 0;
  u64 Address = 0;
};


struct file_traverse
{
  u64 FileOrder, PrevOrder;
  v3i FileFrom3, FileTo3;
  u64 Address = 0;
};


idx2_Inline bool
BitPlaneIsExponent(i16 BitPlane)
{
  return BitPlane == ExponentBitPlane_;
}


idx2_Inline u64
GetAddress(u64 Brick, int BrickShift, i8 Level, i8 Subband, i16 BitPlane)
{
  return (u64(Level) << 60) +               // 4 bits
         u64((Brick >> BrickShift) << 18) + // 42 bits
         (u64(Subband << 12)) +             // 6 bits
         (u64(BitPlane) & 0xFFF);           // 12 bits
}


idx2_Inline void
UnpackAddress(const idx2_file& Idx2, u64 Address, u64* Brick, i8* Level, i8* Subband, i16* BitPlane)
{
  *BitPlane = i16((i32(Address & 0xFFF) << 20) >> 20); // this convoluted double shifts is to keep the sign of BitPlane
  *Subband = (Address >> 12) & 0x3F;
  *Level = (Address >> 60) & 0xF;
  *Brick = ((Address >> 18) & 0x3FFFFFFFFFFull) << Log2Ceil(Idx2.BricksPerFile[*Level]);
}


idx2_Inline u64
GetChunkAddress(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BitPlane)
{
  return GetAddress(Brick, Log2Ceil(Idx2.BricksPerChunk[Level]), Level, Subband, BitPlane);
}


idx2_Inline u64
GetFileAddress(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BitPlane)
{
  if (Idx2.GroupSubbands)
    Subband = 0;
  if (Idx2.GroupLevels)
    Level = 0;
  if (Idx2.GroupBitPlanes)
    BitPlane = 0;
  return GetAddress(Brick, Log2Ceil(Idx2.BricksPerFile[Level]), Level, Subband, BitPlane);
}


/* Get the linear index of the input brick on the given level */
u64
GetLinearBrick(const idx2_file& Idx2, int Level, v3i Brick3);


v3i
GetSpatialBrick(const idx2_file& Idx2, int Level, u64 LinearBrick);


file_id
ConstructFilePath(const idx2_file& Idx2, u64 Brick, i8 Level, i8 Subband, i16 BitPlane);


file_id
ConstructFilePath(const idx2_file& Idx2, u64 BrickAddress);


// Compose a key from Brick + Level
idx2_Inline u64
GetBrickKey(i8 Level, u64 Brick)
{
  return (Brick << 4) + Level;
}


// Get the Brick from a Key composed of Brick + Level
idx2_Inline u64
GetBrickFromBrickKey(u64 BrickKey)
{
  return BrickKey >> 4;
}


idx2_Inline i8
GetLevelFromBrickKey(u64 BrickKey)
{ return i8(BrickKey & 0xF); }


// Compose a Key from Level + Subband + BitPlane
idx2_Inline u32
GetChannelKey(i16 BitPlane, i8 Level, i8 Subband)
{
  return (u32(BitPlane) << 16) + (u32(Subband) << 4) + Level;
}


// Get Level from a Key composed of Iteration + Level + BitPlane
idx2_Inline i8
GetSubbandFromChannelKey(u64 ChannelKey)
{
  return i8((ChannelKey >> 4) & 0xFFFF);
}


// Get Iteration from a Key composed of Iteration + Level + BitPlane
idx2_Inline i8
GetLevelFromChannelKey(u64 ChannelKey)
{
  return i8(ChannelKey & 0xF);
}


// Get BitPlane from a Key composed of Iteration + Level + BitPlane
idx2_Inline i16
BitPlaneFromChannelKey(u64 ChannelKey)
{
  return i16(ChannelKey >> 16);
}


} // namespace idx2
