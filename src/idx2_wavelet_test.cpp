#include "idx2_test.h"
#include "idx2_array.h"
#include "idx2_common.h"
#include "idx2_function.h"
#include "idx2_random.h"
#include "idx2_timer.h"
#include "idx2_wavelet.h"
#include "idx2_volume.h"
#include <math.h>
#include <string.h>

#define Array10x10\
 { 56, 40, 8, 24, 48, 48, 40, 16, 30, 32, 0,\
   40, 8, 24, 48, 48, 40, 16, 30, 32, 56, 0,\
   8, 24, 48, 48, 40, 16, 30, 32, 56, 40, 0,\
   24, 48, 48, 40, 16, 30, 32, 56, 40, 8, 0,\
   48, 48, 40, 16, 30, 32, 56, 40, 8, 24, 0,\
   48, 40, 16, 30, 32, 56, 40, 8, 24, 48, 0,\
   40, 16, 30, 32, 56, 40, 8, 24, 48, 48, 0,\
   16, 30, 32, 56, 40, 8, 24, 48, 48, 40, 0,\
   30, 32, 56, 40, 8, 24, 48, 48, 40, 16, 0,\
   32, 56, 40, 8, 24, 48, 48, 40, 16, 30, 0,\
    0,  0,  0, 0,  0,  0,  0,  0,  0,  0, 0 }

#define Array9x9\
 { 56, 40,  8, 24, 48, 48, 40, 16, 30,\
   40,  8, 24, 48, 48, 40, 16, 30, 32,\
    8, 24, 48, 48, 40, 16, 30, 32, 56,\
   24, 48, 48, 40, 16, 30, 32, 56, 40,\
   48, 48, 40, 16, 30, 32, 56, 40,  8,\
   48, 40, 16, 30, 32, 56, 40,  8, 24,\
   40, 16, 30, 32, 56, 40,  8, 24, 48,\
   16, 30, 32, 56, 40,  8, 24, 48, 48,\
   30, 32, 56, 40,  8, 24, 48, 48, 40 }

#define Array2_9x9\
 {  1,  2,  3,  4,  5,  6,  7,  8,  9,\
   10, 11, 12, 13, 14, 15, 16, 17, 18,\
   19, 20, 21, 22, 23, 24, 25, 26, 27,\
   28, 29, 30, 31, 32, 33, 34, 35, 36,\
   37, 38, 39, 40, 41, 42, 43, 44, 45,\
   46, 47, 48, 49, 50, 51, 52, 53, 54,\
   55, 56, 57, 58, 59, 60, 61, 62, 63,\
   64, 65, 66, 67, 68, 69, 70, 71, 72,\
   73, 74, 75, 76, 77, 78, 79, 80, 81 }

#define Array3_9x9\
 {  1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1,\
    1,  1,  1,  1,  1,  1,  1,  1,  1 }

#define Array9x9x9\
 { 56, 40,  8, 24, 48, 48, 40, 16, 30,  40,  8, 24, 48, 48, 40, 16, 30, 32,   8, 24, 48, 48, 40, 16, 30, 32, 56,\
   24, 48, 48, 40, 16, 30, 32, 56, 40,  48, 48, 40, 16, 30, 32, 56, 40,  8,  48, 40, 16, 30, 32, 56, 40,  8, 24,\
   40, 16, 30, 32, 56, 40,  8, 24, 48,  16, 30, 32, 56, 40,  8, 24, 48, 48,  30, 32, 56, 40,  8, 24, 48, 48, 40,\
                                                                                                                \
   40,  8, 24, 48, 48, 40, 16, 30, 32,   8, 24, 48, 48, 40, 16, 30, 32, 56,  24, 48, 48, 40, 16, 30, 32, 56, 40,\
   48, 48, 40, 16, 30, 32, 56, 40,  8,  48, 40, 16, 30, 32, 56, 40,  8, 24,  40, 16, 30, 32, 56, 40,  8, 24, 48,\
   16, 30, 32, 56, 40,  8, 24, 48, 48,  30, 32, 56, 40,  8, 24, 48, 48, 40,  56, 40,  8, 24, 48, 48, 40, 16, 30,\
                                                                                                                \
    8, 24, 48, 48, 40, 16, 30, 32, 56,  24, 48, 48, 40, 16, 30, 32, 56, 40,  48, 48, 40, 16, 30, 32, 56, 40,  8,\
   48, 40, 16, 30, 32, 56, 40,  8, 24,  40, 16, 30, 32, 56, 40,  8, 24, 48,  16, 30, 32, 56, 40,  8, 24, 48, 48,\
   30, 32, 56, 40,  8, 24, 48, 48, 40,  56, 40,  8, 24, 48, 48, 40, 16, 30,  40,  8, 24, 48, 48, 40, 16, 30, 32,\
                                                                                                                \
   24, 48, 48, 40, 16, 30, 32, 56, 40,  48, 48, 40, 16, 30, 32, 56, 40,  8,  48, 40, 16, 30, 32, 56, 40,  8, 24,\
   40, 16, 30, 32, 56, 40,  8, 24, 48,  16, 30, 32, 56, 40,  8, 24, 48, 48,  30, 32, 56, 40,  8, 24, 48, 48, 40,\
   56, 40,  8, 24, 48, 48, 40, 16, 30,  40,  8, 24, 48, 48, 40, 16, 30, 32,   8, 24, 48, 48, 40, 16, 30, 32, 56,\
                                                                                                                \
   48, 48, 40, 16, 30, 32, 56, 40,  8,  48, 40, 16, 30, 32, 56, 40,  8, 24,  40, 16, 30, 32, 56, 40,  8, 24, 48,\
   16, 30, 32, 56, 40,  8, 24, 48, 48,  30, 32, 56, 40,  8, 24, 48, 48, 40,  56, 40,  8, 24, 48, 48, 40, 16, 30,\
   40,  8, 24, 48, 48, 40, 16, 30, 32,   8, 24, 48, 48, 40, 16, 30, 32, 56,  24, 48, 48, 40, 16, 30, 32, 56, 40,\
                                                                                                                \
   48, 40, 16, 30, 32, 56, 40,  8, 24,  40, 16, 30, 32, 56, 40,  8, 24, 48,  16, 30, 32, 56, 40,  8, 24, 48, 48,\
   30, 32, 56, 40,  8, 24, 48, 48, 40,  56, 40,  8, 24, 48, 48, 40, 16, 30,  40,  8, 24, 48, 48, 40, 16, 30, 32,\
    8, 24, 48, 48, 40, 16, 30, 32, 56,  24, 48, 48, 40, 16, 30, 32, 56, 40,  48, 48, 40, 16, 30, 32, 56, 40,  8,\
                                                                                                                \
   40, 16, 30, 32, 56, 40,  8, 24, 48,  16, 30, 32, 56, 40,  8, 24, 48, 48,  30, 32, 56, 40,  8, 24, 48, 48, 40,\
   56, 40,  8, 24, 48, 48, 40, 16, 30,  40,  8, 24, 48, 48, 40, 16, 30, 32,   8, 24, 48, 48, 40, 16, 30, 32, 56,\
   24, 48, 48, 40, 16, 30, 32, 56, 40,  48, 48, 40, 16, 30, 32, 56, 40,  8,  48, 40, 16, 30, 32, 56, 40,  8, 24,\
                                                                                                                \
   16, 30, 32, 56, 40,  8, 24, 48, 48,  30, 32, 56, 40,  8, 24, 48, 48, 40,  56, 40,  8, 24, 48, 48, 40, 16, 30,\
   40,  8, 24, 48, 48, 40, 16, 30, 32,   8, 24, 48, 48, 40, 16, 30, 32, 56,  24, 48, 48, 40, 16, 30, 32, 56, 40,\
   48, 48, 40, 16, 30, 32, 56, 40,  8,  48, 40, 16, 30, 32, 56, 40,  8, 24,  40, 16, 30, 32, 56, 40,  8, 24, 48,\
                                                                                                                \
   30, 32, 56, 40,  8, 24, 48, 48, 40,  56, 40,  8, 24, 48, 48, 40, 16, 30,  40,  8, 24, 48, 48, 40, 16, 30, 32,\
    8, 24, 48, 48, 40, 16, 30, 32, 56,  24, 48, 48, 40, 16, 30, 32, 56, 40,  48, 48, 40, 16, 30, 32, 56, 40,  8,\
   48, 40, 16, 30, 32, 56, 40,  8, 24,  40, 16, 30, 32, 56, 40,  8, 24, 48,  16, 30, 32, 56, 40,  8, 24, 48, 48 }

