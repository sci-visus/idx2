cmake_minimum_required(VERSION 3.8)
project(idx2Applications CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(idx2 idx2.cpp)
target_link_libraries(idx2 idx2Lib Threads::Threads)

add_executable(examples Examples.cpp)
target_link_libraries(examples idx2Lib Threads::Threads)
