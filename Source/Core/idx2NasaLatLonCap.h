#pragma once

#include "Array.h"
#include "CircularQueue.h"
#include "Enum.h"
#include "MemoryMap.h"
#include "idx2Common.h"

idx2_Enum(face, u8, Face01, Face2, Face34) // for NASA data
idx2_Enum(grouping, u8, Time, Depth)

namespace idx2 {

/* ---------------------- TYPES ----------------------*/

/* Keep track of NASA LLC2160 files (each storing one time step) */
struct nasa_file {
  mmap_file File;
  i32 Id = 0;
  bool Mapped = false; // whether the file is currently mapped
};

struct nasa_params : public params {
  array<stack_string<64>> FileNames; // each file is a time step
  circular_queue<nasa_file, 64> FileQueue; // circular queue storing the memory mapped NASA time step files
  array<i8> FileIndex; // FileIndex[i] = the index of the file[i] in FileQueue
  i32 N = 0; // the N in N x 3N, N x3N, N x N, 3N x N, 3N x N
  i32 D = 0; // number of depth levels
  i32 Time; // time step
  i8 Depth; // depth
  face Face; // one of Face12, Face3, Face45
  grouping Grouping; // one of Time, Depth
};

/* ---------------------- FUNCTIONS ----------------------*/

void Dealloc(nasa_params* P);

}

