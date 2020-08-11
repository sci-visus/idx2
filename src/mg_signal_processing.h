#pragma once

#include "math.h"
#include "mg_assert.h"
#include "mg_common.h"
#include "mg_macros.h"
#include "mg_memory.h"

namespace mg {

struct grid;
struct volume;

/* Computing errors between two functions */
mg_T(t) f64 SqError(const buffer_t<t>& FBuf, const buffer_t<t>& GBuf);
mg_T(t) f64 SqError(const grid& FGrid, const volume& FVol, const grid& GGrid, const volume& GVol);
mg_T(t) f64 SqError(const volume& FVol, const volume& GVol);

mg_T(t) f64 RMSError(const buffer_t<t>& FBuf, const buffer_t<t>& GBuf);
mg_T(t) f64 RMSError(const grid& FGrid, const volume& FVol, const grid& GGrid, const volume& GVol);
mg_T(t) f64 RMSError(const volume& FVol, const volume& GVol);

mg_T(t) f64 PSNR(const buffer_t<t>& FBuf, const buffer_t<t>& GBuf);
mg_T(t) f64 PSNR(const grid& FGrid, const volume& FVol, const grid& GGrid, const volume& GVol);
mg_T(t) f64 PSNR(const volume& FVol, const volume& GVol);

/* Negabinary */
mg_TTi(t, u) void
FwdNegaBinary(const buffer_t<t>& SBuf, buffer_t<u>* DBuf) {
  mg_Assert(is_signed<t>::Value);
  mg_Assert((is_same_type<typename traits<t>::unsigned_t, u>::Value));
  mg_Asser(SBuf.Size == DBuf->Size);
  const auto Mask = traits<u>::NBinaryMask;
  mg_For(int, I, 0, Size(SBuf)) (*DBuf)[I] = u((SBuf[I] + Mask) ^ Mask);
}

mg_T(t) void FwdNegaBinary(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol);
void FwdNegaBinary(const volume& SVol, volume* DVol);

mg_TTi(t, u) void
InvNegaBinary(const buffer_t<u>& SBuf, buffer_t<t>* DBuf) {
  mg_Assert(is_signed<t>::Value);
  mg_Assert((is_same_type<typename traits<t>::unsigned_t, u>::Value));
  mg_Assert(SBuf.Size == DBuf->Size);
  auto Mask = traits<u>::NBinaryMask;
  mg_For(int, I, 0, Size(SBuf)) (*DBuf)[I] = t((SBuf[I] ^ Mask) - Mask);
}

mg_T(t) void InvNegaBinary(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol);
void InvNegaBinary(const volume& SVol, volume* DVol);

/* Quantization */
mg_TTi(t, u) int
Quantize(int Bits, const buffer_t<t>& SBuf, buffer_t<u>* DBuf) {
  mg_Assert(is_floating_point<t>::Value);
  mg_Assert(is_integral<u>::Value);
  mg_Assert(mg_BitSizeOf(t) >= Bits);
  mg_Assert(mg_BitSizeOf(u) >= Bits);
  mg_Assert(SBuf.Size == DBuf->Size);
  t MaxAbs = 0;
  mg_For(int, I, 0, Size(SBuf)) MaxAbs = Max(MaxAbs, (t)fabs(SBuf[I]));
  int EMax = Exponent(MaxAbs);
  f64 Scale = ldexp(1, Bits - 1 - EMax);
  mg_For(int, I, 0, Size(SBuf)) (*DBuf)[I] = u(Scale * SBuf[I]);
  return EMax;
}

mg_TTi(t, u) int
QuantizeF32(int Bits, const buffer_t<t>& SBuf, buffer_t<u>* DBuf) {
  mg_Assert(is_floating_point<t>::Value);
  mg_Assert(is_integral<u>::Value);
  mg_Assert(mg_BitSizeOf(t) >= Bits);
  mg_Assert(mg_BitSizeOf(u) >= Bits);
  mg_Assert(SBuf.Size == DBuf->Size);
  t MaxAbs = 0;
  mg_For(int, I, 0, Size(SBuf)) MaxAbs = Max(MaxAbs, (t)fabs(SBuf[I]));
  int EMax = Exponent<f32>((f32)MaxAbs);
  f64 Scale = ldexp(1, Bits - 1 - EMax);
  mg_For(int, I, 0, Size(SBuf)) (*DBuf)[I] = u(Scale * SBuf[I]);
  return EMax;
}

mg_TTi(t, u) int
QuantizeF64(int Bits, const buffer_t<t>& SBuf, buffer_t<u>* DBuf) {
  mg_Assert(is_floating_point<t>::Value);
  mg_Assert(is_integral<u>::Value);
  mg_Assert(mg_BitSizeOf(t) >= Bits);
  mg_Assert(mg_BitSizeOf(u) >= Bits);
  mg_Assert(SBuf.Size == DBuf->Size);
  t MaxAbs = 0;
  mg_For(int, I, 0, Size(SBuf)) MaxAbs = Max(MaxAbs, (t)fabs(SBuf[I]));
  int EMax = Exponent<f64>((f64)MaxAbs);
  f64 Scale = ldexp(1, Bits - 1 - EMax);
  mg_For(int, I, 0, Size(SBuf)) (*DBuf)[I] = u(Scale * SBuf[I]);
  return EMax;
}

mg_T(t) int Quantize(int Bits, const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol);
int Quantize(int Bits, const volume& SVol, volume* DVol);

mg_TTi(t, u) void
Dequantize(int EMax, int Bits, const buffer_t<t>& SBuf, buffer_t<u>* DBuf) {
  mg_Assert(is_integral<t>::Value);
  mg_Assert(is_floating_point<u>::Value);
  mg_Assert(mg_BitSizeOf(t) >= Bits);
  mg_Assert(mg_BitSizeOf(u) >= Bits);
  mg_Assert(SBuf.Size == DBuf->Size);
  f64 Scale = 1.0 / ldexp(1, Bits - 1 - EMax);
  mg_For(int, I, 0, Size(SBuf)) (*DBuf)[I] = (Scale * SBuf[I]);
}

mg_T(t) void Dequantize(int EMax, int Bits, const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol);
void Dequantize(int EMax, int Bits, const volume& SVol, volume* DVol);

/* Convert the type */
mg_T(t) void ConvertType(const grid& SGrid, const volume& SVol, const grid& DGrid, volume* DVol);
void ConvertType(const volume& SVol, volume* DVol);

mg_T(t) f64 Norm(const t& Begin, const t& End);
mg_T(c) void Upsample(const c& In, c* Out);
mg_T(c) void Convolve(const c& F, const c& G, c* H);

} // namespace mg

