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




VOID FixRelocImage(VOID* PeMemPtr)
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


// by kdmapper
bool FixSecurityCookie(void* local_image, UINT64 kernel_image_base)
{
	auto headers = GetNtHeaders(local_image);
	if (!headers)
		return false;

	auto load_config_directory = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress;
	if (!load_config_directory)
	{
		return true;
	}
	
	auto load_config_struct = (PIMAGE_LOAD_CONFIG_DIRECTORY64)((uintptr_t)local_image + load_config_directory);
	auto stack_cookie = load_config_struct->SecurityCookie;
	if (!stack_cookie)
	{ 
		return true; // as I said, it is not an error and we should allow that behavior
	}

	stack_cookie = stack_cookie - (uintptr_t)kernel_image_base + (uintptr_t)local_image; //since our local image is already relocated the base returned will be kernel address

	if (*(uintptr_t*)(stack_cookie) != 0x2B992DDFA232) { 
		return false;
	} 

	auto new_cookie = 0x2B992DDFA232 ^ (UINT32)PsGetCurrentProcessId() ^ (UINT32)PsGetCurrentProcessId(); // here we don't really care about the value of stack cookie, it will still works and produce nice result
	if (new_cookie == 0x2B992DDFA232)
		new_cookie = 0x2B992DDFA233;

	*(uintptr_t*)(stack_cookie) = new_cookie; // the _security_cookie_complement will be init by the driver itself if they use crt
	return true;
}









