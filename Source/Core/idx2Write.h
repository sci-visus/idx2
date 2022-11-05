#pragma once


#include "Common.h"
#include "Error.h"
#include "idx2Common.h"


namespace idx2
{


struct encode_data;
struct channel;
struct sub_channel;
struct idx2_file;
struct params;


error<idx2_err_code>
FlushChunkExponents(const idx2_file& Idx2, encode_data* E);

void
WriteChunkExponents(const idx2_file& Idx2, encode_data* E, sub_channel* Sc, i8 Iter, i8 Level);

error<idx2_err_code>
FlushChunks(const idx2_file& Idx2, encode_data* E);

void
WriteChunk(const idx2_file& Idx2, encode_data* E, channel* C, i8 Iter, i8 Level, i16 BitPlane);

void
WriteMetaFile(const idx2_file& Idx2, cstr FileName);

void
WriteMetaFile(const idx2_file& Idx2, const params& P, cstr FileName);

void
PrintStats();


} // namespace idx2