#define Array7x6x5\
 { 56, 40,  8, 24, 48, 48, 40,   16, 30, 32, 40,  8, 24, 48,\
   48, 40, 16, 30, 32, 56,  8,   24, 48, 48, 40, 16, 30, 32,\
   56, 40, 24, 48, 48, 40, 16,   30, 32, 56, 40,  8, 48, 48,\
                                                            \
   40, 16, 30, 32, 56, 40,  8,   24, 48, 40, 16, 30, 32, 56,\
   40,  8, 24, 48, 40, 16, 30,   32, 56, 40,  8, 24, 48, 48,\
   16, 30, 32, 56, 40,  8, 24,   48, 48, 40, 30, 32, 56, 40,\
                                                            \
    8, 24, 48, 48, 40, 16, 32,   56, 40,  8, 24, 48, 48, 40,\
   16, 30, 56, 40,  8, 24, 48,   48, 40, 16, 30, 32, 40,  8,\
   24, 48, 48, 40, 16, 30, 32,   56,  8, 24, 48, 48, 40, 16,\
                                                            \
   30, 32, 56, 40, 24, 48, 48,   40, 16, 30, 32, 56, 40,  8,\
   48, 48, 40, 16, 30, 32, 56,   40,  8, 24, 48, 40, 16, 30,\
   32, 56, 40, 8, 24, 48,  40,   16, 30, 32, 56, 40,  8, 24,\
                                                            \
   48, 48, 16, 30, 32, 56, 40,    8, 24, 48, 48, 40, 30, 32,\
   56, 40,  8, 24, 48, 48, 40,   16, 32, 56, 40,  8, 24, 48,\
   48, 40, 16, 30, 56, 40,  8,   24, 48, 48, 40, 16, 30, 32 }

using namespace idx2;

void
TestWavelet() {
  { // 1D, one level
    f64 A[] = Array10x10;
    f64 B[] = Array10x10;
    volume Vol(A, v3i(11, 1, 1));
    grid Grid(v3i(10, 1, 1));
    ForwardCdf53(Grid, 1, &Vol);
    InverseCdf53(Grid, 1, &Vol);
    for (int X = 0; X < Size(Grid); ++X) {
      idx2_Assert((fabs(A[X] - B[X]) < 1e-9));
    }
  }
  { // 1D, three levels
    f64 A[] = Array10x10;
    f64 B[] = Array10x10;
    volume Vol(A, v3i(11, 1, 1));
    grid Grid(v3i(10, 1, 1));
    ForwardCdf53(Grid, 3, &Vol);
    InverseCdf53(Grid, 3, &Vol);
    for (int X = 0; X < Size(Grid); ++X) {
      idx2_Assert(fabs(A[X] - B[X]) < 1e-9);
    }
  }
  { // 2D, one level
    f64 A[] = Array10x10;
    f64 B[] = Array10x10;
    f64 C[11 * 11];
    volume VolA(A, v3i(10, 10, 1)), VolB(B, v3i(10, 10, 1)), VolC(C, v3i(11, 11, 1));
    grid Grid(v3i(10, 10, 1));
    Copy<f64, f64>(Grid, VolA, Grid, &VolC);
    ForwardCdf53(Grid, 1, &VolC);
    InverseCdf53(Grid, 1, &VolC);
    auto ItrC = Begin<f64>(Grid, VolC);
    auto ItrB = Begin<f64>(VolB);
    for (; ItrC != End<f64>(Grid, VolC); ++ItrC, ++ItrB) {
      idx2_Assert(fabs(*ItrC - *ItrB) < 1e-9);
    }
  }
  { // 2D, three levels
    f64 A[] = Array10x10;
    f64 B[] = Array10x10;
    f64 C[11 * 11];
    volume VolA(A, v3i(10, 10, 1)), VolB(B, v3i(10, 10, 1)), VolC(C, v3i(11, 11, 1));
    grid Grid(v3i(10, 10, 1));
    Copy<f64, f64>(Grid, VolA, Grid, &VolC);
    ForwardCdf53(Grid, 3, &VolC);
    InverseCdf53(Grid, 3, &VolC);
    auto ItrC = Begin<f64>(Grid, VolC);
    auto ItrB = Begin<f64>(VolB);
    for (; ItrC != End<f64>(Grid, VolC); ++ItrC, ++ItrB) {
      idx2_Assert(fabs(*ItrC - *ItrB) < 1e-9);
    }
  }
  { // 3D, one level
    f64 A[] = Array7x6x5;
    f64 B[] = Array7x6x5;
    f64 C[8 * 7 * 6];
    volume VolA(A, v3i(7, 6, 5)), VolB(B, v3i(7, 6, 5)), VolC(C, v3i(8, 7, 6));
    grid Grid(v3i(7, 6, 5));
    Copy<f64, f64>(Grid, VolA, Grid, &VolC);
    ForwardCdf53(Grid, 1, &VolC);
    InverseCdf53(Grid, 1, &VolC);
    auto ItrC = Begin<f64>(Grid, VolC);
    auto ItrB = Begin<f64>(VolB);
    for (; ItrC != End<f64>(Grid, VolC); ++ItrC, ++ItrB) {
      idx2_Assert(fabs(*ItrC - *ItrB) < 1e-9);
    }
  }
  { // 3D, two levels
    f64 A[] = Array7x6x5;
    f64 B[] = Array7x6x5;
    f64 C[8 * 7 * 6];
    volume VolA(A, v3i(7, 6, 5)), VolB(B, v3i(7, 6, 5)), VolC(C, v3i(8, 7, 6));
    grid Grid(v3i(7, 6, 5));
    Copy<f64, f64>(Grid, VolA, Grid, &VolC);
    ForwardCdf53(Grid, 2, &VolC);
    InverseCdf53(Grid, 2, &VolC);
    auto ItrC = Begin<f64>(Grid, VolC);
    auto ItrB = Begin<f64>(VolB);
    for (; ItrC != End<f64>(Grid, VolC); ++ItrC, ++ItrB) {
      idx2_Assert(fabs(*ItrC - *ItrB) < 1e-9);
    }
  }
  { // "real" 3d volume
    volume Vol(v3i(64), dtype::float64);
    ReadVolume("D:/Datasets/3D/Small/MIRANDA-DENSITY-[64-64-64]-Float64.raw",
               v3i(64), dtype::float64, &Vol);
    volume VolExt(v3i(65), dtype::float64);
    extent Ext(v3i(64));
    Copy<f64>(grid(Ext), Vol, &VolExt);
    ForwardCdf53(Ext, 5, &VolExt);
    InverseCdf53(Ext, 5, &VolExt);
    volume_iterator It = Begin<f64>(Vol), VolEnd = End<f64>(Vol);
    extent_iterator ItExt = Begin<f64>(Ext, VolExt);
    for (; It != VolEnd; ++It, ++ItExt) {
      idx2_Assert(fabs(*It - *ItExt) < 1e-9);
    }
  }
  { // "real" 3d volume
    //volume Vol(v3i(384, 384, 256), dtype::float64);
    //ReadVolume("D:/Datasets/3D/Miranda/MIRANDA-DENSITY-[384-384-256]-Float64.raw",
    //           v3i(384, 384, 256), dtype::float64, &Vol);
    //grid_volume Grid(Vol);
    //volume VolCopy(v3i(385, 385, 257), dtype::float64);
    //grid_volume GridCopy(v3i(384, 384, 256), VolCopy);
    //Copy(Grid, &GridCopy);
    //ForwardCdf53(&GridCopy, 7);
    //InverseCdf53(&GridCopy, 7);
    //volume_iterator It = Begin<f64>(Vol), VolEnd = End<f64>(Vol);
    //grid_iterator CopyIt = Begin<f64>(GridCopy);
    //for (; It != VolEnd; ++It, ++CopyIt) {
    //  idx2_Assert(fabs(*It - *CopyIt) < 1e-9);
    //}
  }
}

