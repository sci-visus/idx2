cmake_minimum_required(VERSION 3.8)
project(idx2 CXX)

add_subdirectory(Core)
set(IDX2_LIB_SOURCE_FILES idx2.h idx2.cpp)

add_library(idx2 ${IDX2_LIB_SOURCE_FILES})

set_target_properties(idx2 PROPERTIES 
  PUBLIC_HEADER "idx2.h"
  POSITION_INDEPENDENT_CODE ON)

target_link_libraries(idx2 PUBLIC idx2Core)

target_include_directories(idx2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/Core>
    $<INSTALL_INTERFACE:include>)

add_subdirectory(Applications)
add_subdirectory(Python)

install(TARGETS idx2
	EXPORT idx2-config
  ARCHIVE DESTINATION lib/idx2
  PUBLIC_HEADER DESTINATION include/idx2)

install(EXPORT idx2-config DESTINATION lib/idx2)
