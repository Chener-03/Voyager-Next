#pragma once
#include "Pch.h"


#define BL_MEMORY_ATTRIBUTE_RWX 0x424000
#define BL_MEMORY_TYPE_APPLICATION 0xE0000012

#define SEC_TO_MS(seconds) seconds * 1000000

#define SECTION_RWX ((EFI_IMAGE_SCN_MEM_WRITE | \
	EFI_IMAGE_SCN_CNT_CODE | \
	EFI_IMAGE_SCN_CNT_UNINITIALIZED_DATA | \
	EFI_IMAGE_SCN_MEM_EXECUTE | \
	EFI_IMAGE_SCN_CNT_INITIALIZED_DATA | \
	EFI_IMAGE_SCN_MEM_READ))

#define RESOLVE_RVA(SIG_RESULT, RIP_OFFSET, RVA_OFFSET) \
	(*(INT32*)(((UINT64)SIG_RESULT) + RVA_OFFSET)) + ((UINT64)SIG_RESULT) + RIP_OFFSET



#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif



extern VOID __fastcall ASM_CPUID(unsigned int* Vendor);
extern int CPU_VENDOR;
int GetCPUVendor();
int GetCPUVendorCache();


UINT64 AsciiStrCmp2(CHAR8* FirstString, CHAR8* SecondString);


// taken from umap (btbd)
BOOLEAN CheckMask(CHAR8* base, CHAR8* pattern, CHAR8* mask);
VOID* FindPattern(CHAR8* base, UINTN size, CHAR8* pattern, CHAR8* mask);
VOID* GetExport(UINT8* base, CHAR8* export);
VOID* GetExportByIndex(UINT8* ModuleBase, UINT16 Index);
VOID MemCopy(VOID* dest, VOID* src, UINTN size);


UINT32 HashFunction(void* data, UINT32 size, UINT32 seed);

// 屏幕分辨率
VOID UefiSetResolution(int w, int h);


// 获取展开pe节
EFI_IMAGE_SECTION_HEADER* FindMapPeSectionByName(VOID* ImageBase, char* SectionName);

// 展开pe节
VOID MapPeSection(VOID* PeFilePtr, VOID* PeMemPtr);

UINT32 GetPeFileImageSize(VOID* InPePtr);

EFI_IMAGE_NT_HEADERS64* GetPeFileHeader(VOID* InPePtr);

VOID* AddSection(VOID* ImageBase, CHAR8* SectionName, UINT32 VirtualSize, UINT32 Characteristics);

VOID FixRelocImage(VOID* PeMemPtr);