void
TestWaveletBlock() {
  { // test with 2 tiles
    f64 A[] = Array9x9;
    f64 B[] = Array9x9;
    f64 C[] = Array9x9;
    f64 D[9 * 9] = {};
    volume VolA(A, v3i(9, 9, 1)), VolB(B, v3i(9, 9, 1));
    volume VolC(C, v3i(9, 9, 1)), VolD(D, v3i(9, 9, 1));
    extent ExtLeft(v3i(0, 0, 0), v3i(5, 9, 1));
    extent ExtRght(v3i(4, 0, 0), v3i(5, 9, 1));
    FLiftCdf53X<f64>(grid(ExtLeft), Dims(VolA), lift_option::PartialUpdateLast, &VolA);
    FLiftCdf53Y<f64>(grid(ExtLeft), Dims(VolA), lift_option::Normal, &VolA);
    FLiftCdf53X<f64>(grid(ExtRght), Dims(VolB), lift_option::Normal, &VolB);
    FLiftCdf53Y<f64>(grid(ExtRght), Dims(VolB), lift_option::Normal, &VolB);
    Add(ExtLeft, VolA, ExtLeft, &VolD);
    Add(ExtRght, VolB, ExtRght, &VolD);
    FLiftCdf53X<f64>(grid(Dims(VolC)), Dims(VolC), lift_option::Normal, &VolC);
    FLiftCdf53Y<f64>(grid(Dims(VolC)), Dims(VolC), lift_option::Normal, &VolC);
    auto ItrC = Begin<f64>(VolC);
    auto ItrD = Begin<f64>(VolD);
    for (ItrC = Begin<f64>(VolC); ItrC != End<f64>(VolC); ++ItrC, ++ItrD) {
      idx2_Assert(fabs(*ItrC - *ItrD) < 1e-15);
    }
  }
  { // 2D test
//    f64 A[] = Array9x9;
//    v3i M(9, 9, 1);
//    volume VolA(A, M);
//    volume VolB; Clone(VolA, &VolB);
//    Fill(Begin<f64>(VolB), End<f64>(VolB), 0);
//    ForwardCdf53Tile2D(1, v3i(4, 4, 1), VolA, &VolB);
//    ForwardCdf53Old(&VolA, 1);
//    for (auto ItA = Begin<f64>(VolA), ItB = Begin<f64>(VolB);
//         ItA != End<f64>(VolA); ++ItA, ++ItB) {
//      idx2_Assert(*ItA == *ItB);
//    }
  }
  { // bigger 2D test
//    v3i M(17, 17, 1);
//    f64 A[17 * 17];
//    for (int Y = 0; Y < M.Y; ++Y) {
//      f64 YY = (Y - M.Y / 2.0) / 2.0;
//      f64 Vy = 3 / sqrt(2 * Pi) * exp(-0.5 * YY * YY);
//      for (int X = 0; X < M.X; ++X) {
//        f64 XX = (X - M.X / 2.0) / 2.0;
//        f64 Vx = 3 / sqrt(2 * Pi) * exp(-0.5 * XX * XX);
//        A[Y * 17 + X] = Vx * Vy;
//      }
//    }
//    volume VolA(A, M);
//    volume VolB; Clone(VolA, &VolB);
//    Fill(Begin<f64>(VolB), End<f64>(VolB), 0);
//    int NLevels = 2;
//    v3i TDims3(4, 4, 1);
//    ForwardCdf53Tile2D(NLevels, TDims3, VolA, &VolB);
//    ForwardCdf53Old(&VolA, NLevels);
//    for (auto ItA = Begin<f64>(VolA), ItB = Begin<f64>(VolB);
//         ItA != End<f64>(VolA); ++ItA, ++ItB) {
//      idx2_Assert(fabs(*ItA - *ItB) < 1e-15);
//    }
  }
  { // small 3D test
//    f64 A[] = Array9x9x9;
//    v3i M(9);
//    volume VolA(A, M);
//    volume VolB; Clone(VolA, &VolB);
//    Fill(Begin<f64>(VolB), End<f64>(VolB), 0);
//    ForwardCdf53Tile(1, v3i(4), VolA, &VolB);
//    ForwardCdf53Old(&VolA, 1);
//    array<extent> Sbands; BuildSubbands(M, 1, &Sbands);
//    for (int Sb = 0; Sb < Size(Sbands); ++Sb) {
//      v3i SbFrom3 = From(Sbands[Sb]);
//      v3i SbDims3 = Dims(Sbands[Sb]);
//      v3i T;
//      idx2_BeginFor3(T, SbFrom3, SbFrom3 + SbDims3, v3i(4)) {
//        v3i P;
//        v3i D3 = Min(SbFrom3 + SbDims3 - P, v3i(4));
//        idx2_BeginFor3(P, T, T + D3, v3i::One) {
//          f64 Va = VolA.At<f64>(P);
//          f64 Vb = VolB.At<f64>(P);
//          idx2_Assert(fabs(Va - Vb) < 1e-9);
//        } idx2_EndFor3
//      } idx2_EndFor3
//    }
  }
  //{ // big 3D test
  //  v3i M(384, 384, 256);
  //  volume Vol;
  //  volume OutVol(M, dtype::float64);
  //  ReadVolume("D:/Datasets/3D/Miranda/MIRANDA-DENSITY-[384-384-256]-Float64.raw",
  //             M, dtype::float64, &Vol);
  //  timer Timer;
  //  StartTimer(&Timer);
  //  ForwardCdf53Tile(3, v3i(32), Vol, &OutVol);
  //  auto TotalTime = Milliseconds(ResetTimer(&Timer));
  //  ForwardCdf53Old(&Vol, 3);
  //  auto TotalTime2 = Milliseconds(ElapsedTime(&Timer));
  //  printf("Time1 %fms Time2 %fms\n", TotalTime, TotalTime2);
  //}
}

