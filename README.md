# What is idx2?
idx2 is a compressed file format for scientific data that supports adaptive data access in both resolution and precision.
Currently there is only an executable for 2-way conversion between raw binary and the idx2 format, but there will soon be a library for working with the format at a lower level.
(TODO): refer to the original idx

# Compilation
1. On Mac
`./build-clang.sh Release idx2`
2. On Linux
`./build-gcc.sh Release idx2` or `./build-clang.sh Release idx2`
3. Windows
If you are using the MSVC compiler (`cl.exe`), make sure the three paths in `build-vs.bat` (`VSPath`, `WinSDKLibrary`, and `WinSDKInclude`) are correct for your system.
You may want to copy `build-vs.bat` to another file and modify these paths to suite your system. Then run `./build-vs.bat Release idx2`.
Alternatively, if you use Clang instead of MSVC, use `build-clang.bat` (after checking the four variables `LLVMPath`, `ClangInclude`, `WinSDKLibrary`, and `WinSDKInclude`).

# Convert from raw to idx2
`./bin/idx2.exe --encode --input MIRANDA-VISCOSITY-[384-384-256]-Float64.raw --version 1 0 --accuracy 1e-16 --brick_size 64 64 64 --files_per_dir 512 --num_passes_per_iteration 1 --num_iterations 2 --tiles_per_file 8 --bricks_per_tile 8 --group_levels --group_bit_planes --out_dir .`
Make sure the raw file is named in this format: `Name-Field-[DimX-DimY-DimZ]-Type.raw`, where `Name` and `Field` can be anything, `DimX`, `DimY`, `DimZ` are the field dimensions (any of which can be 1), and `Type` is either `Float32` or `Float64` (currently idx2 only supports floating-point scalar fields).
The only options that should be customized in the command above is `--num_iterations` (which controls the number of resolution levels) and `--accuracy` (which controls the absolute error tolerance).
