#pragma once
#include "Pch.h"

#define IsValidHandle(handle) ((handle != NULL) && (handle != INVALID_HANDLE_VALUE) )

#define P2ALIGNDOWN(x, align) ((x) & -(align))
#define P2ALIGNUP(x, align) (-(-(x) & -(align)))



namespace Call
{

	template<typename RetVal = UINT64, typename... Args>
	RetVal CallFunctionAddress(PVOID Address, Args... Vars) {
		typedef RetVal(*FunctionTemplate)(Args...);
		return ((FunctionTemplate)Address)(Vars...);
	}


	template<typename RetVal = UINT64, typename... Args>
	RetVal CallFunction(LPCWSTR ModuleName, LPCSTR FuncName, Args... Vars)
	{
		auto dll = GetModuleHandleW(ModuleName);
		auto address = GetProcAddress(dll, FuncName);
		if (dll == 0 || address == 0)
		{
			return 0;
		}
		return ((RetVal(*)(Args...))address)(Vars...);
	}
}

std::wstring String2Wstring(const std::string& str);

std::string Wstring2String(const std::wstring& wstr);

VOID TextPrint(std::string Str, UINT16 Color = ConsoleColor::White);

VOID TextPrint(std::wstring Wstr, UINT16 Color = ConsoleColor::White);

// 强制删除文件
BOOL ForceDeleteFile(LPCWSTR FilePath);

// 删除自身
VOID DeleteSelf();

// 获取特权
BOOL PrivilegeMgr(LPCSTR name, BOOL enable);

// 获取固件类型
INT GetFirmwarType();

// 是否是UEFI以及是否开启SB
BOOL UefiFirmware(BOOL& SecurityBootEnable);


LPVOID MemAlloc(UINT32 Size, UINT32 MemoryType = PAGE_READWRITE);

BOOL MemFree(PVOID ptr);



// 禁用KVAS
VOID DisableKvas();

// 禁用快速启动
VOID DisableFastboot();

// 开启Hyper-V
VOID EnableHyperV();

// 禁用内核隔离
VOID DisableHvci();

// 获取服务列表
std::pair<ENUM_SERVICE_STATUS_PROCESSA*, DWORD> GetSvcList(int ServiceState, int ServiceType = SERVICE_DRIVER);

// 删除服务
VOID DeleteSvc(LPCSTR ServiceName,BOOL OnlyClose = TRUE);

// 锁定控制台不可操作
VOID LockConsole(bool Switch);


// 重启
VOID ForceReboot();

BOOL IsRunAsAdmin();

int RandomInt(int start, int end);


class FFile
{
public:

	FFile(LPCWSTR FilePath, BOOL IsDir) : IsDir(IsDir), FilePath(FilePath) {}
	~FFile();

	BOOL Exits();

	BOOL CheckIsDir() const;

