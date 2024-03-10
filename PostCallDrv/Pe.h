#pragma once
#include "KUtils.h"
#include "Pch.h"
#include "PEStructs.h"


#define IMAGE_FIRST_SECTION( ntheader ) ((PIMAGE_SECTION_HEADER)        \
    ((ULONG_PTR)(ntheader) +                                            \
     FIELD_OFFSET( IMAGE_NT_HEADERS, OptionalHeader ) +                 \
     ((ntheader))->FileHeader.SizeOfOptionalHeader   \
    ))



VOID* GetExport(PVOID ModuleBase, CHAR8* Name);

VOID* GetExportByIndex(PVOID ModuleBase, UINT16 Index);



PIMAGE_NT_HEADERS64 GetNtHeaders(void* ptr);

VOID FixPeRelocTable(VOID* PeMemPtr);

BOOLEAN FixPeImport0(VOID* PeMemPtr);
