#!/bin/sh -x
set -x

# Parameters
export COMPILER=g++
export OUTPUT=$2
export INCLUDE_PATHS=-I../src

# Compiler flags
export CFLAGS="-std=gnu++17 -fopenmp-simd -Wall -Wextra -Wpedantic -Wno-missing-braces -Wno-format-zero-length -g -lpthread"

if [ "$1" = "Debug" ]
  then
    export CFLAGS="$CFLAGS -O0"
    export CDEFS="-Dmg_Slow=1"
fi

if [ "$1" = "FastDebug" ]
  then
    export CFLAGS="$CFLAGS -Og -DNDEBUG -ftree-vectorize -march=native"
fi

if [ "$1" = "Release" ]
  then
    export CFLAGS="$CFLAGS -gno-column-info -O2 -DNDEBUG -ftree-vectorize -march=native"
fi

# Compiling
mkdir -p bin
cd bin
${COMPILER} "../src/$2.cpp" ${INCLUDE_PATHS} -o ${OUTPUT} ${CFLAGS} ${CDEFS}
cd ..
