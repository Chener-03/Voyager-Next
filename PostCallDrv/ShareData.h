#pragma once
#include "Pch.h"

typedef struct _MapFileStateCtx {
	unsigned int size;
	unsigned int name_off;
	unsigned short path[255];
	unsigned int unpatch_offs[64];
	unsigned char nt_headers[0x1000];
	FILE_BASIC_INFORMATION fbi;
}MapFileStateCtx, * PMapFileStateCtx;

typedef struct _PostCallMapperContext
{

	void* KernelModuleBase;
	void* KeQueryPerformanceCounterFunAddress;
	void* KeQueryPerformanceCounterParam;

}PostCallMapperContext, * PPostCallMapperContext;


extern "C" {

#pragma data_seg(".data")
	__declspec(dllexport) extern PostCallMapperContext g_PostCallMapperContext;
#pragma data_seg()

}