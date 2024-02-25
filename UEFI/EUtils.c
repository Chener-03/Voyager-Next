#include "EUtils.h"

#include "BootMgfw.h"
#include "Defs.h"

#define P2ALIGNDOWN(x, align) ((x) & -(align))

#define P2ALIGNUP(x, align) (-(-(x) & -(align)))


BOOLEAN CheckMask(CHAR8* base, CHAR8* pattern, CHAR8* mask)
{
	for (; *mask; ++base, ++pattern, ++mask)
		if (*mask == 'x' && *base != *pattern)
			return FALSE;

	return TRUE;
}

VOID* FindPattern(CHAR8* base, UINTN size, CHAR8* pattern, CHAR8* mask)
{
	size -= AsciiStrLen(mask);
	for (UINTN i = 0; i <= size; ++i)
	{
		VOID* addr = &base[i];
		if (CheckMask(addr, pattern, mask))
			return addr;
	}
	return NULL;
}

VOID* GetExport(UINT8* ModuleBase, CHAR8* export)
{
	EFI_IMAGE_DOS_HEADER* dosHeaders = (EFI_IMAGE_DOS_HEADER*)ModuleBase;
	if (dosHeaders->e_magic != EFI_IMAGE_DOS_SIGNATURE)
		return NULL;

	EFI_IMAGE_NT_HEADERS64* ntHeaders = (EFI_IMAGE_NT_HEADERS64*)(ModuleBase + dosHeaders->e_lfanew);
	UINT32 exportsRva = ntHeaders->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	EFI_IMAGE_EXPORT_DIRECTORY* exports = (EFI_IMAGE_EXPORT_DIRECTORY*)(ModuleBase + exportsRva);
	UINT32* nameRva = (UINT32*)(ModuleBase + exports->AddressOfNames);

	for (UINT32 i = 0; i < exports->NumberOfNames; ++i)
	{
		CHAR8* func = (CHAR8*)(ModuleBase + nameRva[i]);
		if (AsciiStrCmp(func, export) == 0)
		{
			UINT32* funcRva = (UINT32*)(ModuleBase + exports->AddressOfFunctions);
			UINT16* ordinalRva = (UINT16*)(ModuleBase + exports->AddressOfNameOrdinals);
			return (VOID*)(((UINT64)ModuleBase) + funcRva[ordinalRva[i]]);
		}
	}
	return NULL;
}

VOID* GetExportByIndex(UINT8* ModuleBase, UINT16 Index)
{
	EFI_IMAGE_DOS_HEADER* dosHeaders = (EFI_IMAGE_DOS_HEADER*)ModuleBase;
	if (dosHeaders->e_magic != EFI_IMAGE_DOS_SIGNATURE)
		return NULL;

	EFI_IMAGE_NT_HEADERS64* ntHeaders = (EFI_IMAGE_NT_HEADERS64*)(ModuleBase + dosHeaders->e_lfanew);
	UINT32 exportsRva = ntHeaders->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	EFI_IMAGE_EXPORT_DIRECTORY* exports = (EFI_IMAGE_EXPORT_DIRECTORY*)(ModuleBase + exportsRva);

	if (Index >= exports->NumberOfFunctions)
		return NULL;

	UINT32* funcRva = (UINT32*)(ModuleBase + exports->AddressOfFunctions);
	UINT64 exportAddress = ((UINT64)ModuleBase) + funcRva[Index];

	return (VOID*)exportAddress;
}



VOID MemCopy(VOID* dest, VOID* src, UINTN size)
{
	for (UINT8* d = dest, *s = src; size--; *d++ = *s++); 
}


int CPU_VENDOR = -1;

int GetCPUVendor()
{
	unsigned int Vendor[3] = { 0 };
	ASM_CPUID(&Vendor[0]);

	if (Vendor[0] == 0x756e6547 && Vendor[1] == 0x49656e69 && Vendor[2] == 0x6c65746e)
	{
		// GenuineIntel
		return 1;
	}
	else if (Vendor[0] == 0x68747541 && Vendor[1] == 0x69746E65 && Vendor[2] == 0x444D4163)
	{
		return 2;
	}

	return 0;
}

int GetCPUVendorCache()
{
	if (CPU_VENDOR == -1)
	{
		CPU_VENDOR = GetCPUVendor();
	}
	return CPU_VENDOR;
}


UINT64 AsciiStrCmp2(CHAR8* FirstString, CHAR8* SecondString)
{
	while (*FirstString && (*FirstString == *SecondString)) {
		FirstString++;
		SecondString++;
	}
	return (UINT64)(*FirstString - *SecondString);
}


UINT32 HashFunction(void* data, UINT32 size, UINT32 seed) {
	UINT8* bytes = (UINT8*)data;
	UINT32 hash = seed;

	for (UINT32 i = 0; i < size; ++i) {
		hash ^= bytes[i];
		hash = (hash << 5) ^ (hash >> 27);
	}
	return hash;
}


