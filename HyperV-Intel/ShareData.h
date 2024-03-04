#pragma once
#include "Defs.h"



#pragma pack(push, 1)
typedef struct _HvContext
{
	UINT64 VmExitHandlerRva;
	UINT64 HypervModuleBase;
	UINT64 HypervModuleSize;
	UINT64 ModuleBase;
	UINT64 ModuleSize;
} HvContext, * PHvContext;
#pragma pack(pop)


extern "C" {
#pragma data_seg(".data")
	__declspec(dllexport) extern HvContext g_HvContext;
#pragma data_seg()
}
