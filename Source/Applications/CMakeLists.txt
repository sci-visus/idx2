cmake_minimum_required(VERSION 3.8)
project(idx2Applications CXX)

add_executable(idx2 idx2.cpp)
target_link_libraries(idx2 idx2Lib)

add_executable(examples Examples.cpp)
target_link_libraries(examples idx2Lib)
