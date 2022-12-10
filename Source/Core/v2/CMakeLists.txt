cmake_minimum_required(VERSION 3.8)
project(idx2Core CXX)

set(IDX2_CORE_HEADER_FILES_V2
  idx2Decode_v2.h
  idx2Encode_v2
  idx2Lookup_v2.h
  idx2Read_v2.h
  idx2Write_v2.h
)

set(IDX2_CORE_SOURCE_FILES_V2
  idx2Encode_v2.cpp
  idx2Lookup_v2.cpp
  idx2Read_v2.cpp
  idx2Write_v2.cpp
)
