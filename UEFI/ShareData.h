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

	// postcall������map������sys·��
	char* postCallLoadSysPath[255];

	// hyperv data
	char hvPayloadData[1 * 1024 * 1024L]; //1MB

	// ��ԭ�ļ�ʱ��
	MapFileStateCtx bootmgfw;			// \EFI\Microsoft\Boot\bootmgfw.efi
	MapFileStateCtx bootParent;			// \EFI\Microsoft
	MapFileStateCtx boot;				// \EFI\Microsoft\Boot
	// ��ԭ�ļ�
	unsigned short bootmgfwBackupPath[255];
}MapperContext,*PMapperContext;


// post call ר�� 
typedef struct _PostCallMapperContext
{

	void* KernelModuleBase;
	void* KeQueryPerformanceCounterFunAddress;
	void* KeQueryPerformanceCounterParam;

	// postcall������map������sys·��
	char* postCallLoadSysPath[255];

}PostCallMapperContext, * PPostCallMapperContext;

// hyper-v ר��
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