![teaser](https://raw.githubusercontent.com/sci-visus/IDX2/master/teaser.jpg)

# What is idx2?
idx2 is a compressed file format for scientific data represented as 2D or 3D regular grids of data samples. idx2 supports adaptive, coarse-scale data retrieval in both resolution and precision.
idx2 is the next version of the idx file format, which is handled by [OpenVisus](https://github.com/sci-visus/OpenVisus) (alternatively, a less extensive but lightweight idx reader and writer is [hana](https://github.com/hoangthaiduong/hana)). Compared to idx, idx2 features better compression (leveraging [zfp](https://github.com/LLNL/zfp)) and the capability to retrieve coarse-precision data.

Currently there is an executable (named `idx2`) for 2-way conversion between raw binary and the idx2 format, and a header-only library (`idx2.hpp`) for working with the format at a lower level.

# Compilation
You will need a C++ compiler that supports C++17. All output binaries are under the `bin` directory.

1. On Mac:
`./build-clang.sh Release idx2` (Release) or `./build-clang.sh Debug idx2` (Debug)
2. On Linux:
`./build-gcc.sh Release idx2` (Release) or `./build-gcc.sh Debug idx2` (Debug)
3. On Windows using Visual Studio (MSVC):
- Copy `build-vs.bat` to another file (say `my-build-vs.bat`)
- Open `my-build-vs.bat` with a text editor, update the variables `VSPath`, `WinSDKLibrary` and `WinSDKInclude` to match your system (`find-paths.bat` can be used to help locate these paths)
- Run `./my-build-vs.bat Release idx2` (Release) or `./my-build-vs.bat Debug idx2` (Debug)
4. On Windows using Clang:
- Copy `build-clang.bat` to another file (say `my-build-clang.bat`)
- Open `my-build-clang.bat` with a text editor, update the variables `ClangPath` and `ClangInclude` to point to your installation of Clang
- Run `./my-build-clang.bat Release idx2` (Release) or `./my-build-clang.bat Debug idx2` (Debug)

# Using `idx2` to convert from raw to idx2
`idx2 --encode --input MIRANDA-VISCOSITY-[384-384-256]-Float64.raw --accuracy 1e-16 --num_levels 2 --brick_size 64 64 64 --bricks_per_tile 512 --tiles_per_file 512 --files_per_dir 512 --out_dir .`

Make sure the input raw file is named in the `Name-Field-[DimX-DimY-DimZ]-Type.raw` format, where `Name` and `Field` can be anything, `DimX`, `DimY`, `DimZ` are the field's dimensions (any of which can be 1), and `Type` is either `Float32` or `Float64` (currently idx2 only supports **floating-point** scalar fields). Most of the time, the only options that should be customized are `--input` (the input raw file), `--out_dir` (the output directory), `--num_levels` (the number of resolution levels) and `--accuracy` (the absolute error tolerance). The output will be multiple files written to the `out_dir/Name` directory, and the main metadata file is `out_dir/Name/Field.idx2`.

# Using `idx2` to convert from idx2 to raw
`idx2 --decode --input MIRANDA/VISCOSITY.idx2 --in_dir . --first 0 0 0 --last 383 383 255 --level 1 --mask 128 --accuracy 0.001`.

Use `--first` and `--last` (inclusive) to specify the region of interest (which can be the whole field), `--level` and `--mask` (which should be `128` most of the time) to specify the desired resolution level (`0` is the finest level), and `--accuracy` to specify the desired absolute error tolerance. If `--mask` is not provided, a detailed instruction on how it is used will be printed. The output will be written to a `.raw` file in the directory specified by `--in_dir`.

# Using the header-only library `idx2.hpp` to read data from idx2 to memory
`idx2.hpp` is a (auto-generated) single-file, header-only library. To use it, just do

```
#define idx2_Implementation
#include <idx2.hpp>
```

The `#define` line should only appear *once* in your code.
Make sure you include these flags `/std:c++17` and `/Zc:preprocessor` if using MSVC (`cl.exe`), and `-std=gnu++17` if using Clang or GCC.
For instructions on using the library, please refer to the code examples with comments in `idx2_examples.cpp`.

# References
[Efficient and flexible hierarchical data layouts for a unified encoding of scalar field precision and resolution](https://ieeexplore.ieee.org/document/9222049)
D. Hoang, B. Summa, H. Bhatia, P. Lindstrom, P. Klacansky, W. Usher, P-T. Bremer, and V. Pascucci.
2021 - IEEE Transactions on Visualization and Computer Graphics

For the paper preprint and presentation slides, see http://www.sci.utah.edu/~duong
