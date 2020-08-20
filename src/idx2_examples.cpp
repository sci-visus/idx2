#define idx2_Implementation
#include "idx2_lib.hpp"

void
Decode1() {
  idx2::params P;
  idx2::idx2_file Idx2;
  idx2::buffer OutBuf;
  idx2::Decode(&Idx2, P, &OutBuf);
}

int
main() {
  Decode1();
}
