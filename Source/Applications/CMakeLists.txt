cmake_minimum_required(VERSION 3.3)
project(idx2Applications CXX)

add_executable(idx2 idx2.cpp)
target_link_libraries(idx2 idx2Core)

add_executable(examples Examples.cpp)
target_link_libraries(examples idx2Core)