

namespace idx2 {

// TODO: think of a way that does not require writing a function three times

idx2_T(t) f64
SqError(const buffer_t<t>& FBuf, const buffer_t<t>& GBuf) {
  idx2_Assert(FBuf.Size == GBuf.Size);
  extent Ext(v3i(FBuf.Size, 1, 1));
  return SqError<t>(Ext, volume(FBuf), Ext, volume(GBuf));
}

idx2_T(t) f64
SqError(const grid& FGrid, const volume& FVol, const grid& GGrid, const volume& GVol) {
  idx2_Assert(Dims(FGrid) <= Dims(FVol));
  idx2_Assert(Dims(GGrid) <= Dims(GVol));
  idx2_Assert(Dims(FGrid) == Dims(GGrid));
  auto FIt = Begin<t>(FGrid, FVol), FEnd = End<t>(FGrid, FVol);
  auto GIt = Begin<t>(GGrid, GVol);
  f64 Err = 0;
  for (; FIt != FEnd; ++FIt, ++GIt) {
    f64 Diff = f64(*FIt) - f64(*GIt);
    Err += Diff * Diff;
  }
  return Err;
}

idx2_T(t) f64
SqError(const volume& FVol, const volume& GVol) {
  return SqError<t>(grid(FVol), FVol, grid(GVol), GVol);
}

idx2_T(t) f64
RMSError(const grid& FGrid, const volume& FVol, const grid& GGrid, const volume& GVol) {
  return sqrt(SqError<t>(FGrid, FVol, GGrid, GVol) / Size(FGrid));
}

// idx2_T(t) f64
// RMSError(const buffer_t<t>& FBuf, const buffer_t<t>& GBuf) {
//   idx2_Assert(FBuf.Size == GBuf.Size);
//   grid Ext(v3i(FBuf.Size, 1, 1));
//   return RMSError(Ext, volume(FBuf), Ext, volume(GBuf));
// }

idx2_T(t) f64
RMSError(const volume& FVol, const volume& GVol) {
  return RMSError<t>(grid(FVol), FVol, grid(GVol), GVol);
}

// idx2_T(t) f64
// PSNR(const buffer_t<t>& FBuf, const buffer_t<t>& GBuf) {
//   idx2_Assert(FBuf.Size == GBuf.Size);
//   grid Ext(v3i(FBuf.Size, 1, 1));
//   return PSNR(Ext, volume(FBuf), Ext, volume(GBuf));
// }

idx2_T(t) f64
PSNR(const grid& FGrid, const volume& FVol, const grid& GGrid, const volume& GVol) {
  idx2_Assert(Dims(FGrid) <= Dims(FVol));
  idx2_Assert(Dims(GGrid) <= Dims(GVol));
  idx2_Assert(Dims(FGrid) == Dims(GGrid));
  auto FIt = Begin<t>(FGrid, FVol), FEnd = End<t>(FGrid, FVol);
  auto GIt = Begin<t>(GGrid, GVol);
  f64 Err = 0;
  t MinElem = traits<t>::Max, MaxElem = traits<t>::Min;
  for (; FIt != FEnd; ++FIt, ++GIt) {
    f64 Diff = f64(*FIt) - f64(*GIt);
    Err += Diff * Diff;
    MinElem = Min(MinElem, *FIt);
    MaxElem = Max(MaxElem, *FIt);
  }
  f64 D = 0.5 * (MaxElem - MinElem);
  Err = Err / Size(FGrid);
  return 20.0 * log10(D) - 10.0 * log10(Err);
}

idx2_T(t) f64
PSNR(const volume& FVol, const volume& GVol) {
  return PSNR<t>(grid(FVol), FVol, grid(GVol), GVol);
}

idx2_T(t) void
FwdNegaBinary(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), UnsignedType(SVol.Type));
  idx2_Assert(Dims(SGrid) <= Dims(SVol));
  idx2_Assert(Dims(DGrid) <= Dims(*DVol));
  idx2_Assert(Dims(SGrid) == Dims(DGrid));
  idx2_Assert(IsSigned(SVol.Type));
  idx2_Assert(DVol->Type == UnsignedType(SVol.Type));
  using u = typename traits<t>::unsigned_t;
  auto SIt = Begin<t>(SGrid, SVol), SEnd = End<t>(SGrid, SVol);
  auto DIt = Begin<u>(DGrid, *DVol);
  auto Mask = traits<t>::NBinaryMask;
  for (; SIt != SEnd; ++SIt, ++DIt)
    *DIt = u((*SIt + Mask) ^ Mask);
}

idx2_T(t) void
FwdNegaBinary(const volume& SVol, volume* DVol) {
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), UnsignedType(SVol.Type));
  return FwdNegaBinary<t>(grid(SVol), SVol, grid(*DVol), DVol);
}

idx2_T(t) void
InvNegaBinary(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  idx2_Assert(Dims(SGrid) <= Dims(SVol));
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), SignedType(SVol.Type));
  idx2_Assert(Dims(DGrid) <= Dims(*DVol));
  idx2_Assert(Dims(SGrid) == Dims(DGrid));
  idx2_Assert(IsSigned(DVol->Type));
  idx2_Assert(SVol.Type == UnsignedType(DVol->Type));
  using u = typename traits<t>::unsigned_t;
  auto SIt = Begin<u>(SGrid, SVol), SEnd = End<u>(SGrid, SVol);
  auto DIt = Begin<t>(DGrid, *DVol);
  auto Mask = traits<t>::NBinaryMask;
  for (; SIt != SEnd; ++SIt, ++DIt)
    *DIt = t((*SIt ^ Mask) - Mask);
}

