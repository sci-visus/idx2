cmake_minimum_required(VERSION 3.3)
project(idx2Lib CXX)

add_subdirectory(Core)
set(IDX2_LIB_SOURCE_FILES idx2Lib.h idx2Lib.cpp)

add_subdirectory(Applications)
add_subdirectory(Python)

add_library(idx2Lib SHARED STATIC ${IDX2_LIB_SOURCE_FILES})
target_link_libraries(idx2Lib PUBLIC idx2Core)
