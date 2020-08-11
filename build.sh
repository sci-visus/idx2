#!/bin/sh -x
set -x

# Parameters
export COMPILER=g++
export OUTPUT=$2
export INCLUDE_PATHS=-I../src

# Compiler flags
export CFLAGS="Please-provide-a-build-configuration:-Debug/FastDebug/Release"

if [ "$1" = "Debug" ]
  then
    export CFLAGS="
      -std=gnu++2a
      -fopenmp-simd
      -Wall -Wextra -Wpedantic
      -Wno-missing-braces
      -g -O0
      -lpthread"
    export CDEFS="-Dmg_Slow=1"
fi

if [ "$1" = "FastDebug" ]
  then
    export CFLAGS="
      -std=gnu++2a
      -fopenmp-simd
      -Wall -Wextra -Wpedantic
      -Wno-missing-braces
      -g -Og -DNDEBUG -ftree-vectorize -march=native
      -lpthread"
fi

if [ "$1" = "Release" ]
  then
    export CFLAGS="
      -std=gnu++2a
      -fopenmp-simd
      -Wall -Wextra -Wpedantic
      -Wno-missing-braces
      -g -gno-column-info -O2 -DNDEBUG -ftree-vectorize -march=native
      -lpthread"
fi

# Compiling
mkdir -p bin
cd bin
${COMPILER} "../src/$2.cpp" ${INCLUDE_PATHS} -o ${OUTPUT} ${CFLAGS} ${CDEFS}
cd ..
