cmake_minimum_required(VERSION 3.8)
project(idx2Core CXX)

set(IDX2_CORE_SOURCE_FILES
  Algorithm.h
  Args.cpp
  Args.h
  Array.h
  Assert.cpp
  Assert.h
  BitOps.h
  BitStream.h
  CircularQueue.h
  Codecs.h
  Common.h
  DataSet.cpp
  DataSet.h
  DataTypes.h
  DebugBreak.h
  Enum.h
  Error.h
  ErrorCodes.h
  Expected.h
  FileUtilities.h
  Filesystem.cpp
  Filesystem.h
  Function.h
  HashSet.h
  HashTable.h
  idx2Common.h
  idx2Common.cpp
  idx2Decode.h
  idx2Decode.cpp
  idx2Encode.h  
  idx2Encode.cpp
  idx2Lookup.h
  idx2Lookup.cpp
  InputOutput.cpp
  InputOutput.h
  LinkedList.h
  Logger.cpp
  Logger.h
  Macros.h
  Math.h
  Memory.cpp
  Memory.h
  MemoryMap.cpp
  MemoryMap.h
  Mutex.h
  Random.h
  ScopeGuard.h
  Stacktrace.cpp
  Stacktrace.h
  Statistics.h
  String.cpp
  String.h
  Test.h
  Timer.h
  Utilities.cpp
  Utilities.h
  VarInt.cpp
  VarInt.h
  Volume.cpp
  Volume.h
  Wavelet.cpp
  Wavelet.h
  Zfp.cpp
  Zfp.h
)

add_library(idx2Core SHARED STATIC ${IDX2_CORE_SOURCE_FILES})

target_compile_features(idx2Core PUBLIC cxx_std_17)
target_compile_definitions(idx2Core PUBLIC -Didx2_Avx2)
if (MSVC)
  target_compile_definitions(idx2Core PUBLIC -D_CRT_SECURE_NO_WARNINGS)
  target_compile_options(idx2Core PUBLIC /Zc:preprocessor /Zc:__cplusplus /wd5105)
  target_link_options(idx2Core PUBLIC dbghelp.lib)
elseif (UNIX)
  target_compile_options(idx2Core PUBLIC -Wno-format-zero-length)
endif()