void
TestWavGrid() {
  // v3i N(16, 16, 1);
  // int NLevels = 1;
  // int Sb = 0;
  // extent WavBlock(v3i(0), v3i(4));
  // array<grid> Subbands;
  // BuildSubbands(N, NLevels, &Subbands);
  // grid WavGrid = SubGrid(Subbands[Sb], WavBlock);
  // wav_grids WGrids;
  // wav_grids WG = ComputeWavGrids(2, 0, extent(v3i(0), v3i(4)), WavGrid, v3i(1000));
  //printf("ValGrid: " idx2_PrStrGrid"\n", idx2_PrGrid(WG.ValGrid));
  //printf("WavGrid: " idx2_PrStrGrid"\n", idx2_PrGrid(WG.WavGrid));
  //printf("WrkGrid: " idx2_PrStrGrid"\n", idx2_PrGrid(WG.WrkGrid));
}

void
TestWaveletExtrapolation() {
  // 1D
  {
    v3i D3(34, 1, 1); // Dims
    v3i BlockSize3(128, 1, 1);
    v3i P3 = BlockSize3;
    volume Vol(BlockSize3 + 1, dtype::float64); // should have dims 129, 1, 1
    Fill(Begin<f64>(Vol), End<f64>(Vol), 0.0);
    ReadVolume("/Users/TrAnG/Desktop/Downloads/Small/MIRANDA-PRESSURE-[34-1-1]-Float64.raw", D3, dtype::float64, &Vol);
    int NLevels = Log2Floor(BlockSize3.X);
    v3i R3 = D3 + IsEven(D3);
    v3i S3(1); // Strides
    SetDims(&Vol, BlockSize3 + 1);
    stack_array<v3i, 10> Dims3Stack;
    for (int I = 0; I < NLevels; ++I) {
      FLiftCdf53X<f64>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), BlockSize3, lift_option::Normal, &Vol);
      Dims3Stack[I] = P3 + 1;
      P3 = (P3 + 1) / 2;
      D3 = (R3 + 1) / 2;
      R3 = D3 + IsEven(D3);
      S3 = S3 * 2;
    }
    for (int I = NLevels - 1; I >= 0; --I) {
      S3 = S3 / 2;
      ILiftCdf53X<f64>(grid(v3i(0), Dims3Stack[I], S3), BlockSize3, lift_option::Normal, &Vol);
    }
    for (int I = 0; I < BlockSize3.X; ++I) {
      printf("%f ", Vol.At<f64>(I));
    }
    printf("\n");
  }
  // 2D
  {
    v3i D3(40, 34, 1); // Dims
    v3i BlockSize3(128, 128, 1);
    v3i P3 = BlockSize3;
    volume Vol(v3i(BlockSize3.XY + 1, 1), dtype::float64);
    Fill(Begin<f64>(Vol), End<f64>(Vol), 0.0);
    volume InputVol;
    idx2_CleanUp(Dealloc(&Vol));
    idx2_CleanUp(Dealloc(&InputVol));
    ReadVolume("/Users/TrAnG/Desktop/Downloads/Small/MIRANDA-PRESSURE-[40-34-1]-Float64.raw", D3, dtype::float64, &InputVol);
    Copy<f64>(grid(D3), InputVol, &Vol);
    int NLevels = Log2Floor(BlockSize3.X);
    v3i R3 = D3 + IsEven(D3);
    v3i S3(1); // Strides
    SetDims(&Vol, v3i(BlockSize3.XY + 1, 1));
    stack_array<v3i, 10> Dims3Stack;
    stack_array<v3<v3i>, 10> DimsV3Stack;
    for (int I = 0; I < NLevels; ++I) {
      FLiftCdf53X<f64>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), BlockSize3, lift_option::Normal, &Vol);
      FLiftCdf53Y<f64>(grid(v3i(0), v3i(R3.X, D3.Y, D3.Z), S3), BlockSize3, lift_option::Normal, &Vol);
      Dims3Stack[I] = v3i(P3.XY + 1, 1);
      P3 = (P3 + 1) / 2;
      D3 = (R3 + 1) / 2;
      R3 = D3 + IsEven(D3);
      S3 = S3 * 2;
    }
    for (int I = NLevels - 1; I >= 0; --I) {
      S3 = S3 / 2;
      ILiftCdf53Y<f64>(grid(v3i(0), Dims3Stack[I], S3), BlockSize3, lift_option::Normal, &Vol);
      ILiftCdf53X<f64>(grid(v3i(0), Dims3Stack[I], S3), BlockSize3, lift_option::Normal, &Vol);
    }
    FILE* Fp = fopen("slice.raw", "wb");
    //WriteVolume(Fp, Vol, extent(Vol));
    fwrite(Vol.Buffer.Data, Vol.Buffer.Bytes, 1, Fp);
    fclose(Fp);
  }
  // 3D
  {
    v3i D3(40, 34, 64); // Dims
    v3i BlockSize3(128, 128, 128);
    v3i P3 = BlockSize3;
    volume Vol(BlockSize3 + 1, dtype::float64);
    Fill(Begin<f64>(Vol), End<f64>(Vol), 0.0);
    volume InputVol;
    idx2_CleanUp(Dealloc(&Vol));
    idx2_CleanUp(Dealloc(&InputVol));
    ReadVolume("/Users/TrAnG/Desktop/Downloads/Small/MIRANDA-PRESSURE-[40-34-64]-Float64.raw", D3, dtype::float64, &InputVol);
    Copy<f64>(grid(D3), InputVol, &Vol);
    int NLevels = Log2Floor(BlockSize3.X);
    v3i R3 = D3 + IsEven(D3);
    v3i S3(1); // Strides
    SetDims(&Vol, BlockSize3 + 1);
    stack_array<v3i, 10> Dims3Stack;
    for (int I = 0; I < NLevels; ++I) {
      FLiftCdf53X<f64>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), BlockSize3, lift_option::Normal, &Vol);
      FLiftCdf53Y<f64>(grid(v3i(0), v3i(R3.X, D3.Y, D3.Z), S3), BlockSize3, lift_option::Normal, &Vol);
      FLiftCdf53Z<f64>(grid(v3i(0), v3i(R3.X, R3.Y, D3.Z), S3), BlockSize3, lift_option::Normal, &Vol);
      Dims3Stack[I] = P3 + 1;
      P3 = (P3 + 1) / 2;
      D3 = (R3 + 1) / 2;
      R3 = D3 + IsEven(D3);
      S3 = S3 * 2;
    }
    for (int I = NLevels - 1; I >= 0; --I) {
      S3 = S3 / 2;
      ILiftCdf53Z<f64>(grid(v3i(0), Dims3Stack[I], S3), BlockSize3, lift_option::Normal, &Vol);
      ILiftCdf53Y<f64>(grid(v3i(0), Dims3Stack[I], S3), BlockSize3, lift_option::Normal, &Vol);
      ILiftCdf53X<f64>(grid(v3i(0), Dims3Stack[I], S3), BlockSize3, lift_option::Normal, &Vol);
    }
    FILE* Fp = fopen("volume.raw", "wb");
    fwrite(Vol.Buffer.Data, Vol.Buffer.Bytes, 1, Fp);
    fclose(Fp);
  }
}