VOID UefiSetResolution(int w, int h) //w, h = 0 (max resolution)
{
	EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
	EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	if (gBS->LocateProtocol(&GraphicsOutputProtocolGuid, NULL, (void**)&gop) || !gop) {
		return;
	}

	UINTN best_i = gop->Mode->Mode;
	int best_w = gop->Mode->Info->HorizontalResolution;
	int best_h = gop->Mode->Info->VerticalResolution;
	w = (w <= 0 ? w < 0 ? best_w : 0x7fffffff : w);
	h = (h <= 0 ? h < 0 ? best_h : 0x7fffffff : h);

	for (UINT32 i = gop->Mode->MaxMode; i--;)
	{
		int new_w = 0, new_h = 0;

		UINTN info_size;
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
		if (EFI_ERROR(gop->QueryMode(gop, i, &info_size, &info))) {
			continue;
		}

		if (info_size < sizeof(*info)) {
			if (info) {
				gBS->FreePool(info);
			}
			continue;
		}

		new_w = info->HorizontalResolution;
		new_h = info->VerticalResolution;

		if (info) {
			gBS->FreePool(info);
		}

		const int new_missing = max(w - new_w, 0) + max(h - new_h, 0);
		const int best_missing = max(w - best_w, 0) + max(h - best_h, 0);
		if (new_missing > best_missing) {
			continue;
		}

		int new_over = max(-w + new_w, 0) + max(-h + new_h, 0);
		int best_over = max(-w + best_w, 0) + max(-h + best_h, 0);
		if (new_missing == best_missing && new_over >= best_over) {
			continue;
		}

		best_w = new_w;
		best_h = new_h;
		best_i = i;
	}

	if (best_i != gop->Mode->Mode) {
		gop->SetMode(gop, best_i);
	}
}


EFI_IMAGE_SECTION_HEADER* FindMapPeSectionByName(VOID* ImageBase, char* SectionName)
{
	EFI_IMAGE_DOS_HEADER* dosHeader = (EFI_IMAGE_DOS_HEADER*)ImageBase;
	if (dosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
		return NULL;
	}
	EFI_IMAGE_NT_HEADERS64* ntHeader = (EFI_IMAGE_NT_HEADERS64*)((UINT64)ImageBase + dosHeader->e_lfanew);
	if (ntHeader->Signature != EFI_IMAGE_NT_SIGNATURE) {
		return NULL;
	}
	EFI_IMAGE_SECTION_HEADER* sectionHeader = (EFI_IMAGE_SECTION_HEADER*)((unsigned char*)ntHeader + sizeof(EFI_IMAGE_NT_HEADERS64));
	for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; ++i) {
		if (AsciiStrCmp2((char*)sectionHeader[i].Name, SectionName) == 0) {
			return &sectionHeader[i];
		}
	}
	return NULL;
}


UINT32 GetPeFileImageSize(VOID* InPePtr)
{
	EFI_IMAGE_DOS_HEADER* DosHeader = (EFI_IMAGE_DOS_HEADER*)InPePtr;
	if (DosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE)
	{
		return 0;
	}
	EFI_IMAGE_NT_HEADERS64* NtHeader = (EFI_IMAGE_NT_HEADERS64*)((UINT64)InPePtr + DosHeader->e_lfanew);
	return NtHeader->OptionalHeader.SizeOfImage;
}


EFI_IMAGE_NT_HEADERS64* GetPeFileHeader(VOID* InPePtr)
{
	EFI_IMAGE_DOS_HEADER* DosHeader = (EFI_IMAGE_DOS_HEADER*)InPePtr;
	if (DosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE)
	{
		return NULL;
	}
	EFI_IMAGE_NT_HEADERS64* NtHeader = (EFI_IMAGE_NT_HEADERS64*)((UINT64)InPePtr + DosHeader->e_lfanew);
	return NtHeader;
}


EFI_IMAGE_SECTION_HEADER* GetPeFirstSection(VOID* PePtr)
{
	EFI_IMAGE_DOS_HEADER* DosHeader = (EFI_IMAGE_DOS_HEADER*)PePtr;
	EFI_IMAGE_NT_HEADERS64* NtHeader = (EFI_IMAGE_NT_HEADERS64*)((UINT64)PePtr + DosHeader->e_lfanew);
	return (EFI_IMAGE_SECTION_HEADER*)((UINT64)NtHeader + sizeof(EFI_IMAGE_NT_HEADERS64));
}


VOID MapPeSection(VOID* PeFilePtr, VOID* PeMemPtr)
{
	EFI_IMAGE_DOS_HEADER* DosHeader = (EFI_IMAGE_DOS_HEADER*)PeFilePtr;
	EFI_IMAGE_NT_HEADERS64* NtHeader = (EFI_IMAGE_NT_HEADERS64*)((UINT64)PeFilePtr + DosHeader->e_lfanew);
	MemCopy(PeMemPtr, PeFilePtr, NtHeader->OptionalHeader.SizeOfHeaders);

	EFI_IMAGE_SECTION_HEADER* current_image_section = GetPeFirstSection(PeMemPtr);

	for (auto i = 0; i < NtHeader->FileHeader.NumberOfSections; ++i) {
		if ((current_image_section[i].Characteristics & EFI_IMAGE_SCN_CNT_UNINITIALIZED_DATA) > 0)
			continue;
		void* local_section = (void*)((UINT64)(PeMemPtr) + current_image_section[i].VirtualAddress);
		MemCopy(local_section, (void*)((UINT64)(PeFilePtr) + current_image_section[i].PointerToRawData), current_image_section[i].SizeOfRawData);
	}
}