idx2_T(t) void
InvNegaBinary(const volume& SVol, volume* DVol) {
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), SignedType(SVol.Type));
  return InvNegaBinary<t>(grid(SVol), SVol, grid(*DVol), DVol);
}

idx2_T(t) int
Quantize(int Bits, const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  idx2_Assert(Dims(SGrid) <= Dims(SVol));
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), IntType(SVol.Type));
  idx2_Assert(Dims(DGrid) <= Dims(*DVol));
  idx2_Assert(Dims(SGrid) == Dims(DGrid));
  idx2_Assert(IsFloatingPoint(SVol.Type));
  idx2_Assert(IsIntegral(DVol->Type));
  idx2_Assert(BitSizeOf(SVol.Type) >= Bits);
  idx2_Assert(BitSizeOf(DVol->Type) >= Bits);
  auto SIt = Begin<t>(SGrid, SVol), SEnd = End<t>(SGrid, SVol);
  auto DIt = Begin<t>(DGrid, *DVol);
  /* find the max absolute value */
  t MaxAbs = 0;
  for (; SIt != SEnd; ++SIt)
    MaxAbs = Max(MaxAbs, (t)fabs(*SIt));
  int EMax = Exponent(MaxAbs);
  /* quantize */
  f64 Scale = ldexp(1, Bits - 1 - EMax);
  SIt = Begin<t>(SGrid, SVol);
  for (; SIt != SEnd; ++SIt, ++DIt)
    *DIt = t(Scale * *SIt);
  return EMax;
}

idx2_T(t) int
Quantize(int Bits, const volume& SVol, volume* DVol) {
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), IntType(SVol.Type));
  return Quantize<t>(Bits, grid(SVol), SVol, grid(*DVol), DVol);
}

idx2_T(t) void
Dequantize(int EMax, int Bits, const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  idx2_Assert(Dims(SGrid) <= Dims(SVol));
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), FloatType(SVol.Type));
  idx2_Assert(Dims(DGrid) <= Dims(*DVol));
  idx2_Assert(Dims(SGrid) == Dims(DGrid));
  idx2_Assert(BitSizeOf(SVol.Type) >= Bits);
  idx2_Assert(BitSizeOf(DVol->Type) >= Bits);
  idx2_Assert(IsIntegral(SVol.Type));
  idx2_Assert(IsFloatingPoint(DVol->Type));
  auto SIt = Begin<t>(SGrid, SVol), SEnd = End<t>(SGrid, SVol);
  auto DIt = Begin<t>(DGrid, *DVol);
  f64 Scale = 1.0 / ldexp(1, Bits - 1 - EMax);
  for (; SIt != SEnd; ++SIt, ++DIt)
    *DIt = (Scale * *SIt);
}

idx2_T(t) void
Dequantize(int EMax, int Bits, const volume& SVol, volume* DVol) {
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), FloatType(SVol.Type));
  return Dequantize<t>(EMax, Bits, grid(SVol), SVol, grid(*DVol), DVol);
}

idx2_T(t) void
ConvertType(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol) {
  idx2_Assert(DVol->Type != dtype::__Invalid__);
  idx2_Assert(Dims(SGrid) <= Dims(SVol));
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), DVol->Type);
  idx2_Assert(Dims(DGrid) <= Dims(*DVol));
  idx2_Assert(Dims(SGrid) == Dims(DGrid));
  idx2_Assert(SVol.Type != DVol->Type);
  auto SIt = Begin<t>(SGrid, SVol), SEnd = End<t>(SGrid, SVol);
  auto DIt = Begin<t>(DGrid, *DVol);
  for (; SIt != SEnd; ++SIt, ++DIt)
    *DIt = (t)(*SIt);
}

idx2_T(t) void
ConvertType(const volume& SVol, volume* DVol) {
  idx2_Assert(DVol->Type != dtype::__Invalid__);
  if (!DVol->Buffer)
    *DVol = volume(Dims(SVol), DVol->Type);
  return ConvertType<t>(grid(SVol), SVol, grid(*DVol), DVol);
}

// TODO: receive one container
idx2_T(t) f64
Norm(const t& Begin, const t& End) {
  f64 Result = 0;
  for (auto It = Begin; It != End; ++It)
    Result += (*It) * (*It);
  return sqrt(Result);
}

// TODO: use concept to constraint Input and Output
idx2_T(c) void
Upsample(const c& In, c* Out) {
  i64 N = Size(In);
  i64 M = N * 2 - 1;
  Resize(Out, M);
  (*Out)[M - 1] = In[(M - 1) >> 1];
  for (i64 I = M - 3; I >= 0; I -= 2) {
    (*Out)[I    ] = In[I >> 1];
    (*Out)[I + 1] = 0;
  }
}

/* Compute H = F * G */
idx2_T(c) void
Convolve(const c& F, const c& G, c* H) {
  i64 N = Size(F), M = Size(G);
  i64 P = N + M - 1;
  Resize(H, P);
  for (i64 I = 0; I < P; ++I) {
    using type = typename remove_cv_ref<decltype(F[0])>::type;
    type Acc = 0;
    i64 K = Min(N - 1, I);
    i64 L = Max(i64(0), I - M + 1);
    for (i64 J = L; J <= K; ++J)
      Acc += F[J] * G[I - J];
    (*H)[I] = Acc;
  }
}

} // namespace idx2