// TODO: rewrite the wavelet predict step to not overflow intermediate results
// TODO: figure out the 33x33 matrix and compute its infinity norm
void
TestWaveletQuantize() {
  { // floating point calculations
//    v3i D3(384, 384, 256); // Dims
    v3i D3(33, 33, 33); // Dims
    v3i M3 = D3;
    v3i N3 = D3 + 1;
    volume InputVol(D3, dtype::float64);
    volume Vol(N3, dtype::float64);
    idx2_CleanUp(Dealloc(&Vol));
    idx2_CleanUp(Dealloc(&InputVol));
    ReadVolume("D:/Datasets/3D/Small/MIRANDA-PRESSURE-[33-33-33]-Float64.raw", D3, dtype::float64, &InputVol);
//    ReadVolume("D:/Datasets/3D/Miranda/MIRANDA-VISCOSITY-[384-384-256]-Float64.raw", D3, dtype::float64, &InputVol);
    Copy<f64>(grid(D3), InputVol, &Vol);
    volume Copy;
    Clone(Vol, &Copy);
    int NLevels = 3;
    v3i R3 = D3 + IsEven(D3);
    v3i S3(1); // Strides
    stack_array<v3<v3i>, 10> Dims3Stack;
    for (int I = 0; I < NLevels; ++I) {
      FLiftCdf53X<f64>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &Vol);
      FLiftCdf53Y<f64>(grid(v3i(0), v3i(R3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &Vol);
      FLiftCdf53Z<f64>(grid(v3i(0), v3i(R3.X, R3.Y, D3.Z), S3), M3, lift_option::Normal, &Vol);
      Dims3Stack[I] = v3(v3i(D3.X, D3.Y, D3.Z), v3i(R3.X, D3.Y, D3.Z), v3i(R3.X, R3.Y, D3.Z));
      D3 = (R3 + 1) / 2;
      R3 = D3 + IsEven(D3);
      S3 = S3 * 2;
    }
    for (int I = NLevels - 1; I >= 0; --I) {
      S3 = S3 / 2;
      ILiftCdf53Z<f64>(grid(v3i(0), Dims3Stack[I].Z, S3), M3, lift_option::Normal, &Vol);
      ILiftCdf53Y<f64>(grid(v3i(0), Dims3Stack[I].Y, S3), M3, lift_option::Normal, &Vol);
      ILiftCdf53X<f64>(grid(v3i(0), Dims3Stack[I].X, S3), M3, lift_option::Normal, &Vol);
    }
    printf("floating point psnr = %f\n", PSNR<f64>(grid(D3), Vol, grid(D3), Copy));
  }
  { // integer calculations
//    v3i D3(384, 384, 256); // Dims
    v3i D3(33, 33, 33); // Dims
    v3i M3 = D3;
    v3i N3 = M3 + 1;
    volume InputVol(D3, dtype::float64);
    volume Vol(N3, dtype::float64);
    idx2_CleanUp(Dealloc(&Vol));
    idx2_CleanUp(Dealloc(&InputVol));
    ReadVolume("D:/Datasets/3D/Small/MIRANDA-PRESSURE-[33-33-33]-Float64.raw", D3, dtype::float64, &InputVol);
//    ReadVolume("D:/Datasets/3D/Miranda/MIRANDA-VISCOSITY-[384-384-256]-Float64.raw", D3, dtype::float64, &InputVol);
    Copy<f64>(grid(D3), InputVol, &Vol);
    volume Copy;
    Clone(Vol, &Copy);
    volume IntVol(Dims(Vol), dtype::int64);
    int Bits = 59;
    int EMax = Quantize<f64>(Bits, grid(D3), Vol, grid(D3), &IntVol);
    int NLevels = 3;
    v3i R3 = D3 + IsEven(D3);
    v3i S3(1); // Strides
    stack_array<v3<v3i>, 10> Dims3Stack;
    volume CopyInt(Dims(IntVol), dtype::int64);
    Clone(IntVol, &CopyInt);
    for (int I = 0; I < NLevels; ++I) {
      FLiftCdf53X<i64>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &IntVol);
      FLiftCdf53Y<i64>(grid(v3i(0), v3i(R3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &IntVol);
      FLiftCdf53Z<i64>(grid(v3i(0), v3i(R3.X, R3.Y, D3.Z), S3), M3, lift_option::Normal, &IntVol);
      Dims3Stack[I] = v3(v3i(D3.X, D3.Y, D3.Z), v3i(R3.X, D3.Y, D3.Z), v3i(R3.X, R3.Y, D3.Z));
      D3 = (R3 + 1) / 2;
      R3 = D3 + IsEven(D3);
      S3 = S3 * 2;
    }
    for (int I = NLevels - 1; I >= 0; --I) {
      S3 = S3 / 2;
      ILiftCdf53Z<i64>(grid(v3i(0), Dims3Stack[I].Z, S3), M3, lift_option::Normal, &IntVol);
      ILiftCdf53Y<i64>(grid(v3i(0), Dims3Stack[I].Y, S3), M3, lift_option::Normal, &IntVol);
      ILiftCdf53X<i64>(grid(v3i(0), Dims3Stack[I].X, S3), M3, lift_option::Normal, &IntVol);
    }
    auto MM = MinMaxElem(Begin<f64>(extent(D3), Copy), End<f64>(extent(D3), Copy));
    printf("min %f max %f\n", *(MM.Min), *(MM.Max));
    Dequantize<f64>(EMax, Bits, grid(D3), IntVol, grid(D3), &Vol);
    printf("integer psnr = %f\n", PSNR<f64>(grid(D3), Vol, grid(D3), Copy));
    printf("integer rmse = %.18f\n", RMSError<f64>(grid(D3), Vol, grid(D3), Copy));
    printf("integer psnr = %.18f\n", RMSError<f64>(grid(D3), IntVol, grid(D3), CopyInt));
  }
}

// The range expansion for 3 levels of XYZ is 21 (from 4 to 5 bits)
// For 1 level of X, it is 2 (1 bit) (so 1 level of XYZ needs 3 bits)
// NOTE: this procedure prints the transpose of the wavelet matrix
void
TestWaveletMatrices() {
  {
    v3i D3(7, 1, 1); // Dims
    v3i M3 = D3;
    volume IntVol(D3, dtype::float64);
    idx2_CleanUp(Dealloc(&IntVol));
    FILE* Fp = fopen("matrix.txt", "w");
    idx2_CleanUp(fclose(Fp));
    int NLevels = 1;
    for (int I = 0; I < M3.X; ++I) {
      IntVol = 0.0;
      IntVol.At<f64>(v3i(I, 0, 0)) = 1;
      D3 = M3;
      v3i S3(1); // Strides
      v3i R3 = D3 + IsEven(D3);
      for (int L = 0; L < NLevels; ++L) {
        ILiftCdf53X<f64>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &IntVol);
//        FLiftCdf53Y<i64>(grid(v3i(0), v3i(R3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &IntVol);
//        FLiftCdf53Z<i64>(grid(v3i(0), v3i(R3.X, R3.Y, D3.Z), S3), M3, lift_option::Normal, &IntVol);
        D3 = (R3 + 1) / 2;
        R3 = D3 + IsEven(D3);
        S3 = S3 * 2;
      }
      for (int J = 0; J < M3.X; ++J)
        fprintf(Fp, "%f ", IntVol.At<f64>(v3i(J, 0, 0)));
      fprintf(Fp, "\n");
    }
  }
}

/* TODO: load a real volume from disk
 * TODO: perform the forward transform
 * TODO: introduce some noise to the wavelet coefficients
 * TODO: do the inverse transform
 * TODO: check the error
 */
void
TestWaveletRangeExpansion() {
  v3i D3(65, 1, 1); // Dims
  int NLevels = 5;
  v3i M3 = D3;
  volume FloatVol(D3, dtype::float64);
  pcg32 Pcg;
  idx2_CleanUp(Dealloc(&FloatVol));
  f64 MaxExpansion = 1;
  for (int Iter = 0; Iter < 1000; ++Iter) {
    /* generate a random vector */
    for (int I = 0; I < M3.X; ++I) {
      FloatVol.At<f64>(v3i(I, 0, 0)) = NextDouble(&Pcg);
    }
    /* compute the infinity norm */
    auto NormBefore = fabs(FloatVol.At<f64>(v3i(0, 0, 0)));
    for (int I = 0; I < M3.X; ++I) {
      NormBefore = MAX(NormBefore, fabs(FloatVol.At<f64>(v3i(I, 0, 0))));
    }
    //printf("norm before %f\n", NormBefore);
    D3 = M3;
    v3i S3(1); // Strides
    v3i R3 = D3 + IsEven(D3);
    for (int L = 0; L < NLevels; ++L) {
      FLiftCdf53X<f64>(grid(v3i(0), v3i(D3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &FloatVol);
//      FLiftCdf53Y<i64>(grid(v3i(0), v3i(R3.X, D3.Y, D3.Z), S3), M3, lift_option::Normal, &FloatVol);
//      FLiftCdf53Z<i64>(grid(v3i(0), v3i(R3.X, R3.Y, D3.Z), S3), M3, lift_option::Normal, &FloatVol);
      D3 = (R3 + 1) / 2;
      R3 = D3 + IsEven(D3);
      S3 = S3 * 2;
    }
    /* compute the infinity norm after the transform */
    auto NormAfter = fabs(FloatVol.At<f64>(v3i(0, 0, 0)));
    for (int I = 0; I < M3.X; ++I) {
      NormAfter = MAX(NormAfter, fabs(FloatVol.At<f64>(v3i(I, 0, 0))));
    }
    MaxExpansion = MAX(MaxExpansion, NormAfter / NormBefore);
    //printf("norm after %f\n", NormAfter);
  }
  printf("max expansion is %f\n", MaxExpansion);
}

void
BuildSubbandsTest() {
  {
    v3i N(2);
    int NLevels = 1;
    cstr TransformOrder = "XYZ++";
    array<subband> Subbands;
    BuildSubbands(N, NLevels, EncodeTransformOrder(TransformOrder), &Subbands);
//    printf("\n%lld subbands\n", Size(Subbands));
    for (int I = 0; I < Size(Subbands); ++I) {
//      const subband& S = Subbands[I];
//      printf("From " idx2_PrStrV3i " Dims " idx2_PrStrV3i " Strd " idx2_PrStrV3i " Level " idx2_PrStrV3i " LowHigh " idx2_PrStrV3i "Level %d\n",
//            idx2_PrV3i(From(S.Grid)), idx2_PrV3i(Dims(S.Grid)), idx2_PrV3i(Strd(S.Grid)), idx2_PrV3i(S.Level3), idx2_PrV3i(S.LowHigh3), S.Level);
    }
  }
  {
    v3i N(4);
    int NLevels = 2;
    cstr TransformOrder = "ZYX++";
    array<subband> Subbands;
    BuildSubbands(N, NLevels, EncodeTransformOrder(TransformOrder), &Subbands);
//    printf("\n%lld subbands\n", Size(Subbands));
    for (int I = 0; I < Size(Subbands); ++I) {
//      const subband& S = Subbands[I];
//      printf("From " idx2_PrStrV3i " Dims " idx2_PrStrV3i " Strd " idx2_PrStrV3i " Level " idx2_PrStrV3i " LowHigh " idx2_PrStrV3i "Level %d\n",
//            idx2_PrV3i(From(S.Grid)), idx2_PrV3i(Dims(S.Grid)), idx2_PrV3i(Strd(S.Grid)), idx2_PrV3i(S.Level3), idx2_PrV3i(S.LowHigh3), S.Level);
    }
  }
  {
    v3i N(32);
    int NLevels = 5;
    cstr TransformOrder = "YZX+XYZ++";
    array<subband> Subbands;
    BuildSubbands(N, NLevels, EncodeTransformOrder(TransformOrder), &Subbands);
//    printf("\n%lld subbands\n", Size(Subbands));
    for (int I = 0; I < Size(Subbands); ++I) {
//      const subband& S = Subbands[I];
//      printf("From " idx2_PrStrV3i " Dims " idx2_PrStrV3i " Strd " idx2_PrStrV3i " Level " idx2_PrStrV3i " LowHigh " idx2_PrStrV3i "Level %d\n",
//            idx2_PrV3i(From(S.Grid)), idx2_PrV3i(Dims(S.Grid)), idx2_PrV3i(Strd(S.Grid)), idx2_PrV3i(S.Level3), idx2_PrV3i(S.LowHigh3), S.Level);
    }
  }
  {
    v3i N(16);
    int NLevels = 4;
    cstr TransformOrder = "XY++";
    array<subband> Subbands;
    BuildSubbands(N, NLevels, EncodeTransformOrder(TransformOrder), &Subbands);
//    printf("\n%lld subbands\n", Size(Subbands));
    for (int I = 0; I < Size(Subbands); ++I) {
//      const subband& S = Subbands[I];
//      printf("From " idx2_PrStrV3i " Dims " idx2_PrStrV3i " Strd " idx2_PrStrV3i " Level " idx2_PrStrV3i " LowHigh " idx2_PrStrV3i "Level %d\n",
//            idx2_PrV3i(From(S.Grid)), idx2_PrV3i(Dims(S.Grid)), idx2_PrV3i(Strd(S.Grid)), idx2_PrV3i(S.Level3), idx2_PrV3i(S.LowHigh3), S.Level);
    }
  }
  {
    v3i N(16);
    int NLevels = 4;
    cstr TransformOrder = "X+Y+Z++";
    array<subband> Subbands;
    BuildSubbands(N, NLevels, EncodeTransformOrder(TransformOrder), &Subbands);
//    printf("\n%lld subbands\n", Size(Subbands));
    for (int I = 0; I < Size(Subbands); ++I) {
//      const subband& S = Subbands[I];
//      printf("From " idx2_PrStrV3i " Dims " idx2_PrStrV3i " Strd " idx2_PrStrV3i " Level " idx2_PrStrV3i " LowHigh " idx2_PrStrV3i "Level %d\n",
//            idx2_PrV3i(From(S.Grid)), idx2_PrV3i(Dims(S.Grid)), idx2_PrV3i(Strd(S.Grid)), idx2_PrV3i(S.Level3), idx2_PrV3i(S.LowHigh3), S.Level);
    }
  }
  {
    v3i N(8, 8, 1);
    int NLevels = 2;
    cstr TransformOrder = "XY++";
    array<subband> Subbands;
    BuildSubbands(N, NLevels, EncodeTransformOrder(TransformOrder), &Subbands);
//    printf("\n%lld subbands\n", Size(Subbands));
    for (int I = 0; I < Size(Subbands); ++I) {
//      const subband& S = Subbands[I];
//      printf("From " idx2_PrStrV3i " Dims " idx2_PrStrV3i " Strd " idx2_PrStrV3i " Level " idx2_PrStrV3i " LowHigh " idx2_PrStrV3i "Level %d\n",
//            idx2_PrV3i(From(S.Grid)), idx2_PrV3i(Dims(S.Grid)), idx2_PrV3i(Strd(S.Grid)), idx2_PrV3i(S.Level3), idx2_PrV3i(S.LowHigh3), S.Level);
    }
  }
  {
    v3i N(32, 32, 16);
    int NLevels = 4;
    cstr TransformOrder = "X+Y+XYZ++";
    array<subband> Subbands;
    BuildSubbands(N, NLevels, EncodeTransformOrder(TransformOrder), &Subbands);
//    printf("\n%lld subbands\n", Size(Subbands));
    for (int I = 0; I < Size(Subbands); ++I) {
//      const subband& S = Subbands[I];
//      printf("From " idx2_PrStrV3i " Dims " idx2_PrStrV3i " Strd " idx2_PrStrV3i " Level " idx2_PrStrV3i " LowHigh " idx2_PrStrV3i "Level %d\n",
//            idx2_PrV3i(From(S.Grid)), idx2_PrV3i(Dims(S.Grid)), idx2_PrV3i(Strd(S.Grid)), idx2_PrV3i(S.Level3), idx2_PrV3i(S.LowHigh3), S.Level);
    }
  }
}

void
TestDecodeTransformOrder() {
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "XYZ++";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrder) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "ZYX++";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrder) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "YZX+XYZ++";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrder) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "XY++";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrder) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "X+Y+Z++";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrder) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "X+Y+XYZ++";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrder) == 0);
  }
}

