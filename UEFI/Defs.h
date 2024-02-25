#pragma once
#include "Pch.h"

typedef unsigned long       DWORD;
typedef long LONG;
typedef long long LONGLONG;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    } DUMMYSTRUCTNAME;
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER;


typedef struct _FILE_BASIC_INFORMATION {
	LARGE_INTEGER	CreationTime;
	LARGE_INTEGER	LastAccessTime;
	LARGE_INTEGER	LastWriteTime;
	LARGE_INTEGER	ChangeTime;
	unsigned long	FileAttributes;
} FILE_BASIC_INFORMATION, * PFILE_BASIC_INFORMATION;




typedef struct _SMBIOS3_TABLE_HEADER
{
	unsigned char Signature[5];                                                     //0x0
	unsigned char Checksum;                                                         //0x5
	unsigned char Length;                                                           //0x6
	unsigned char MajorVersion;                                                     //0x7
	unsigned char MinorVersion;                                                     //0x8
	unsigned char Docrev;                                                           //0x9
	unsigned char EntryPointRevision;                                               //0xa
	unsigned char Reserved;                                                         //0xb
	unsigned long StructureTableMaximumSize;                                        //0xc
	unsigned long long StructureTableAddress;                                        //0x10
} SMBIOS3_TABLE_HEADER, * PSMBIOS3_TABLE_HEADER;


enum WinloadContext
{
	ApplicationContext,
	FirmwareContext
};



typedef struct _LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;	// 16
	LIST_ENTRY InMemoryOrderLinks;	// 32
	LIST_ENTRY InInitializationOrderLinks; // 48
	UINT64 ModuleBase; // 56
	UINT64 EntryPoint; // 64
	UINTN SizeOfImage; // 72
}LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY, ** PPLDR_DATA_TABLE_ENTRY;



typedef struct _IMAGE_THUNK_DATA64 {
	union {
		UINT64 ForwarderString;  // PBYTE 
		UINT64 Function;         // PDWORD
		UINT64 Ordinal;
		UINT64 AddressOfData;    // PIMAGE_IMPORT_BY_NAME
	} u1;
} IMAGE_THUNK_DATA64;
typedef IMAGE_THUNK_DATA64* PIMAGE_THUNK_DATA64;


typedef struct _IMAGE_IMPORT_DESCRIPTOR {
	union {
		DWORD   Characteristics;            // 0 for terminating null import descriptor
		DWORD   OriginalFirstThunk;         // RVA to original unbound IAT (PIMAGE_THUNK_DATA)
	} DUMMYUNIONNAME;
	DWORD   TimeDateStamp;                  // 0 if not bound,
	// -1 if bound, and real date\time stamp
	//     in IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND)
	// O.W. date/time stamp of DLL bound to (Old BIND)

	DWORD   ForwarderChain;                 // -1 if no forwarders
	DWORD   Name;
	DWORD   FirstThunk;                     // RVA to IAT (if bound this IAT has actual addresses)
} IMAGE_IMPORT_DESCRIPTOR;

