project(idx2Py)
cmake_minimum_required(VERSION 3.17)

find_package (Python COMPONENTS Development)
add_subdirectory(nanobind)

nanobind_add_module(
  idx2Py
  NB_STATIC # Build static libnanobind (the extension module itself remains a shared library)
  idx2Py.cpp
)

include_directories(${idx2Lib_SOURCE_DIR})
target_link_libraries(idx2Py PUBLIC idx2Lib)


install(TARGETS idx2Py LIBRARY DESTINATION .)