void
TestDecodeTransformOrderFull() {
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "XYZ++";
    v3i N3(4);
    cstr TransformOrderFull = "XYZXYZ";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), N3, TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrderFull) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "YZX+XYZ++";
    v3i N3(8);
    cstr TransformOrderFull = "YZXXYZXYZ";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), N3, TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrderFull) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "ZZZ+XY++";
    v3i N3(4, 4, 8);
    cstr TransformOrderFull = "ZZZXYXY";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), N3, TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrderFull) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "X+Y+Z++";
    v3i N3(2, 2, 8);
    cstr TransformOrderFull = "XYZZZ";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), N3, TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrderFull) == 0);
  }
  {
    char TransformOrderStr[128];
    cstr TransformOrder = "XYZ++";
    v3i N3(2, 2, 2);
    cstr TransformOrderFull = "XYZ";
    DecodeTransformOrder(EncodeTransformOrder(TransformOrder), N3, TransformOrderStr);
    idx2_Assert(strcmp(TransformOrderStr, TransformOrderFull) == 0);
  }
}

/* *4 - 1, *4 - 33*/
void
TestWavNorms() {
//  wav_basis_norms BasisNorms = GetCdf53Norms(8);
//  printf("scaling norms:\n");
//  for (int I = 0; I < Size(BasisNorms.ScalNorms); ++I) {
//    printf("%.20f\n", BasisNorms.ScalNorms[I]);
//  }
//  printf("wavelet norms:\n");
//  for (int I = 0; I < Size(BasisNorms.ScalNorms); ++I) {
//    printf("%.20f\n", BasisNorms.WaveNorms[I]);
//  }
  wav_basis_norms_static<10> BasisNormsStatic = GetCdf53NormsFast<10>();
//  printf("scaling norms:\n");
  for (int I = 0; I < Size(BasisNormsStatic.ScalNorms); ++I) {
//    printf("%.20f\n", BasisNormsStatic.ScalNorms[I]);
  }
//  printf("wavelet norms:\n");
  for (int I = 0; I < Size(BasisNormsStatic.ScalNorms); ++I) {
//    printf("%.20f\n", BasisNormsStatic.WaveNorms[I]);
  }
}