	BOOL OpenFile(BOOL MustExits = TRUE)
	{
		if (IsOpened) return IsOpened;

		if (IsDir)
		{
			FileHandle = CreateFileW(FilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL
				, MustExits ? OPEN_EXISTING : CREATE_NEW, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		}
		else
		{
			FileHandle = CreateFileW(FilePath, GENERIC_READ | GENERIC_WRITE
				, FILE_SHARE_READ, NULL
				, MustExits ? OPEN_EXISTING : CREATE_NEW, FILE_ATTRIBUTE_NORMAL
				, NULL);
		}

		if (!IsValidHandle(FileHandle))
		{
			IsOpened = FALSE;
			#if _DEBUG
			auto err = GetLastError();
			TextPrint(L"Open [" + std::wstring(FilePath) + L"] file failed! Error code: " + std::to_wstring(err) + L"\n", ConsoleColor::Red);
			#endif
		}
		else
		{
			IsOpened = TRUE;
		}

		return IsOpened;
	}

	VOID CloseFile()
	{
		if (IsValidHandle(FileHandle))
		{
			CloseHandle(FileHandle);
			FileHandle = NULL;
			IsOpened = FALSE;
		}
	}

	FILE_BASIC_INFORMATION GetFileBaseInfo()
	{
		if (!IsValidHandle(FileHandle))
		{
			return { 0 };
		}
		IO_STATUS_BLOCK io_block;
		FILE_BASIC_INFORMATION fbi_ = {0};
		Call::CallFunction(NTDLL, "NtQueryInformationFile", FileHandle, &io_block, &fbi_, 0x28u, (FILE_INFORMATION_CLASS)4);
		return fbi_;
	}

	BOOL SetFileBaseInfo(const FILE_BASIC_INFORMATION& fbi)
	{
		IO_STATUS_BLOCK io_block;
		return !Call::CallFunction<BOOL>(NTDLL, "NtSetInformationFile", FileHandle, &io_block, (PVOID)&fbi, 0x28u, (FILE_INFORMATION_CLASS)4);
	}

	std::pair<PVOID,UINT32> ReadFile(UINT32 PreallocSize = 0)
	{
		if (!IsOpened || IsDir)
		{
			return {0,0};
		}

		auto fileSize = GetFileSize(FileHandle, 0);
		PVOID Buffer = MemAlloc(fileSize + PreallocSize);
		memset(Buffer, 0, fileSize + PreallocSize);

		::ReadFile(FileHandle, Buffer, fileSize, NULL, NULL);

		return { Buffer ,fileSize };
	}

	BOOL WriteFile(PVOID Buffer, UINT32 Size)
	{
		if (!IsOpened)
		{
			return FALSE;
		}
		return ::WriteFile(FileHandle, Buffer, Size, NULL, NULL);
	}

	BOOL DeleteFileOrDir()
	{
		if (IsDir)
		{
			return RemoveDirectoryW(FilePath);
		}
		return ::DeleteFileW(FilePath);
	}
	
private:
	LPCWSTR FilePath;
	BOOL IsDir;
	BOOL IsOpened = FALSE;
	HANDLE FileHandle = NULL;
};


class PEFile
{
public:
	PEFile(PVOID MemPtr):MemPtr(MemPtr) { }

	PIMAGE_DOS_HEADER GetDosHeader()
	{
		return reinterpret_cast<PIMAGE_DOS_HEADER>(MemPtr);
	}

	PIMAGE_NT_HEADERS64 GetNtHeaders()
	{
		const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(MemPtr);

		if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
			return nullptr;

		const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS64>(reinterpret_cast<uint64_t>(MemPtr) + dos_header->e_lfanew);

		if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
			return nullptr;

		return nt_headers;
	}

	IMAGE_SECTION_HEADER* GetSectionHeader(LPCSTR SectionName) {
		IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)MemPtr;
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
			return nullptr;
		}

		IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((UINT64)MemPtr + dosHeader->e_lfanew);
		if (ntHeader->Signature != IMAGE_NT_SIGNATURE) {
			return nullptr;
		}

