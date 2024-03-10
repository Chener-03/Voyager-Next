#include "Pe.h"






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





PIMAGE_NT_HEADERS64 GetNtHeaders(void* ptr)
{
	const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(ptr);

	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
		return nullptr;

	const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS64>(reinterpret_cast<UINT64>(ptr) + dos_header->e_lfanew);

	if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
		return nullptr;

	return nt_headers;
}




VOID FixPeRelocTable(VOID* PeMemPtr)
{
	IMAGE_DOS_HEADER* DosHeader = (IMAGE_DOS_HEADER*)PeMemPtr;
	IMAGE_NT_HEADERS64* NtHeader = (IMAGE_NT_HEADERS64*)((UINT64)PeMemPtr + DosHeader->e_lfanew);


	//计算 delta
	INT64 DeltaOffset = (INT64)((UINT64)PeMemPtr - NtHeader->OptionalHeader.ImageBase);
	if (!DeltaOffset) {
		return;
	}

	// 先找到重定位表
	UINT32 reloc_va = NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	if (!reloc_va)
	{
		return;
	}


	IMAGE_BASE_RELOCATION* current_base_relocation = (IMAGE_BASE_RELOCATION*)(((UINT64)PeMemPtr) + reloc_va);
	IMAGE_BASE_RELOCATION* reloc_end = (IMAGE_BASE_RELOCATION*)((UINT64)current_base_relocation + NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

	while (current_base_relocation < reloc_end && current_base_relocation->SizeOfBlock) {

		UINT32 va = current_base_relocation->VirtualAddress;

		UINT64 address = (UINT64)(PeMemPtr)+va;
		UINT16* item = (UINT16*)((UINT64)(current_base_relocation)+sizeof(IMAGE_BASE_RELOCATION));
		UINT32 count = (current_base_relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(UINT16);

		for (int i = 0; i < count; ++i)
		{
			UINT16 type = item[i] >> 12;
			UINT16 offset = item[i] & 0xFFF;

			if (type == IMAGE_REL_BASED_DIR64)
			{
				*(UINT64*)(address + offset) += DeltaOffset;
			}

		}
		current_base_relocation = (IMAGE_BASE_RELOCATION*)((UINT64)(current_base_relocation)+current_base_relocation->SizeOfBlock);
	}

}




BOOLEAN FixPeImport0(VOID* PeMemPtr)
{
	DbgPrintEx(77, 0, "FixPeImport0 .. !\n");
	IMAGE_DOS_HEADER* DosHeader = (IMAGE_DOS_HEADER*)PeMemPtr;
	IMAGE_NT_HEADERS64* NtHeader = (IMAGE_NT_HEADERS64*)((UINT64)PeMemPtr + DosHeader->e_lfanew);
	UINT32 import_va = NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

	if (import_va == 0)
	{
		return TRUE;
	}

	IMAGE_IMPORT_DESCRIPTOR* current_import_descriptor = (IMAGE_IMPORT_DESCRIPTOR*)((UINT64)(PeMemPtr)+import_va);

	struct ModuleAddr
	{
		bool use = false;
		char name[100];
		UINT64 addr;
	};

	// ModuleAddr moduleCache[3] = { 0 };

	ModuleAddr* moduleCache = (ModuleAddr*)KAlloc(sizeof(ModuleAddr) * 100);
	RtlIsZeroMemory(moduleCache, sizeof(ModuleAddr) * 100);

	while (current_import_descriptor->FirstThunk) {

		char* module_name = (char*)((UINT64)(PeMemPtr)+current_import_descriptor->Name);

		PVOID ModelBase = 0;

		for (int i = 0; i < 100; ++i)
		{
			auto& cache = moduleCache[i];
			if (!cache.use)
			{
				continue;
			}
			if (strcmp(cache.name, module_name) == 0)
			{
				ModelBase = (PVOID)cache.addr;
				break;
			}
		}

		if (ModelBase == 0)
		{
			ModelBase = (PVOID)GetKernelModuleAddress(module_name);
			if (ModelBase)
			{
				for (int i = 0; i < 100; ++i)
				{
					auto& cache = moduleCache[i];
					if (!cache.use)
					{
						cache.use = true;
						strcpy(cache.name, module_name);
						cache.addr = (UINT64)ModelBase;
						break;
					}
				}
			}
		}

		if (ModelBase == 0)
		{
			return FALSE;
		}

		PIMAGE_THUNK_DATA64 current_first_thunk = (PIMAGE_THUNK_DATA64)((UINT64)(PeMemPtr)+current_import_descriptor->FirstThunk);
		PIMAGE_THUNK_DATA64 current_originalFirstThunk = (PIMAGE_THUNK_DATA64)((UINT64)(PeMemPtr)+current_import_descriptor->OriginalFirstThunk);
		
		while (current_originalFirstThunk->u1.Function) {
			IMAGE_IMPORT_BY_NAME* thunk_data = (IMAGE_IMPORT_BY_NAME*)((UINT64)(PeMemPtr)+current_originalFirstThunk->u1.AddressOfData);
			char* funName = thunk_data->Name;
			UINT64* funAddr = (UINT64*)&current_first_thunk->u1.Function;

			void* knFunAddress = GetExport(ModelBase, funName);

			if (knFunAddress)
			{
				*funAddr = (UINT64)knFunAddress;
			}

			++current_originalFirstThunk;
			++current_first_thunk;
		}

		++current_import_descriptor;
	}

	if (moduleCache)
	{
		KFree(moduleCache);
	}

	return TRUE;
}






