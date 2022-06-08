#include <idx2Lib.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/tensor.h>
#include <string>


namespace nb = nanobind;


using namespace nb::literals;


///* First3, Dims3, Strd3 */
//std::tuple<int, int, int, int, int, int, int, int, int>
//GetOutputGrid()
//{
//}


/* Return a 3D tensor storing an extent */
nb::tensor<nb::numpy, float, nb::shape<nb::any, nb::any, nb::any>>
DecodeExtent3f32(const std::string& InputFile,
                 const std::string& InputPath,
                 std::tuple<int, int, int, int, int, int>& Extent,
                 int Level,    // Level 0 is the finest, level 1 is half of level 0 in each dimension, etc
                 int SubLevel, // from 0 to 7 (7 is the finest)
                 double Accuracy) // 0 is "lossless"
{
  using namespace idx2;
  using namespace std;

  params P;
  P.InputFile = InputFile.c_str(); // name of data set and field
  P.InDir = InputPath.c_str();     // the directory containing the InputFile

  idx2_file Idx2;
  idx2_CleanUp(Dealloc(&Idx2)); // clean up Idx2 automatically in case of error
  Init(&Idx2, P);               // TODO: throw exception

  P.OutputLevel = Level;
  P.DecodeLevel = P.OutputLevel;  // most of the time we want this to be the same as OutputLevel
  P.DecodeMask = (1 << SubLevel); // controls the exact sub-level to extract (by default is 128)
  P.DecodeAccuracy = Accuracy;
  v3i From3(get<0>(Extent), get<1>(Extent), get<2>(Extent));
  v3i To3(get<3>(Extent), get<4>(Extent), get<5>(Extent));
  P.DecodeExtent = idx2::extent(From3, To3);
  //P.DecodeExtent = extent(Idx2.Dims3); // get the whole volume
  // P.DecodeExtent = extent(v3i(10, 20, 30), v3i(100, 140, 160)); // get a portion of the whole
  // volume
  grid OutGrid = idx2::GetOutputGrid(Idx2, P);

  buffer OutBuf; // buffer to store the output
  // idx2_CleanUp(DeallocBuf(&OutBuf)); // deallocate OutBuf automatically in case of error
  AllocBuf(&OutBuf, Prod<i64>(Dims(OutGrid)) * SizeOf(Idx2.DType));
  idx2::Decode(&Idx2, P, &OutBuf); // TODO: throw exception

  v3i D3 = Dims(OutGrid);
  size_t Shape[3] = { (size_t)D3.Z, (size_t)D3.Y, (size_t)D3.X };
  return nb::tensor<nb::numpy, float, nb::shape<nb::any, nb::any, nb::any>>(OutBuf.Data, 3, Shape);
}


/* Return a 3D tensor */
nb::tensor<nb::numpy, float, nb::shape<nb::any, nb::any, nb::any>>
Decode3f32(const std::string& InputFile,
           const std::string& InputPath,
           int Level,    // Level 0 is the finest, level 1 is half of level 0 in each dimension, etc
           int SubLevel, // from 0 to 7 (7 is the finest)
           double Accuracy) // 0 is "lossless"
{
  using namespace idx2;

  params P;
  P.InputFile = InputFile.c_str(); // name of data set and field
  P.InDir = InputPath.c_str();     // the directory containing the InputFile

  idx2_file Idx2;
  idx2_CleanUp(Dealloc(&Idx2)); // clean up Idx2 automatically in case of error
  Init(&Idx2, P);               // TODO: throw exception

  P.OutputLevel = Level;
  P.DecodeLevel = P.OutputLevel;  // most of the time we want this to be the same as OutputLevel
  P.DecodeMask = (1 << SubLevel); // controls the exact sub-level to extract (by default is 128)
  P.DecodeAccuracy = Accuracy;
  P.DecodeExtent = extent(Idx2.Dims3); // get the whole volume
  // P.DecodeExtent = extent(v3i(10, 20, 30), v3i(100, 140, 160)); // get a portion of the whole
  // volume
  grid OutGrid = GetOutputGrid(Idx2, P);

  buffer OutBuf; // buffer to store the output
  // idx2_CleanUp(DeallocBuf(&OutBuf)); // deallocate OutBuf automatically in case of error
  AllocBuf(&OutBuf, Prod<i64>(Dims(OutGrid)) * SizeOf(Idx2.DType));
  idx2::Decode(&Idx2, P, &OutBuf); // TODO: throw exception

  v3i D3 = Dims(OutGrid);
  size_t Shape[3] = { (size_t)D3.Z, (size_t)D3.Y, (size_t)D3.X };
  return nb::tensor<nb::numpy, float, nb::shape<nb::any, nb::any, nb::any>>(OutBuf.Data, 3, Shape);
}


