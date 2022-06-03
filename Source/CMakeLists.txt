cmake_minimum_required(VERSION 3.8)
project(idx2Lib CXX)

add_subdirectory(Core)
set(IDX2_LIB_SOURCE_FILES idx2Lib.h idx2Lib.cpp)

add_library(idx2Lib ${IDX2_LIB_SOURCE_FILES})
set_target_properties(idx2Lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(idx2Lib PUBLIC idx2Core)

add_subdirectory(Applications)
add_subdirectory(Python)
