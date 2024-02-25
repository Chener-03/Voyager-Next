#pragma once
#include "Pch.h"
#include "ShareData.h"


// extern PVOYAGER_T PayLoadDataPtr;


#if WINVER == 2302   // 23H2
// 通过 0F 01 C3 vmresume 查找附近的，应该在下边的...
#define INTEL_VMEXIT_HANDLER_SIG "\xFB\x8B\xD6\x0B\x54\x24\x30\xE8\x00\x00\x00\x00\xE9"
#define INTEL_VMEXIT_HANDLER_MASK "xxxxxxxx????x"

#elif WINVER == 1903
#define INTEL_VMEXIT_HANDLER_SIG "\x48\x8B\x4C\x24\x00\xEB\x07\xE8\x00\x00\x00\x00\xEB\xF2\x48\x8B\x54\x24\x00\xE8\x00\x00\x00\x00\xE9"
#define INTEL_VMEXIT_HANDLER_MASK "xxxx?xxx????xxxxxx?x????x"

#endif

static_assert(sizeof(INTEL_VMEXIT_HANDLER_SIG) == sizeof(INTEL_VMEXIT_HANDLER_MASK), "signature does not match mask size!");

VOID MapHypervImage(VOID* HvImageBase, UINT32 HvImageSize);


