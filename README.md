# What is idx2?
idx2 is a compressed file format for scientific data represented as regular grids of data samples. idx2 supports adaptive, coarse-scale data retrieval in both resolution and precision.
Currently there is only an executable for 2-way conversion between raw binary and the idx2 format, but there will soon be a library for working with the format at a lower level.
idx2 is the next version of the idx file format, which is handled by [OpenVisus](https://github.com/sci-visus/OpenVisus) (alternatively, a fast and lightweight idx reader and writer is [hana](https://github.com/hoangthaiduong/hana)). Compared to idx, idx2 adds the compression capability (through the wavelet transform and [zfp](https://github.com/LLNL/zfp)), as well as the capability to retrieve coarse-precision data (idx only allows coarse-resolution retrieval).

# Compilation
1. On Mac:
`./build-clang.sh Release idx2`
2. On Linux:
`./build-gcc.sh Release idx2` or `./build-clang.sh Release idx2`
3. On Windows:
If you are using the MSVC compiler (`cl.exe`), make sure the three paths in `build-vs.bat` (`VSPath`, `WinSDKLibrary`, and `WinSDKInclude`) are correct for your system.
You may want to copy `build-vs.bat` to another file and modify these paths to suite your system. After that, run `./build-vs.bat Release idx2`.
Alternatively, if you use Clang instead of MSVC, use `build-clang.bat` (after checking the four variables `LLVMPath`, `ClangInclude`, `WinSDKLibrary`, and `WinSDKInclude`).

# Converting from raw to idx2
Use the command: `./bin/idx2 --encode --input MIRANDA-VISCOSITY-[384-384-256]-Float64.raw --version 1 0 --accuracy 1e-16 --brick_size 64 64 64 --files_per_dir 512 --num_passes_per_iteration 1 --num_iterations 2 --tiles_per_file 8 --bricks_per_tile 8 --group_levels --group_bit_planes --out_dir .`
Make sure the input raw file is named in this format: `Name-Field-[DimX-DimY-DimZ]-Type.raw`, where `Name` and `Field` can be anything, `DimX`, `DimY`, `DimZ` are the field dimensions (any of which can be 1), and `Type` is either `Float32` or `Float64` (currently idx2 only supports floating-point scalar fields).
The only options that should be customized are `--input` (the input raw file), `--our-dir` (the output directory), `--num_iterations` (the number of resolution levels) and `--accuracy` (the absolute error tolerance).

# Extracting from idx2 to raw:
Use the command:
`./bin/idx2 --decode --input MIRANDA/VISCOSITY.idx --in_dir . --first 0 0 0 --last 383 383 255 --iteration 1 --mask 128 --accuracy 0.001`.
Use `--first` and `--last` (inclusive) to specify the region of interest (which can be the whole field), `--iteration` and `--mask` (which should always be `128`) to specify the desired resolution level (`0` is the finest level), and `--accuracy` to specify the desired absolute error tolerance. The output will be written to a `.raw` file in the directory specified by `--in_dir`.