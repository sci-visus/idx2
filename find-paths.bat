@echo off

echo VSPATH (only use this part C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.27.29110)
for /r "C:\Program Files (x86)\Microsoft Visual Studio" %%a in (*) do if "%%~nxa"=="cl.exe" echo %%~dpnxa

echo.

echo WinSDKInclude (only use this part C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0)
for /r "C:\Program Files (x86)\Windows Kits" %%a in (*) do if "%%~nxa"=="stdlib.h" echo %%~dpnxa

echo.

echo WinSDKLibrary (only use this part C:\Program Files (x86)\Windows Kits\10\Lib\10.0.18362.0)
for /r "C:\Program Files (x86)\Windows Kits" %%a in (*) do if "%%~nxa"=="libucrt.lib" echo %%~dpnxa

echo.

echo ClangPath (only use this part C:\Users\User\scoop\shims)
for /r "%userprofile%\scoop\shims" %%a in (*) do if "%%~nxa"=="clang.exe" echo %%~dpnxa
for /r "C:\Program Files\LLVM" %%a in (*) do if "%%~nxa"=="clang.exe" echo %%~dpnxa

echo.

echo ClangInclude (only use this part C:\Users\User\scoop\apps\llvm\10.0.0\lib\clang\10.0.0\include)
for /r "%userprofile%\scoop\apps\llvm" %%a in (*) do if "%%~nxa"=="stddef.h" echo %%~dpnxa
for /r "C:\Program Files\LLVM" %%a in (*) do if "%%~nxa"=="stddef.h" echo %%~dpnxa
