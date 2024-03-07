#include "Pch.h"
#include "KUtils.h"
#include "ShareData.h"



extern "C" {
#pragma data_seg(".data")
	__declspec(dllexport) PostCallMapperContext g_PostCallMapperContext = {};
#pragma data_seg()
}


void Run(void* params)
{
	Sleep(30 * 1000);

	g_PostCallMapperContext.postCallLoadSysPath;

	while (1)
	{
		DbgPrintEx(77, 0, "123\n");
		Sleep(1 * 1000);
	}
	
}


unsigned long long PostCall_Main()
{

	if(!g_PostCallMapperContext.KernelModuleBase || !g_PostCallMapperContext.KeQueryPerformanceCounterFunAddress)
	{
		// Î´Ìî³äº¯ÊýµØÖ·....
		return 0;
	}


	UINT64 RetuenVal = Call::CallFunctionAddress<LARGE_INTEGER, PLARGE_INTEGER>(g_PostCallMapperContext.KeQueryPerformanceCounterFunAddress, (PLARGE_INTEGER)g_PostCallMapperContext.KeQueryPerformanceCounterParam).QuadPart;

	HANDLE handle;
	PsCreateSystemThread(&handle, 0, 0, 0, 0,&Run, 0);
	ZwClose(handle);
	return RetuenVal;
}