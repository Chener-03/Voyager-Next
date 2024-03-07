#pragma once
#include "Defs.h"



typedef struct _MapFileStateCtx {
	unsigned short path[255];
	FILE_BASIC_INFORMATION fbi;
} MapFileStateCtx, * PMapFileStateCtx;

typedef struct _MapperContext
{
	//magic 
	int test;

	// smbois
	int RunWithSpoofer;
	int RandomSeed;

	// postcall sys data
	char PostCallSysData[1 * 1024 * 1024L]; //1MB

	// postcall启动后map第三方sys路径
	char* postCallLoadSysPath[255];

	// hyperv data
	char hvPayloadData[1 * 1024 * 1024L]; //1MB

	// 还原文件时间
	MapFileStateCtx bootmgfw;			// \EFI\Microsoft\Boot\bootmgfw.efi
	MapFileStateCtx bootParent;			// \EFI\Microsoft
	MapFileStateCtx boot;				// \EFI\Microsoft\Boot
	// 还原文件
	unsigned short bootmgfwBackupPath[255];
}MapperContext,*PMapperContext;


// post call 专用 
typedef struct _PostCallMapperContext
{

	void* KernelModuleBase;
	void* KeQueryPerformanceCounterFunAddress;
	void* KeQueryPerformanceCounterParam;

	// postcall启动后map第三方sys路径
	char* postCallLoadSysPath[255];

}PostCallMapperContext, * PPostCallMapperContext;

// hyper-v 专用
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



#pragma data_seg(".data")
__declspec(dllexport) extern MapperContext g_MapperContext;
#pragma data_seg()