void
TestWaveletNormalize() {
  {
    v3i Dims3(33, 33, 33);
    cstr TransformOrder = "XYZ++";
    volume Vol(Dims3, dtype::float64);
    auto Ok = ReadVolume("./data/MIRANDA-PRESSURE-[33-33-33]-Float64.raw", Dims3, dtype::float64, &Vol);
    if (!Ok) {
      printf("%s\n", ToString(Ok));
      idx2_Assert(false);
    }
    volume VolClone;
    Clone(Vol, &VolClone);
    ExtrapolateCdf53(Dims3, EncodeTransformOrder(TransformOrder), &Vol);
    int NLevels = 5;
    ForwardCdf53(Dims3, Dims3, 0, NLevels, EncodeTransformOrder(TransformOrder), &Vol, true);
    InverseCdf53(Dims3, Dims3, 0, NLevels, EncodeTransformOrder(TransformOrder), &Vol, true);
//    WriteVolume(idx2_PrintScratch("out-%d-%d-%d-float64.raw", N3.X, N3.Y, N3.Z), Vol);
    f64 Ps = PSNR<f64>(grid(Dims3), VolClone, grid(Dims3), Vol);
    idx2_Assert(Ps > 300);
//    printf("psnr = %f\n", Ps);
  }
}

/* numerator: f[i] = 4 f[i-1] - 1 */
static const double sweight[] = {
   1.22474487139158904909, /* sqrt(3/2) */
   1.65831239517769992455, /* sqrt(11/4) */
   2.31840462387392593812, /* sqrt(43/8) */
   3.26917420765550516417, /* sqrt(171/16) */
   4.61992965314408223619, /* sqrt(683/32) */
   6.53237131522695975412, /* sqrt(2731/64) */
   9.23774526061419383627, /* sqrt(10923/128) */
  13.06399512974495868860, /* sqrt(43691/256) */
};

/* numerator: f[i] = 4 f[i-1] - 33 */
static const double wweight[] = {
   0.84779124789065851738, /* sqrt(23/32) */
   0.96014321848357602197, /* sqrt(59/64) */
   1.25934010497561777602, /* sqrt(203/128) */
   1.74441071711910781874, /* sqrt(779/256) */
   2.45387130367507252448, /* sqrt(3083/512) */
   3.46565176950887551229, /* sqrt(12299/1024) */
   4.89952763985978550372, /* sqrt(49163/2048) */
   6.92839704021608449852, /* sqrt(196619/4096) */
};

