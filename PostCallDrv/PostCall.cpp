#include "Pch.h"
#include "KUtils.h"
#include "ShareData.h"
#include "Mapper.h"
#include "Pe.h"


extern "C" {
#pragma data_seg(".data")
	__declspec(dllexport) PostCallMapperContext g_PostCallMapperContext = {};
#pragma data_seg()
}
	

void Run(void* params)
{
	Sleep(30 * 1000);

	KIRQL originalIrql;
	KeRaiseIrql(PASSIVE_LEVEL, &originalIrql);
	MapSysFile();
	KeLowerIrql(originalIrql);


	while (1)
	{
		DbgPrintEx(77, 0, "Voyager-Next Is Success Load... \n");
		Sleep(30 * 1000);
	}
	
}


unsigned long long PostCall_Main()
{

	if(!g_PostCallMapperContext.KernelModuleBase || !g_PostCallMapperContext.KeQueryPerformanceCounterFunAddress)
	{
		return 0;
	}


	UINT64 RetuenVal = Call::CallFunctionAddress<LARGE_INTEGER, PLARGE_INTEGER>(g_PostCallMapperContext.KeQueryPerformanceCounterFunAddress, (PLARGE_INTEGER)g_PostCallMapperContext.KeQueryPerformanceCounterParam).QuadPart;

	HANDLE handle;
	PsCreateSystemThread(&handle, 0, 0, 0, 0,&Run, 0);
	ZwClose(handle);
	return RetuenVal;
}