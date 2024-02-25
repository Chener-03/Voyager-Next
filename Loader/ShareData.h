#pragma once
#include "Pch.h"



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

	// hyperv data
	char hvPayloadData[1 * 1024 * 1024L]; //1MB

	// 还原文件时间
	MapFileStateCtx bootmgfw;			// \EFI\Microsoft\Boot\bootmgfw.efi
	MapFileStateCtx bootParent;			// \EFI\Microsoft
	MapFileStateCtx boot;				// \EFI\Microsoft\Boot
	// 还原文件
	unsigned short bootmgfwBackupPath[255];
}MapperContext, * PMapperContext;


extern "C" {
#pragma data_seg(".data")
	__declspec(dllexport) extern MapperContext g_MapperContext;
#pragma data_seg()
}