/* forward lift in x */
static int
fliftx(double* f, uint nx, uint ny, uint nz, uint lx, uint ly, uint lz)
{
  uint mx = nx >> lx;
  uint my = ny >> ly;
  uint mz = nz >> lz;
  double* t = (double*)malloc(mx * sizeof(double));
  double s = sweight[lx] / (lx ? sweight[lx - 1] : 1);
  double w = wweight[lx] / (lx ? sweight[lx - 1] : 1);
//  double s = 1;
//  double w = 1;
  uint x, y, z;

  if (!t)
    return 0;

  /* w-lift */
  for (z = 0; z < mz; z++)
    for (y = 0; y < my; y++)
      for (x = 1; x < mx; x += 2) {
        uint xmin = x - 1;
        uint xmax = (x == mx - 1 ? x - 1 : x + 1);
        f[x + nx * (y + ny * z)] -= f[xmin + nx * (y + ny * z)] / 2;
        f[x + nx * (y + ny * z)] -= f[xmax + nx * (y + ny * z)] / 2;
      }
  /* s-lift */
  for (z = 0; z < mz; z++)
    for (y = 0; y < my; y++)
      for (x = 1; x < mx; x += 2) {
        uint xmin = x - 1;
        uint xmax = (x == mx - 1 ? x - 1 : x + 1);
        f[xmin + nx * (y + ny * z)] += f[x + nx * (y + ny * z)] / 4;
        f[xmax + nx * (y + ny * z)] += f[x + nx * (y + ny * z)] / 4;
      }
  /* scale and reorder */
  for (z = 0; z < mz; z++)
    for (y = 0; y < my; y++) {
      for (x = 0; x < mx; x++)
        f[x + nx * (y + ny * z)] *= (x & 1 ? w : s);
    }
  free(t);
  return 1;
}

/* forward lift in y */
static int
flifty(double* f, uint nx, uint ny, uint nz, uint lx, uint ly, uint lz)
{
  uint mx = nx >> lx;
  uint my = ny >> ly;
  uint mz = nz >> lz;
  double* t = (double*)malloc(my * sizeof(double));
  double s = sweight[ly] / (ly ? sweight[ly - 1] : 1);
  double w = wweight[ly] / (ly ? sweight[ly - 1] : 1);
//  double s = 1;
//  double w = 1;
  uint x, y, z;

  if (!t)
    return 0;

  /* w-lift */
  for (z = 0; z < mz; z++)
    for (x = 0; x < mx; x++)
      for (y = 1; y < my; y += 2) {
        uint ymin = y - 1;
        uint ymax = (y == my - 1 ? y - 1 : y + 1);
        f[x + nx * (y + ny * z)] -= f[x + nx * (ymin + ny * z)] / 2;
        f[x + nx * (y + ny * z)] -= f[x + nx * (ymax + ny * z)] / 2;
      }
  /* s-lift */
  for (z = 0; z < mz; z++)
    for (x = 0; x < mx; x++)
      for (y = 1; y < my; y += 2) {
        uint ymin = y - 1;
        uint ymax = (y == my - 1 ? y - 1 : y + 1);
        f[x + nx * (ymin + ny * z)] += f[x + nx * (y + ny * z)] / 4;
        f[x + nx * (ymax + ny * z)] += f[x + nx * (y + ny * z)] / 4;
      }
  /* scale and reorder */
  for (z = 0; z < mz; z++)
    for (x = 0; x < mx; x++) {
      for (y = 0; y < my; y++)
        f[x + nx * (y + ny * z)] *= (y & 1 ? w : s);
    }
  free(t);
  return 1;
}

/* forward lift in z */
static int
fliftz(double* f, uint nx, uint ny, uint nz, uint lx, uint ly, uint lz)
{
  uint mx = nx >> lx;
  uint my = ny >> ly;
  uint mz = nz >> lz;
  double* t = (double*)malloc(mz * sizeof(double));
  double s = sweight[lz] / (lz ? sweight[lz - 1] : 1);
  double w = wweight[lz] / (lz ? sweight[lz - 1] : 1);
//  double s = 1;
//  double w = 1;
  uint x, y, z;

  if (!t)
    return 0;

  /* w-lift */
  for (y = 0; y < my; y++)
    for (x = 0; x < mx; x++)
      for (z = 1; z < mz; z += 2) {
        uint zmin = z - 1;
        uint zmax = (z == mz - 1 ? z - 1 : z + 1);
        f[x + nx * (y + ny * z)] -= f[x + nx * (y + ny * zmin)] / 2;
        f[x + nx * (y + ny * z)] -= f[x + nx * (y + ny * zmax)] / 2;
      }
  /* s-lift */
  for (y = 0; y < my; y++)
    for (x = 0; x < mx; x++)
      for (z = 1; z < mz; z += 2) {
        uint zmin = z - 1;
        uint zmax = (z == mz - 1 ? z - 1 : z + 1);
        f[x + nx * (y + ny * zmin)] += f[x + nx * (y + ny * z)] / 4;
        f[x + nx * (y + ny * zmax)] += f[x + nx * (y + ny * z)] / 4;
      }
  /* scale and reorder */
  for (y = 0; y < my; y++)
    for (x = 0; x < mx; x++) {
      for (z = 0; z < mz; z++)
        f[x + nx * (y + ny * z)] *= (z & 1 ? w : s);
    }
  free(t);
  return 1;
}

void
TestWaveletNormalize2() {
  {
    v3i Dims3(33, 33, 33);
    cstr TOrder = "XYZ++";
    volume Vol(Dims3, dtype::float64);
    auto Ok = ReadVolume("./data/MIRANDA-PRESSURE-[33-33-33]-Float64.raw", Dims3, dtype::float64, &Vol);
    if (!Ok) {
      printf("%s\n", ToString(Ok));
      idx2_Assert(false);
    }
    volume VolClone;
    Clone(Vol, &VolClone);
//    ExtrapolateCdf53(Dims3, EncodeTransformOrder(TOrder), &Vol);
    int NLevels = 1;
    ForwardCdf53(Dims3, Dims3, 0, NLevels, EncodeTransformOrder(TOrder), &Vol, true);
//    InverseCdf53(Dims3, Dims3, NLevels, EncodeTransformOrder(TOrder), &Vol, true);
//    WriteVolume(idx2_PrintScratch("out-%d-%d-%d-float64.raw", N3.X, N3.Y, N3.Z), Vol);
    for (int I = 0; I < NLevels; ++I) {
      fliftx((double*)VolClone.Buffer.Data, Dims3.X, Dims3.Y, Dims3.Z, I, I, I);
      flifty((double*)VolClone.Buffer.Data, Dims3.X, Dims3.Y, Dims3.Z, I, I, I);
      fliftz((double*)VolClone.Buffer.Data, Dims3.X, Dims3.Y, Dims3.Z, I, I, I);
    }
    f64 Ps = PSNR<f64>(grid(Dims3), VolClone, grid(Dims3), Vol);
//    idx2_Assert(Ps > 300);
    printf("psnr = %f\n", Ps);
  }
}

idx2_RegisterTest(TestWavelet)
// idx2_RegisterTest(TestWaveletBlock)
idx2_RegisterTest(TestWavGrid)
idx2_RegisterTest(TestWavNorms)
idx2_RegisterTest(BuildSubbandsTest)
//idx2_RegisterTest(TestWaveletNormalize)
//idx2_RegisterTest(TestWaveletNormalize2)
idx2_RegisterTest(TestDecodeTransformOrder)
idx2_RegisterTest(TestDecodeTransformOrderFull)
//idx2_RegisterTest(TestWaveletQuantize)
idx2_RegisterTest(TestWaveletMatrices)
idx2_RegisterTest(TestWaveletRangeExpansion)
//idx2_RegisterTest(TestWaveletExtrapolation)
