# What is idx2?
idx2 is a compressed file format for scientific data represented as 2D or 3D regular grids of data samples. idx2 supports adaptive, coarse-scale data retrieval in both resolution and precision.
Currently there is only an executable (named `idx2`) for 2-way conversion between raw binary and the idx2 format, but there will soon be a library for working with the format at a lower level.
idx2 is the next version of the idx file format, which is handled by [OpenVisus](https://github.com/sci-visus/OpenVisus) (alternatively, a fast and lightweight idx reader and writer is [hana](https://github.com/hoangthaiduong/hana)). Compared to idx, idx2 adds the compression capability (by leveraging [zfp](https://github.com/LLNL/zfp)), as well as the capability to retrieve coarse-precision data.

# Compilation
You will need a C++ compiler that supports C++17. All output binaries are under the `bin` directory.

1. On Mac:
`./build-clang.sh Release idx2`
2. On Linux:
`./build-gcc.sh Release idx2` or `./build-clang.sh Release idx2`
3. On Windows:
If you are using the MSVC compiler (`cl.exe`), make sure the three paths in `build-vs.bat` (`VSPath`, `WinSDKLibrary`, and `WinSDKInclude`) are correct for your system (`find-paths.bat` can be used to locate these paths in common install locations).
You might want to copy `build-vs.bat` to another file (say `my-build-vs.bat`) and modify these paths to suite your system.
After that, run `./my-build-vs.bat Release idx2`.
Alternatively, if you use Clang instead of MSVC, use `build-clang.bat` (after possibly updating the variables `ClangPath` and `ClangInclude` beside the other three variables).

# Converting from raw to idx2
`idx2 --encode --input MIRANDA-VISCOSITY-[384-384-256]-Float64.raw --version 1 0 --accuracy 1e-16 --brick_size 64 64 64 --files_per_dir 512 --num_iterations 2 --tiles_per_file 8 --bricks_per_tile 8 --out_dir .`

Make sure the input raw file is named in the `Name-Field-[DimX-DimY-DimZ]-Type.raw` format, where `Name` and `Field` can be anything, `DimX`, `DimY`, `DimZ` are the field's dimensions (any of which can be 1), and `Type` is either `Float32` or `Float64` (currently idx2 only supports floating-point scalar fields). Most of the time, the only options that should be customized are `--input` (the input raw file), `--out_dir` (the output directory), `--num_iterations` (the number of resolution levels) and `--accuracy` (the absolute error tolerance). The output will be multiple files written to the `out_dir/Name` directory, and the main metadata file is `out_dir/Name/Field.idx`.

# Extracting from idx2 to raw:
`idx2 --decode --input MIRANDA/VISCOSITY.idx --in_dir . --first 0 0 0 --last 383 383 255 --iteration 1 --mask 128 --accuracy 0.001`.

Use `--first` and `--last` (inclusive) to specify the region of interest (which can be the whole field), `--iteration` and `--mask` (which should be `128` most of the time) to specify the desired resolution level (`0` is the finest level), and `--accuracy` to specify the desired absolute error tolerance. If `--mask` is not provided, a detailed instruction on how it is used will be printed. The output will be written to a `.raw` file in the directory specified by `--in_dir`.