VOID* AddSection(VOID* ImageBase, CHAR8* SectionName, UINT32 VirtualSize, UINT32 Characteristics)
{
	EFI_IMAGE_DOS_HEADER* dosHeader = (EFI_IMAGE_DOS_HEADER*)ImageBase;
	EFI_IMAGE_NT_HEADERS64* ntHeaders = (EFI_IMAGE_NT_HEADERS64*)((UINT64)ImageBase + dosHeader->e_lfanew);

	UINT16 sizeOfOptionalHeader = ntHeaders->FileHeader.SizeOfOptionalHeader;
	EFI_IMAGE_FILE_HEADER* fileHeader = &(ntHeaders->FileHeader);

	EFI_IMAGE_SECTION_HEADER* firstSectionHeader = (EFI_IMAGE_SECTION_HEADER*)(((UINT64)fileHeader) + sizeof(EFI_IMAGE_FILE_HEADER) + sizeOfOptionalHeader);

	UINT32 numberOfSections = ntHeaders->FileHeader.NumberOfSections;
	UINT32 sectionAlignment = ntHeaders->OptionalHeader.SectionAlignment;
	UINT32 fileAlignment = ntHeaders->OptionalHeader.FileAlignment;

	EFI_IMAGE_SECTION_HEADER* newSectionHeader = &firstSectionHeader[numberOfSections];
	EFI_IMAGE_SECTION_HEADER* lastSectionHeader = &firstSectionHeader[numberOfSections - 1];

	MemCopy(&newSectionHeader->Name, SectionName, AsciiStrLen(SectionName));
	newSectionHeader->Misc.VirtualSize = VirtualSize;
	newSectionHeader->VirtualAddress =
		P2ALIGNUP(lastSectionHeader->VirtualAddress +
			lastSectionHeader->Misc.VirtualSize, sectionAlignment);

	newSectionHeader->SizeOfRawData = P2ALIGNUP(VirtualSize, fileAlignment);
	newSectionHeader->Characteristics = Characteristics;

	newSectionHeader->PointerToRawData =
		(UINT32)(lastSectionHeader->PointerToRawData +
			lastSectionHeader->SizeOfRawData);

	++ntHeaders->FileHeader.NumberOfSections;
	ntHeaders->OptionalHeader.SizeOfImage =
		P2ALIGNUP(newSectionHeader->VirtualAddress +
			newSectionHeader->Misc.VirtualSize, sectionAlignment);

	return ((UINT64)ImageBase) + newSectionHeader->VirtualAddress;
}




VOID FixRelocImage(VOID* PeMemPtr)
{
	EFI_IMAGE_DOS_HEADER* DosHeader = (EFI_IMAGE_DOS_HEADER*)PeMemPtr;
	EFI_IMAGE_NT_HEADERS64* NtHeader = (EFI_IMAGE_NT_HEADERS64*)((UINT64)PeMemPtr + DosHeader->e_lfanew);


	//计算 delta
	INT64 DeltaOffset = (INT64)((UINT64)PeMemPtr - NtHeader->OptionalHeader.ImageBase);
	if (!DeltaOffset) {
		return;
	}

	// 先找到重定位表
	UINT32 reloc_va = NtHeader->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	if (!reloc_va)
	{
		return;
	}


	EFI_IMAGE_BASE_RELOCATION* current_base_relocation = (EFI_IMAGE_BASE_RELOCATION*)(((UINT64)PeMemPtr) + reloc_va);
	EFI_IMAGE_BASE_RELOCATION* reloc_end = (EFI_IMAGE_BASE_RELOCATION*)((UINT64)current_base_relocation + NtHeader->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

	while (current_base_relocation < reloc_end && current_base_relocation->SizeOfBlock) {

		UINT32 va = current_base_relocation->VirtualAddress;

		UINT64 address = (UINT64)(PeMemPtr)+va;
		UINT16* item = (UINT16*)((UINT64)(current_base_relocation)+sizeof(EFI_IMAGE_BASE_RELOCATION));
		UINT32 count = (current_base_relocation->SizeOfBlock - sizeof(EFI_IMAGE_BASE_RELOCATION)) / sizeof(UINT16);

		for (int i = 0; i < count; ++i)
		{
			UINT16 type = item[i] >> 12;
			UINT16 offset = item[i] & 0xFFF;

			if (type == EFI_IMAGE_REL_BASED_DIR64)
			{
				*(UINT64*)(address + offset) += DeltaOffset;
			}

		}
		current_base_relocation = (EFI_IMAGE_BASE_RELOCATION*)((UINT64)(current_base_relocation)+current_base_relocation->SizeOfBlock);
	}

}
