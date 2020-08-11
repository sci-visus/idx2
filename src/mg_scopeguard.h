#pragma once

#include "mg_macros.h"

namespace mg {

mg_T(func_t)
struct scope_guard {
  func_t Func;
  bool Dismissed = false;
  // TODO: std::forward FuncIn?
  scope_guard(const func_t& FuncIn) : Func(FuncIn) {}
  ~scope_guard() { if (!Dismissed) { Func(); } }
};

} // namespace mg

#define mg_BeginCleanUp(...) mg_MacroOverload(mg_BeginCleanUp, __VA_ARGS__)
#define mg_BeginCleanUp_0() auto mg_Cat(__CleanUpFunc__, __LINE__) = [&]()
#define mg_BeginCleanUp_1(N) auto __CleanUpFunc__##N = [&]()
#define mg_EndCleanUp(...) mg_MacroOverload(mg_EndCleanUp, __VA_ARGS__)
#define mg_EndCleanUp_0() mg::scope_guard mg_Cat(__ScopeGuard__, __LINE__)(mg_Cat(__CleanUpFunc__, __LINE__));
#define mg_EndCleanUp_1(N) mg::scope_guard __ScopeGuard__##N(__CleanUpFunc__##N);

//#define mg_BeginCleanUp(n) auto __CleanUpFunc__##n = [&]()
#define mg_CleanUp(...) mg_MacroOverload(mg_CleanUp, __VA_ARGS__)
#define mg_CleanUp_1(...) mg_BeginCleanUp_0() { __VA_ARGS__; }; mg_EndCleanUp_0()
#define mg_CleanUp_2(N, ...) mg_BeginCleanUp_1(N) { __VA_ARGS__; }; mg_EndCleanUp_1(N)

// #define mg_CleanUp(n, ...) mg_BeginCleanUp(n) { __VA_ARGS__; }; mg_EndCleanUp(n)
#define mg_DismissCleanUp(N) { __ScopeGuard__##N.Dismissed = true; }
