CMAKE_MINIMUM_REQUIRED(VERSION 3.0)


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

	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	set(CMAKE_CXX_STANDARD              17) # can we lower this requirement to be C++11?
	set(CMAKE_CXX_STANDARD_REQUIRED     ON)

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
source_group("" FILES ${SOURCES})
ADD_LIBRARY(idx2_lib STATIC ${SOURCES} ${HEADERS})

target_compile_definitions(idx2_lib PUBLIC "$<$<CONFIG:DEBUG>:idx2_Slow=1>")
target_compile_definitions(idx2_lib PUBLIC "$<$<CONFIG:DEBUG>:idx2_Verbose=1>")

if (MSVC)
	target_compile_definitions(idx2_lib PUBLIC -Didx2_Avx2 -D_CRT_SECURE_NO_WARNINGS)
	target_compile_options(idx2_lib PUBLIC /Zc:preprocessor /wd5105)
	target_compile_options(idx2_lib PUBLIC /arch:AVX2)
	target_link_libraries(idx2_lib PUBLIC dbghelp.lib)
else()
	target_link_libraries(idx2_lib PUBLIC pthread)
endif()

ADD_EXECUTABLE(idx2 src/idx2.cpp)
target_link_libraries(idx2 PUBLIC idx2_lib)

# broken
# ADD_EXECUTABLE(idx2_tests src/idx2_tests.cpp)
# target_link_libraries(idx2 PUBLIC idx2_lib)