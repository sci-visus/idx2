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

FILE(GLOB SOURCES src/idx2.cpp )
source_group("" FILES ${SOURCES})
ADD_EXECUTABLE(idx2 ${SOURCES} ${HEADERS})

target_compile_definitions(idx2 PUBLIC "$<$<CONFIG:DEBUG>:idx2_Slow=1>")
target_compile_definitions(idx2 PUBLIC "$<$<CONFIG:DEBUG>:idx2_Verbose=1>")

if (MSVC)
	target_compile_definitions(idx2 PUBLIC -Didx2_Avx2 -D_CRT_SECURE_NO_WARNINGS)
	target_compile_options(idx2 PUBLIC /Zc:preprocessor)
	target_compile_options(idx2 PUBLIC /arch:AVX2)
	target_link_libraries(idx2 PRIVATE dbghelp.lib)
else()
	target_link_libraries(idx2 PRIVATE pthread)
endif()