/* Return a 3D tensor */
nb::tensor<nb::numpy, double, nb::shape<nb::any, nb::any, nb::any>>
Decode3f64(const std::string& InputFile,
           const std::string& InputPath,
           int Level,    // Level 0 is the finest, level 1 is half of level 0 in each dimension, etc
           int SubLevel, // from 0 to 7 (7 is the finest)
           double Accuracy) // 0 is "lossless"
{
  using namespace idx2;

  params P;
  P.InputFile = InputFile.c_str(); // name of data set and field
  P.InDir = InputPath.c_str();     // the directory containing the InputFile

  idx2_file Idx2;
  idx2_CleanUp(Dealloc(&Idx2)); // clean up Idx2 automatically in case of error
  auto InitOk = Init(&Idx2, P); // TODO: throw exception
  if (!InitOk)
    printf("ERROR: %s\n", ToString(InitOk));

  P.OutputLevel = Level;
  P.DecodeLevel = P.OutputLevel;  // most of the time we want this to be the same as OutputLevel
  P.DecodeMask = (1 << SubLevel); // controls the exact sub-level to extract (by default is 128)
  P.DecodeAccuracy = Accuracy;
  P.DecodeExtent = extent(Idx2.Dims3); // get the whole volume
  // P.DecodeExtent = extent(v3i(10, 20, 30), v3i(100, 140, 160)); // get a portion of the whole
  // volume
  grid OutGrid = GetOutputGrid(Idx2, P);

  buffer OutBuf; // buffer to store the output
  // idx2_CleanUp(DeallocBuf(&OutBuf)); // deallocate OutBuf automatically in case of error
  AllocBuf(&OutBuf, Prod<i64>(Dims(OutGrid)) * SizeOf(Idx2.DType));

  idx2::Decode(&Idx2, P, &OutBuf); // TODO: throw exception

  v3i D3 = Dims(OutGrid);
  size_t Shape[3] = { (size_t)D3.Z, (size_t)D3.Y, (size_t)D3.X };
  return nb::tensor<nb::numpy, double, nb::shape<nb::any, nb::any, nb::any>>(OutBuf.Data, 3, Shape);
}


/* Return a 2D tensor */
nb::tensor<nb::numpy, float, nb::shape<nb::any, nb::any>>
Decode2f32(const std::string& InputFile,
           const std::string& InputPath,
           int Level,    // Level 0 is the finest, level 1 is half of level 0 in each dimension, etc
           int SubLevel, // from 0 to 7 (7 is the finest)
           double Accuracy) // 0 is "lossless"
{
  using namespace idx2;

  params P;
  P.InputFile = InputFile.c_str(); // name of data set and field
  P.InDir = InputPath.c_str();     // the directory containing the InputFile

  idx2_file Idx2;
  idx2_CleanUp(Dealloc(&Idx2)); // clean up Idx2 automatically in case of error
  Init(&Idx2, P);               // TODO: throw exception

  P.OutputLevel = Level;
  P.DecodeLevel = P.OutputLevel;  // most of the time we want this to be the same as OutputLevel
  P.DecodeMask = (1 << SubLevel); // controls the exact sub-level to extract (by default is 128)
  P.DecodeAccuracy = Accuracy;
  P.DecodeExtent = extent(Idx2.Dims3); // get the whole volume
  // P.DecodeExtent = extent(v3i(10, 20, 30), v3i(100, 140, 160)); // get a portion of the whole
  // volume
  grid OutGrid = GetOutputGrid(Idx2, P);

  buffer OutBuf; // buffer to store the output
  // idx2_CleanUp(DeallocBuf(&OutBuf)); // deallocate OutBuf automatically in case of error
  AllocBuf(&OutBuf, Prod<i64>(Dims(OutGrid)) * SizeOf(Idx2.DType));
  idx2::Decode(&Idx2, P, &OutBuf); // TODO: throw exception

  v3i D3 = Dims(OutGrid);
  size_t Shape[2] = { (size_t)D3.Y, (size_t)D3.X };
  return nb::tensor<nb::numpy, float, nb::shape<nb::any, nb::any>>(OutBuf.Data, 2, Shape);
}


/* Return a 2D tensor of double-precision floating-points */
nb::tensor<nb::numpy, double, nb::shape<nb::any, nb::any>>
Decode2f64(const std::string& InputFile,
           const std::string& InputPath,
           int Level,    // Level 0 is the finest, level 1 is half of level 0 in each dimension, etc
           int SubLevel, // from 0 to 7 (7 is the finest)
           double Accuracy) // 0 is "lossless"
{
  using namespace idx2;

  params P;
  P.InputFile = InputFile.c_str(); // name of data set and field
  P.InDir = InputPath.c_str();     // the directory containing the InputFile

  idx2_file Idx2;
  idx2_CleanUp(Dealloc(&Idx2)); // clean up Idx2 automatically in case of error
  Init(&Idx2, P);               // TODO: throw exception

  P.OutputLevel = Level;
  P.DecodeLevel = P.OutputLevel;  // most of the time we want this to be the same as OutputLevel
  P.DecodeMask = (1 << SubLevel); // controls the exact sub-level to extract (by default is 128)
  P.DecodeAccuracy = Accuracy;
  P.DecodeExtent = extent(Idx2.Dims3); // get the whole volume
  // P.DecodeExtent = extent(v3i(10, 20, 30), v3i(100, 140, 160)); // get a portion of the whole
  // volume
  grid OutGrid = GetOutputGrid(Idx2, P);


  printf("still okay\n");
  buffer OutBuf; // buffer to store the output
  // idx2_CleanUp(DeallocBuf(&OutBuf)); // deallocate OutBuf automatically in case of error
  AllocBuf(&OutBuf, Prod<i64>(Dims(OutGrid)) * SizeOf(Idx2.DType));
  idx2::Decode(&Idx2, P, &OutBuf); // TODO: throw exception

  v3i D3 = Dims(OutGrid);
  size_t Shape[2] = { (size_t)D3.Y, (size_t)D3.X };
  return nb::tensor<nb::numpy, double, nb::shape<nb::any, nb::any>>(OutBuf.Data, 2, Shape);
}


NB_MODULE(idx2Py, M)
{
  M.def("DecodeExtent3f32", DecodeExtent3f32);
  M.def("Decode3f32", Decode3f32);
  M.def("Decode2f32", Decode2f32);
  M.def("Decode3f64", Decode3f64);
  M.def("Decode2f64", Decode2f64);
}
