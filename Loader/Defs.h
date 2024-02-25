#pragma once
#include <vector>
#include <Windows.h>

#define KERNEL32		L"kernel32.dll"
#define KERNELBASE		L"kernelbase.dll"
#define NTDLL			L"ntdll.dll"
#define ADVAPI32		L"advapi32.dll"
#define SHELL32			L"shell32.dll"



typedef enum _SHUTDOWN_ACTION {
	ShutdownNoReboot,
	ShutdownReboot,
	ShutdownPowerOff
} SHUTDOWN_ACTION, * PSHUTDOWN_ACTION;



typedef struct _FILE_BASIC_INFORMATION {
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	ULONG         FileAttributes;
} FILE_BASIC_INFORMATION, * PFILE_BASIC_INFORMATION;


enum ConsoleColor
{
	Black = 0,
	Blue = 1,
	Green = 2,
	Cyan = 3,
	Red = 4,
	Magenta = 5,
	Brown = 6,
	LightGray = 7,
	DarkGray = 8,
	LightBlue = 9,
	LightGreen = 10,
	LightCyan = 11,
	LightRed = 12,
	LightMagenta = 13,
	Yellow = 14,
	White = 15
};



//pe

// 重定位地址表
typedef struct _RelocItemInfo {
	uint16_t type;
	uint16_t offset;
} RelocItemInfo, * PRelocItemInfo;



typedef struct _RelocInfo
{
	uint64_t address;
	uint16_t* item;
	uint32_t count;
	std::vector<RelocItemInfo> itemInfo;
}RelocInfo,*PRelocInfo;

struct ImportFunctionInfo
{
	std::string name;
	uint64_t* address;
};

struct ImportInfo
{
	std::string module_name;
	std::vector<ImportFunctionInfo> function_datas;
};
