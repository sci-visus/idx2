@echo off

:: Parameters
set "VSPath=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.27.29110"
set "WinSDKLibrary=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.18362.0"
set "WinSDKInclude=C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0"
set "OUTPUT=%2"

:: Setup
set "OLD_PATH=%PATH%"
set "PATH=%VSPath%\bin\Hostx64\x64;%VSBasePath%\bin\Hostx64\x64;%PATH%"

:: Compiler flags
set INCLUDE_PATHS=/I"%WinSDKInclude%\ucrt" /I"%WinSDKInclude%\um" /I"%WinSDKInclude%\shared" /I"%VSPath%\include" /I..\src
set CFLAGS="Please provide a build config: Debug, FastDebug, Profile, Release"
set COMMON_CFLAGS=/std:c++17 /FC /Zi /nologo /EHsc /GR- /Zo /Oi /W4 /wd4702 /wd4201 /wd4100 /wd4189 /wd4505 /wd4127 /arch:AVX2 /Zc:preprocessor
if %1==Release   (set CFLAGS=/O2 /DNDEBUG)
if %1==Profile   (set CFLAGS=/O2 /DNDEBUG)
if %1==FastDebug (set CFLAGS=/Ox /DNDEBUG)
if %1==Debug     (set CFLAGS=/Od /D_DEBUG)

:: Compiler defs
set COMMON_CDEFS=/D_CRT_SECURE_NO_WARNINGS /Didx2_Avx2
if %1==Release   (set CDEFS=                          )
if %1==FastDebug (set CDEFS=/Didx2_Slow=1 /Didx2_Verbose=1)
if %1==Profile   (set CDEFS=                          )
if %1==Debug     (set CDEFS=/Didx2_Slow=1 /Didx2_Verbose=1)

:: Linker flags
set COMMON_LDFLAGS=/machine:x64 /nodefaultlib /subsystem:console /incremental:no /debug:full /opt:ref,icf
if %1==Release   (set LDFLAGS=               )
if %1==Profile   (set LDFLAGS=               )
if %1==FastDebug (set LDFLAGS=-dynamicbase:no)
if %1==Debug     (set LDFLAGS=-dynamicbase:no)

:: Linker lib paths
set COMMON_LIB_PATHS=/libpath:"%VSPath%\lib\x64" /libpath:"%WinSDKLibrary%\ucrt\x64" /libpath:"%WinSDKLibrary%\um\x64"
:: Linker libs
set COMMON_LDLIBS=kernel32.lib User32.lib legacy_stdio_definitions.lib oldnames.lib legacy_stdio_wide_specifiers.lib dbghelp.lib ws2_32.lib
if %1==Release   (set LDLIBS=libucrt.lib  libvcruntime.lib  libcmt.lib  libcpmt.lib  libconcrt.lib )
if %1==Profile   (set LDLIBS=libucrt.lib  libvcruntime.lib  libcmt.lib  libcpmt.lib  libconcrt.lib )
if %1==FastDebug (set LDLIBS=libucrt.lib  libvcruntime.lib  libcmt.lib  libcpmt.lib  libconcrt.lib )
if %1==Debug     (set LDLIBS=libucrtd.lib libvcruntimed.lib libcmtd.lib libcpmtd.lib libconcrtd.lib)

:: Compiling
@echo on
md bin
cd bin
cl.exe "../src/%OUTPUT%.cpp" %INCLUDE_PATHS% %COMMON_CFLAGS% %CFLAGS% %COMMON_CDEFS% %CDEFS%  %COMMON_LDLIBS% %LDLIBS% /link %COMMON_LDFLAGS% %LDFLAGS% %COMMON_LIB_PATHS% /out:"%OUTPUT%.exe"

:: Linking
::link.exe "%OUTPUT%.o" /DEBUG -out:"%OUTPUT%.exe" %COMMON_LDFLAGS% %LDFLAGS% %COMMON_LIB_PATHS% %COMMON_LDLIBS% %LDLIBS%
del "%OUTPUT%.obj"
cd ..

@echo off
set "PATH=%OLD_PATH%"
