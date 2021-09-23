#ifndef idx2_Slow
#define idx2_Slow
#endif

#define  idx2_Implementation
#include "idx2_lib.h"

using namespace idx2;

int main() 
{
  printf("Running %" PRIi64 " tests..........\n", Size(TestFuncMap));
  for (auto It = Begin(TestFuncMap); It != End(TestFuncMap); ++It) {
    printf("-------- %s:\n", *(It.Key));
    (*(It.Val))();
    printf("PASSED\n");
  }
  return 0;
}
