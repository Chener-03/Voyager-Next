#include "KUtils.h"
#include "Defs.h"


UINT64 AsciiStrCmp(CHAR8* FirstString, CHAR8* SecondString)
{
	while (*FirstString && (*FirstString == *SecondString)) {
		FirstString++;
		SecondString++;
	}
	return (UINT64)(*FirstString - *SecondString);
}



VOID Sleep(int msec)
{
	LARGE_INTEGER delay;
	delay.QuadPart = -10000i64 * msec;
	KeDelayExecutionThread(KernelMode, false, &delay);
}



PVOID KAlloc(UINT32 Size, bool exec) {

	PVOID ptr = ExAllocatePool2(exec ? POOL_FLAG_NON_PAGED_EXECUTE : POOL_FLAG_NON_PAGED, Size, 'KVN');
	if (ptr)
	{
		memset(ptr, 0, Size);
	}
	return ptr;
}

VOID KFree(VOID* Buffer) {
	ExFreePoolWithTag(Buffer, 'KVN');
}



NTSTATUS CharToWChar(const char* input, UNICODE_STRING* output)
{
	if (!input || !output)
	{
		return STATUS_INVALID_PARAMETER;
	}

	ANSI_STRING ansiString;
	RtlInitAnsiString(&ansiString, input);

	return RtlAnsiStringToUnicodeString(output, &ansiString, TRUE);
}


BOOLEAN ReadFile(char* path,__out void** data,__out UINT32* size)
{
	*data = nullptr;
	*size = 0;
	 

	UNICODE_STRING fullPath;
	WCHAR buf[255];
	RtlInitEmptyUnicodeString(&fullPath, buf, sizeof(WCHAR) * 255);


	UNICODE_STRING prefix;
	RtlInitUnicodeString(&prefix, L"\\??\\");

	RtlAppendUnicodeStringToString(&fullPath, &prefix);

	UNICODE_STRING fileName;
	CharToWChar(path, &fileName);

	RtlAppendUnicodeStringToString(&fullPath, &fileName);

	OBJECT_ATTRIBUTES fileAttributes;
	InitializeObjectAttributes(
		&fileAttributes,
		&fullPath,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL
	);

	HANDLE fileHandle;
	IO_STATUS_BLOCK ioStatusBlock;

	NTSTATUS status = ZwCreateFile(
		&fileHandle,
		FILE_GENERIC_READ ,
		&fileAttributes,
		&ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE ,
		NULL,
		0
	);

 
	if (!(NT_SUCCESS(status)))
	{
		return FALSE;
	}

	FILE_STANDARD_INFORMATION fileInfo;

	status = ZwQueryInformationFile(
		fileHandle,
		&ioStatusBlock,
		&fileInfo,
		sizeof(fileInfo),
		FileStandardInformation
	);

 

	if (!(NT_SUCCESS(status)))
	{
		ZwClose(fileHandle);
		return FALSE;
	}

	LARGE_INTEGER fileSize = fileInfo.EndOfFile;

 

	void* cache = KAlloc(fileSize.QuadPart);

	LARGE_INTEGER offset;
	offset.QuadPart = 0;

	status = ZwReadFile(
		fileHandle,
		NULL,
		NULL,
		NULL,
		&ioStatusBlock,
		cache,
		fileSize.QuadPart,
		&offset,
		NULL
	);

 
	if (!(NT_SUCCESS(status)))
	{
		KFree(cache);
		ZwClose(fileHandle);
		return FALSE;
	}

	*data = cache;
	*size = fileSize.QuadPart;

	ZwClose(fileHandle);
	return TRUE;
}



UINT64 GetKernelModuleAddress(const char* module_name) {
	void* buffer = nullptr;
	DWORD buffer_size = 0;

	NTSTATUS status = NtQuerySystemInformation(SystemModuleInformation, buffer, buffer_size, (PULONG) & buffer_size);

	while (status == STATUS_INFO_LENGTH_MISMATCH) {
		if (buffer != nullptr)
			KFree(buffer);

		buffer = KAlloc(buffer_size);
		status = NtQuerySystemInformation(SystemModuleInformation, buffer, buffer_size, (PULONG) & buffer_size);
	}

	if (!NT_SUCCESS(status)) {
		if (buffer != nullptr)
			KFree(buffer);
		return 0;
	}

	const auto modules = (PRTL_PROCESS_MODULES)buffer;
	if (!modules)
		return 0;

	for (auto i = 0u; i < modules->NumberOfModules; ++i) {

		auto current_module_name = reinterpret_cast<char*>(modules->Modules[i].FullPathName) + modules->Modules[i].OffsetToFileName;

		if (strcmp(module_name, current_module_name) == 0)
		{
			UINT64 result = reinterpret_cast<UINT64>(modules->Modules[i].ImageBase);

			KFree(buffer);
			return result;
		}
	}

	KFree(buffer);
	return 0;
}

