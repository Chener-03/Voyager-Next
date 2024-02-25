#include "KUtils.h"

UINT64 AsciiStrCmp(CHAR8* FirstString, CHAR8* SecondString)
{
	while (*FirstString && (*FirstString == *SecondString)) {
		FirstString++;
		SecondString++;
	}
	return (UINT64)(*FirstString - *SecondString);
}



VOID* GetExport(PVOID ModuleBase, CHAR8* Name)
{
	IMAGE_DOS_HEADER* dosHeaders = (IMAGE_DOS_HEADER*)ModuleBase;
	if (dosHeaders->e_magic != IMAGE_DOS_SIGNATURE)
		return NULL;

	IMAGE_NT_HEADERS64* ntHeaders = (IMAGE_NT_HEADERS64*)((UINT64)ModuleBase + dosHeaders->e_lfanew);
	UINT32 exportsRva = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)((UINT64)ModuleBase + exportsRva);
	UINT32* nameRva = (UINT32*)((UINT64)ModuleBase + exports->AddressOfNames);

	for (UINT32 i = 0; i < exports->NumberOfNames; ++i)
	{
		CHAR8* func = (CHAR8*)((UINT64)ModuleBase + nameRva[i]);
		if (AsciiStrCmp(func, Name) == 0)
		{
			UINT32* funcRva = (UINT32*)((UINT64)ModuleBase + exports->AddressOfFunctions);
			UINT16* ordinalRva = (UINT16*)((UINT64)ModuleBase + exports->AddressOfNameOrdinals);
			return (VOID*)(((UINT64)ModuleBase) + funcRva[ordinalRva[i]]);
		}
	}
	return NULL;
}

VOID* GetExportByIndex(PVOID ModuleBase, UINT16 Index)
{
	IMAGE_DOS_HEADER* dosHeaders = (IMAGE_DOS_HEADER*)ModuleBase;
	if (dosHeaders->e_magic != IMAGE_DOS_SIGNATURE)
		return NULL;

	IMAGE_NT_HEADERS64* ntHeaders = (IMAGE_NT_HEADERS64*)((UINT64)ModuleBase + dosHeaders->e_lfanew);
	UINT32 exportsRva = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	IMAGE_EXPORT_DIRECTORY* exports = (IMAGE_EXPORT_DIRECTORY*)((UINT64)ModuleBase + exportsRva);

	if (Index >= exports->NumberOfFunctions)
		return NULL;

	UINT32* funcRva = (UINT32*)((UINT64)ModuleBase + exports->AddressOfFunctions);
	UINT64 exportAddress = ((UINT64)ModuleBase) + funcRva[Index];

	return (VOID*)exportAddress;
}



VOID Sleep(int msec)
{
	LARGE_INTEGER delay;
	delay.QuadPart = -10000i64 * msec;
	KeDelayExecutionThread(KernelMode, false, &delay);
}



PVOID KAlloc(UINT32 Size, bool exec) {

	PVOID ptr = ExAllocatePool2(exec ? NonPagedPool : NonPagedPoolNx, Size, 'KVN');
	if (ptr)
	{
		memset(ptr, 0, Size);
	}
	return ptr;
}

VOID KFree(VOID* Buffer) {
	ExFreePoolWithTag(Buffer, 'KVN');
}




