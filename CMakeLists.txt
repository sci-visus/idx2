if (PROJECT_SOURCE_DIR)

	MESSAGE(STATUS "IDX2 is a submodule")

else()

	CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

	# must be before PROJECT command
	set_property(GLOBAL PROPERTY USE_FOLDERS ON) 

	# must be before PROJECT command
	SET(MSVC_INCREMENTAL_DEFAULT OFF) 

	PROJECT(IDX2) 

	if (MSVC)
		MESSAGE(STATUS "Detected Visual Studio (C++ __MSVC_VER - WIN32)")
	elseif (COMPILER_ID MATCHES ".*clang")
		set(CLANG 1)
		MESSAGE(STATUS "Detected apple clang (C++: __clang__ __APPLE__)")
	else()
		set(GNUC  1)
		if (APPLE)
			MESSAGE(STATUS "Detected GNU on apple (C++ __GNUC__ __APPLE__)")
			set(APPLE 1)
		else()
			MESSAGE(STATUS "Detected GNU on unix (C++ __GNUC__)")
		endif()
	endif()

endif()


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD              17) # can we lower this requirement to be C++11?
set(CMAKE_CXX_STANDARD_REQUIRED     ON)

macro(SetupIdx2Target target_name)
	target_compile_definitions(${target_name} PUBLIC "$<$<CONFIG:DEBUG>:idx2_Slow=1>")
	target_compile_definitions(${target_name} PUBLIC "$<$<CONFIG:DEBUG>:idx2_Verbose=1>")

	if (MSVC)
		target_compile_definitions(${target_name} PUBLIC -Didx2_Avx2 -D_CRT_SECURE_NO_WARNINGS)
		target_compile_options(${target_name} PUBLIC /Zc:preprocessor /wd5105)
		target_compile_options(${target_name} PUBLIC /arch:AVX2)
		target_link_libraries(${target_name} PUBLIC dbghelp.lib)
	else()
		target_link_libraries(${target_name} PUBLIC pthread)
	endif()
endmacro()

if (NOT DEFINED IDX2_BUILD_LIB)
	option(IDX2_BUILD_LIB ON)
endif()

if (IDX2_BUILD_LIB)

	FILE(GLOB HEADERS src/*.h src/*.hpp)
	source_group("Headers" FILES ${HEADERS})

	FILE(GLOB SOURCES 
		src/idx2_args.cpp
		src/idx2_assert.cpp
		src/idx2_dataset.cpp
		src/idx2_filesystem.cpp
		src/idx2_io.cpp
		src/idx2_logger.cpp
		src/idx2_math.cpp
		src/idx2_memory.cpp
		src/idx2_memory_map.cpp
		src/idx2_stacktrace.cpp
		src/idx2_string.cpp
		src/idx2_utils.cpp
		src/idx2_v1.cpp
		src/idx2_v0.cpp
		src/idx2_varint.cpp
		src/idx2_volume.cpp
		src/idx2_zfp.cpp
		src/idx2_wavelet.cpp
		src/idx2_lib.cpp
	)
	source_group("" FILES ${SOURCES} ${HEADERS})
	ADD_LIBRARY(VisusIdx2 STATIC ${SOURCES} ${HEADERS})
	target_include_directories(VisusIdx2 PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src)
	SetupIdx2Target(VisusIdx2)
endif()


macro(ADD_IDX2_EXECUTABLE target_name)
	ADD_EXECUTABLE(${target_name} ${ARGN})
	set_target_properties(${target_name} PROPERTIES FOLDER "bin/")
	if (IDX2_BUILD_LIB)
		target_link_libraries(${target_name} PUBLIC VisusIdx2)
	else()
		SetupIdx2Target(${target_name})
		target_compile_definitions(${target_name} PUBLIC -Didx2_Implementation)
	endif()
endmacro()

ADD_IDX2_EXECUTABLE(idx2                      src/idx2.cpp)
ADD_IDX2_EXECUTABLE(idx2_tests                src/idx2_tests.cpp src/idx2_wavelet_test.cpp)
ADD_IDX2_EXECUTABLE(idx2_examples             src/idx2_examples.cpp )
ADD_IDX2_EXECUTABLE(idx2_app_range_expansion  src/app_range_expansion.cpp)

set_target_properties(idx2                     PROPERTIES FOLDER Executable/)
set_target_properties(idx2_tests               PROPERTIES FOLDER Executable/)
set_target_properties(idx2_examples            PROPERTIES FOLDER Executable/)
set_target_properties(idx2_app_range_expansion PROPERTIES FOLDER Executable/)
