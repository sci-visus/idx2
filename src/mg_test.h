#pragma once

#include "mg_common.h"
#include "mg_hashtable.h"
#include <inttypes.h>
#include <stdio.h>

namespace idx2 {

// TODO: use array instead of map
using TestFunc = void(*)();
inline hash_table<cstr, TestFunc> TestFuncMap;
inline int Dummy = (Init(&TestFuncMap, 1), 0);
inline bool CalledOnly = false;

#define mg_RegisterTest(Func)\
bool Test##Func() {\
  if (!CalledOnly) {\
    TestFuncMap[#Func] = Func;\
  }\
  return true;\
}\
inline bool VarTest##Func = Test##Func();

#define mg_RegisterTestOnly(Func)\
bool Test##Func() {\
  if (CalledOnly)\
    mg_Assert(false);\
  CalledOnly = true;\
  Clear(&TestFuncMap);\
  TestFuncMap[#Func] = Func;\
  return true;\
}\
inline bool VarTest##Func = Test##Func();

#define mg_TestMain \
int main() {\
  printf("Running %" PRIi64 " tests..........\n", Size(TestFuncMap));\
  for (auto It = Begin(TestFuncMap); It != End(TestFuncMap); ++It) {\
    printf("-------- %s:\n", *(It.Key));\
    (*(It.Val))();\
    printf("PASSED\n");\
  }\
  return 0;\
}

} // namespace idx2
