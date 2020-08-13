/* Command-line argument processing */
// TODO: encapsulate Argc and Argv into a struct so that we can easily pass it around
#pragma once

#include "idx2_common.h"
#include "idx2_array.h"

namespace idx2 {

bool OptVal(int NArgs, cstr* Args, cstr Opt, cstr* Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, int * Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, u8 * Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, f64 * Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, v3i * Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, v2i * Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, t2<char, int>* Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, v3<t2<char, int>>* Val);
bool OptVal(int NArgs, cstr* Args, cstr Opt, array<int>* Vals);
bool OptExists(int NArgs, cstr* Args, cstr Opt);

} // namespace idx2