		IMAGE_SECTION_HEADER* sectionHeader = (IMAGE_SECTION_HEADER*)((BYTE*)ntHeader + sizeof(IMAGE_NT_HEADERS));
		for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; ++i) {
			if (strcmp((char*)sectionHeader[i].Name, SectionName) == 0) {
				return &sectionHeader[i];
			}
		}

		return nullptr;
	}


	// 添加 section
	VOID AddSection(LPCSTR SectionName,UINT32 VirtualSize,DWORD Characteristics)
	{
		auto dosHander = this->GetDosHeader();
		PIMAGE_NT_HEADERS64 header = this->GetNtHeaders();
		auto fileHeader = &header->FileHeader;
		UINT16 sizeOfOptionalHeader = header->FileHeader.SizeOfOptionalHeader;

		 
		IMAGE_SECTION_HEADER* firstSectionHeader = (IMAGE_SECTION_HEADER*)(((UINT64)fileHeader) + sizeof(IMAGE_FILE_HEADER) + sizeOfOptionalHeader);

		// 计算节表是否允许再添加
		DWORD alreadyUseHeader = dosHander->e_lfanew + sizeof(IMAGE_NT_HEADERS) + header->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
		if (alreadyUseHeader + sizeof(IMAGE_SECTION_HEADER) > header->OptionalHeader.SizeOfHeaders)
		{
			// 重新分配待实现 一般是够用
			TextPrint("Section table is full!\n", ConsoleColor::Red);
			return;
		}


		UINT32 numberOfSections = header->FileHeader.NumberOfSections;
		UINT32 sectionAlignment = header->OptionalHeader.SectionAlignment;
		UINT32 fileAlignment = header->OptionalHeader.FileAlignment;

		IMAGE_SECTION_HEADER* newSectionHeader = &firstSectionHeader[numberOfSections];
		IMAGE_SECTION_HEADER* lastSectionHeader = &firstSectionHeader[numberOfSections - 1];

		memcpy(&newSectionHeader->Name, SectionName, strlen(SectionName));
		newSectionHeader->Misc.VirtualSize = VirtualSize;
		newSectionHeader->VirtualAddress = P2ALIGNUP((INT32)(lastSectionHeader->VirtualAddress + lastSectionHeader->Misc.VirtualSize), (INT32)sectionAlignment);

		newSectionHeader->SizeOfRawData = P2ALIGNUP((INT32)VirtualSize, (INT32)fileAlignment);
		newSectionHeader->Characteristics = Characteristics;


		newSectionHeader->PointerToRawData = (UINT32)(lastSectionHeader->PointerToRawData + lastSectionHeader->SizeOfRawData);
		++header->FileHeader.NumberOfSections;
		header->OptionalHeader.SizeOfImage = P2ALIGNUP((INT32)(newSectionHeader->VirtualAddress + newSectionHeader->Misc.VirtualSize), (INT32)sectionAlignment);
	}


	// 相对虚拟地址 转换为 文件偏移地址
	UINT32 Rva2Raw(PIMAGE_SECTION_HEADER pFirstSection, UINT32 nSectionNum, UINT32 nRva)
	{
		UINT32 nRet = 0;
		for (int i = 0; i < nSectionNum; i++) {
			//从文件中0x200(PointerToRawData)的位置，按照0x200（SizeofRawData）大小复制到内存0x1000（VirtualAddress）位置，复制之后的有效数据是0x26（VirtualSize）
			//VirtualSize 正常情况下是小于SizeofRawData的
			if (pFirstSection[i].VirtualAddress <= nRva && nRva < (pFirstSection[i].VirtualAddress + max(pFirstSection[i].Misc.VirtualSize, pFirstSection[i].SizeOfRawData))) {
				// 文件偏移 = 该段的 PointerToRawData + （内存偏移 - 该段起始的RVA(VirtualAddress)）
				nRet = nRva - pFirstSection[i].VirtualAddress + pFirstSection[i].PointerToRawData;
				break;
			}
		}
		return nRet;
	}

	// 文件偏移地址 转换为 相对虚拟地址
	UINT32 Raw2Rva(PIMAGE_SECTION_HEADER pFirstSection, UINT32 nSectionNum, UINT32 nFileOffset)
	{
		UINT32 nRet = 0;

		for (int i = 0; i < nSectionNum; i++) {
			UINT32 sectionStartFileOffset = pFirstSection[i].PointerToRawData;
			UINT32 sectionEndFileOffset = sectionStartFileOffset + max(pFirstSection[i].Misc.VirtualSize, pFirstSection[i].SizeOfRawData);

			// 判断文件偏移地址是否在该节的范围内
			if (sectionStartFileOffset <= nFileOffset && nFileOffset < sectionEndFileOffset) {
				// 相对虚拟地址 = 该节的 VirtualAddress + （文件偏移 - 该节的起始文件偏移）
				nRet = pFirstSection[i].VirtualAddress + (nFileOffset - sectionStartFileOffset);
				break;
			}
		}

		return nRet;
	}

	PIMAGE_SECTION_HEADER GetSectionHeaderByRva(UINT32 Rva)
	{
		PIMAGE_NT_HEADERS ntHeaders = GetNtHeaders();
		PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
		for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
			DWORD sectionVA = sectionHeader[i].VirtualAddress;
			DWORD sectionSize = sectionHeader[i].SizeOfRawData;
			if (Rva >= sectionVA && Rva < sectionVA + sectionSize) {
				return &sectionHeader[i];
			}
		}
		return nullptr;
	}


	// 获取重定位表(未展开的..)
	std::vector<RelocInfo> GetReloac()
	{
		std::vector<RelocInfo> relocs;
		auto header = this->GetNtHeaders();
		DWORD reloc_va = header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
		if (!reloc_va)
		{
			return relocs;
		}
		
		auto firstsection = IMAGE_FIRST_SECTION(header);

		reloc_va = Rva2Raw(firstsection, header->FileHeader.NumberOfSections, reloc_va);

		auto current_base_relocation = (PIMAGE_BASE_RELOCATION)(((uint64_t)MemPtr) + reloc_va);
		auto reloc_end = (PIMAGE_BASE_RELOCATION)((uint64_t)current_base_relocation + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

		while (current_base_relocation < reloc_end && current_base_relocation->SizeOfBlock) {
			RelocInfo reloc_info;

			auto raw = Rva2Raw(firstsection, header->FileHeader.NumberOfSections, current_base_relocation->VirtualAddress);

			reloc_info.address = reinterpret_cast<uint64_t>(MemPtr) + raw;
			reloc_info.item = reinterpret_cast<uint16_t*>(reinterpret_cast<uint64_t>(current_base_relocation) + sizeof(IMAGE_BASE_RELOCATION));
			reloc_info.count = (current_base_relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);

			for (int i = 0; i < reloc_info.count; ++i)
			{
				const uint16_t type = reloc_info.item[i] >> 12;
				const uint16_t offset = reloc_info.item[i] & 0xFFF;
				RelocItemInfo iti;
				iti.offset = offset;
				iti.type = type;
				reloc_info.itemInfo.push_back(iti);
			}

			relocs.push_back(reloc_info);
			current_base_relocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(reinterpret_cast<uint64_t>(current_base_relocation) + current_base_relocation->SizeOfBlock);
		}
		 
		return relocs;
	}


	// 获取导入表（未展开）
	VOID GetImport()
	{ 
		IMAGE_NT_HEADERS64* NtHeader = GetNtHeaders();
		UINT32 import_va = NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		if (import_va == 0)
		{
			return;
		}

		auto firstsection = IMAGE_FIRST_SECTION(NtHeader);
		import_va = Rva2Raw(firstsection, NtHeader->FileHeader.NumberOfSections, import_va);

		IMAGE_IMPORT_DESCRIPTOR* current_import_descriptor = (IMAGE_IMPORT_DESCRIPTOR*)((UINT64)(MemPtr)+import_va);


		while (current_import_descriptor->FirstThunk) {

			char* module_name = (char*)((UINT64)(MemPtr)+Rva2Raw(firstsection, NtHeader->FileHeader.NumberOfSections, current_import_descriptor->Name));

			auto current_first_thunk = (PIMAGE_THUNK_DATA64)((UINT64)(MemPtr) +Rva2Raw(firstsection, NtHeader->FileHeader.NumberOfSections, current_import_descriptor->FirstThunk));
			auto current_originalFirstThunk = (PIMAGE_THUNK_DATA64)((UINT64)(MemPtr) +Rva2Raw(firstsection, NtHeader->FileHeader.NumberOfSections, current_import_descriptor->OriginalFirstThunk));

			while (Rva2Raw(firstsection, NtHeader->FileHeader.NumberOfSections, current_originalFirstThunk->u1.Function) ) {

				IMAGE_IMPORT_BY_NAME* thunk_data = (IMAGE_IMPORT_BY_NAME*)((UINT64)(MemPtr) +Rva2Raw(firstsection, NtHeader->FileHeader.NumberOfSections, current_originalFirstThunk->u1.AddressOfData));

				auto name = thunk_data->Name;
				auto address = &current_first_thunk->u1.Function;

				++current_originalFirstThunk;
				++current_first_thunk;
			}


			++current_import_descriptor;
		}
	}


	// 修复重定位表偏移(未展开的..)
	VOID RelocateByDelta(const UINT64 delta)
	{
		auto reloac = GetReloac();
		for (const auto& current_reloc : reloac) {
			for (auto i = 0u; i < current_reloc.count; ++i) {
				const uint16_t type = current_reloc.item[i] >> 12;
				const uint16_t offset = current_reloc.item[i] & 0xFFF;

				if (type == IMAGE_REL_BASED_DIR64)
					*reinterpret_cast<uint64_t*>(current_reloc.address + offset) += delta;
			}
		}
	}


	// 修复 /GS
	VOID FixSecurityCookie(PVOID Address)
	{
		auto headers = GetNtHeaders();
		

		auto load_config_directory = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress;
		if (!load_config_directory)
		{
			//Load config directory wasn't found, probably StackCookie not defined, fix cookie skipped
			return ;
		}

		// to foa
		load_config_directory = this->Rva2Raw(IMAGE_FIRST_SECTION(headers), headers->FileHeader.NumberOfSections, load_config_directory);

		auto load_config_struct = (PIMAGE_LOAD_CONFIG_DIRECTORY)((uintptr_t)MemPtr + load_config_directory);
		auto stack_cookie = load_config_struct->SecurityCookie;
		if (!stack_cookie)
		{
			//StackCookie not defined, fix cookie skipped
			return; 
		}

		stack_cookie = stack_cookie - (uintptr_t)MemPtr + (uintptr_t)Address; //since our local image is already relocated the base returned will be kernel address

		if (*(uintptr_t*)(stack_cookie) != 0x2B992DDFA232) {
			//StackCookie already fixed!? this probably wrong
			return;
		}


		auto new_cookie = 0x2B992DDFA232 ^ GetCurrentProcessId() ^ GetCurrentThreadId();
		if (new_cookie == 0x2B992DDFA232)
			new_cookie = 0x2B992DDFA233;

		*(uintptr_t*)(stack_cookie) = new_cookie;
	}


	 
	// 导出表中结构体复制数据
	BOOL ExportVarCopy(WORD ExportIndex,PVOID DataPtr,UINT32 DataSize)
	{
		PIMAGE_NT_HEADERS64 header = this->GetNtHeaders();
		if (header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0)
		{
			return FALSE;
		}
		int nSectionNum = header->FileHeader.NumberOfSections;
		IMAGE_SECTION_HEADER* pSection = (IMAGE_SECTION_HEADER*)MemAlloc(nSectionNum * sizeof(IMAGE_SECTION_HEADER));

		for (int i = 0; i < nSectionNum; i++)
		{
			memcpy(&pSection[i], (BYTE*)header + sizeof(IMAGE_NT_HEADERS64) + i * sizeof(IMAGE_SECTION_HEADER), sizeof(IMAGE_SECTION_HEADER));
		}	

		auto exportOffset = Rva2Raw(pSection, nSectionNum, header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
		IMAGE_EXPORT_DIRECTORY* pExport = (IMAGE_EXPORT_DIRECTORY*)((UINT64)MemPtr + exportOffset);

		DWORD* addressOfFunctions = (DWORD*)((BYTE*)MemPtr + Rva2Raw(pSection, nSectionNum, pExport->AddressOfFunctions));
		DWORD* addressOfNames = (DWORD*)((BYTE*)MemPtr + Rva2Raw(pSection, nSectionNum, pExport->AddressOfNames));
		WORD* ordinals = (WORD*)((BYTE*)MemPtr + Rva2Raw(pSection, nSectionNum, pExport->AddressOfNameOrdinals));

		for (int i = 0; i < pExport->NumberOfFunctions; ++i)
		{
			DWORD functionAddress = addressOfFunctions[i];
			WORD ordinal = ordinals[i];
			char* functionName = (char*)((BYTE*)MemPtr + Rva2Raw(pSection, nSectionNum, addressOfNames[i]));
			if (ordinal == ExportIndex)
			{
				memcpy((PVOID)((UINT64)MemPtr + Rva2Raw(pSection, nSectionNum, functionAddress)), DataPtr,DataSize);
				#if _DEBUG
				TextPrint("Export [" + std::string(functionName) + "] data copy success!\n", ConsoleColor::Green);
				#endif
				break;
			}
		}
		MemFree(pSection);
		return TRUE;
	}

	// 给section写入数据
	VOID CopyDataToSection(LPCSTR SectionName ,UINT32 SectionOffset, PVOID DataPtr, UINT32 DataSize)
	{
		auto section = GetSectionHeader(SectionName);
		if (section == nullptr)
		{
			return;
		}
		memcpy((PVOID)((UINT64)MemPtr + section->PointerToRawData + SectionOffset), DataPtr, DataSize); 
	}


	// 在内存中展开pe节  请确保提供的内存足够
	VOID MapPeSection(PVOID DataPtr)
	{
		auto header = this->GetNtHeaders();
		memcpy(DataPtr, MemPtr, header->OptionalHeader.SizeOfHeaders);

		// 复制展开 sections
		const PIMAGE_SECTION_HEADER current_image_section = IMAGE_FIRST_SECTION(header);

		for (auto i = 0; i < header->FileHeader.NumberOfSections; ++i) {
			if ((current_image_section[i].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) > 0)
				continue;
			auto local_section = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(DataPtr) + current_image_section[i].VirtualAddress);
			memcpy(local_section, reinterpret_cast<void*>(reinterpret_cast<uint64_t>(MemPtr) + current_image_section[i].PointerToRawData), current_image_section[i].SizeOfRawData);
		}
	}


private:
	PVOID MemPtr = NULL;
};



