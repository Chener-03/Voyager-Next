#include "Utils.h"



std::wstring String2Wstring(const std::string& str) {
	return std::wstring(str.begin(), str.end());
}

std::string Wstring2String(const std::wstring& wstr)
{
	return std::string(wstr.begin(), wstr.end());
}


VOID TextPrint(std::string Str, UINT16 Color)
{
	auto console = Call::CallFunction<HANDLE>(KERNELBASE, "GetStdHandle", STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO ConsolePrev;
	Call::CallFunction(KERNEL32, "GetConsoleScreenBufferInfo", console, &ConsolePrev);
	Call::CallFunction(KERNEL32, "SetConsoleTextAttribute", console, Color);

	DWORD Bytes = Str.length();
	Call::CallFunction(KERNEL32, "WriteConsoleA", console, Str.c_str(), Bytes, &Bytes, nullptr);

	Call::CallFunction(KERNEL32, "SetConsoleTextAttribute", console, ConsolePrev.wAttributes);
}

VOID TextPrint(std::wstring Wstr, UINT16 Color)
{
	TextPrint(std::string(Wstr.begin(), Wstr.end()), Color);
}


BOOL ForceDeleteFile(LPCWSTR FilePath)
{
	HANDLE hFile = Call::CallFunction<HANDLE>(KERNEL32, "CreateFileW", FilePath, DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	FILE_RENAME_INFO FRI = { true, 0, 2 * sizeof(WCHAR), ':' }; FRI.FileName[1] = '~';
	auto ReturnInfo = Call::CallFunction<BOOL>(KERNEL32, "SetFileInformationByHandle", hFile, FileRenameInfo, &FRI, sizeof(FRI));
	Call::CallFunction(KERNEL32, "CloseHandle", hFile);

	if (ReturnInfo)
	{
		hFile = Call::CallFunction<HANDLE>(KERNEL32, "CreateFileW", FilePath, DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)
			return false;

		FILE_DISPOSITION_INFO FDI = { true };
		Call::CallFunction(KERNEL32, "SetFileInformationByHandle", hFile, FileDispositionInfo, &FDI, sizeof(FDI));
		Call::CallFunction(KERNEL32, "CloseHandle", hFile);
	}

	return ReturnInfo;
}



VOID DeleteSelf()
{
	WCHAR FileName[255] = { 0 };
	Call::CallFunction(KERNEL32, "GetModuleFileNameW", NULL, FileName, sizeof(FileName));
	ForceDeleteFile(FileName);
}





BOOL PrivilegeMgr(LPCSTR name, BOOL enable) {
	HANDLE hToken;
	TOKEN_PRIVILEGES Priv;
	Priv.PrivilegeCount = 1;
	Call::CallFunction(ADVAPI32, "OpenProcessToken", (HANDLE)-1, TOKEN_ALL_ACCESS, &hToken);
	Priv.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
	auto ret = Call::CallFunction<BOOL>(ADVAPI32, "LookupPrivilegeValueA", nullptr, name, &Priv.Privileges[0].Luid);
	Call::CallFunction(ADVAPI32, "AdjustTokenPrivileges", hToken, false, &Priv, sizeof(Priv), nullptr, nullptr);
	Call::CallFunction(ADVAPI32, "CloseHandle", hToken);
	return ret;
}


INT GetFirmwarType()
{
	INT BootType = FirmwareTypeUnknown;
	if (!Call::CallFunction<BOOL>(KERNEL32, "GetFirmwareType", &BootType) || (BootType == FirmwareTypeUnknown)) {
		return FirmwareTypeUnknown;
	}
	return BootType;
}

BOOL UefiFirmware(BOOL& SecurityBootEnable)
{
	auto bootType = GetFirmwarType();
	if (bootType == FirmwareTypeUefi) {
		PrivilegeMgr("SeSystemEnvironmentPrivilege", TRUE);
		Call::CallFunction<DWORD>(KERNEL32, "GetFirmwareEnvironmentVariableA", "SecureBoot", "{8be4df61-93ca-11d2-aa0d-00e098032b8c}", &SecurityBootEnable, 1);
		PrivilegeMgr("SeSystemEnvironmentPrivilege", FALSE);
		return TRUE;
	}
	return FALSE;
}




LPVOID MemAlloc(UINT32 Size, UINT32 MemoryType) {
	return 	Call::CallFunction<LPVOID>(KERNEL32, "VirtualAlloc", 0, Size, MEM_COMMIT, MemoryType);
}

BOOL MemFree(PVOID ptr) {
	return 	Call::CallFunction<BOOL>(KERNEL32, "VirtualFree", ptr, 0, MEM_RELEASE);
}



// 禁用KVAS
VOID DisableKvas()
{
	HKEY MemoryManagement;
	ULONG Value = 3;
	Call::CallFunction(ADVAPI32, "RegOpenKeyA", HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management", &MemoryManagement);
	Call::CallFunction(ADVAPI32, "RegSetValueExA", MemoryManagement, "FeatureSettingsOverride", 0, REG_DWORD, (const BYTE*)&Value, sizeof(Value));
	Call::CallFunction(ADVAPI32, "RegSetValueExA", MemoryManagement, "FeatureSettingsOverrideMask", 0, REG_DWORD, (const BYTE*)&Value, sizeof(Value));
	Call::CallFunction(KERNEL32, "CloseHandle", MemoryManagement);
}

// 禁用快速启动
VOID DisableFastboot()
{
	HKEY Power; ULONG Value = 0;
	Call::CallFunction(ADVAPI32, "RegOpenKeyA", HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power", &Power);
	Call::CallFunction(ADVAPI32, "RegSetValueExA", Power, "HiberbootEnabled", 0, REG_DWORD, (const BYTE*)&Value, sizeof(Value));
	Call::CallFunction(KERNEL32, "CloseHandle", Power);
}

// 开启Hyper-V
VOID EnableHyperV()
{
	Call::CallFunction(SHELL32, "ShellExecuteA", 0, "open", "cmd", "/C bcdedit /set hypervisorlaunchtype auto", NULL, SW_HIDE);
}

// 禁用内核隔离
VOID DisableHvci()
{
	HKEY Key;
	ULONG Value = 0;
	Call::CallFunction(ADVAPI32, "RegOpenKeyA", HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\CredentialGuard", &Key);
	Call::CallFunction(ADVAPI32, "RegSetValueExA", Key, "Enabled", 0, REG_DWORD, (const BYTE*)&Value, sizeof(Value));
	Call::CallFunction(ADVAPI32, "CloseHandle", Key);
	Call::CallFunction(ADVAPI32, "RegOpenKeyA", HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity", &Key);
	Call::CallFunction(ADVAPI32, "RegSetValueExA", Key, "Enabled", 0, REG_DWORD, (const BYTE*)&Value, sizeof(Value));
	Call::CallFunction(ADVAPI32, "CloseHandle", Key);
	Call::CallFunction(ADVAPI32,"ShellExecuteA", 0, "open", "cmd", "reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity\" /v \"Enabled\" /t REG_DWORD /d 0 /f", NULL, SW_HIDE);
	Call::CallFunction(ADVAPI32,"ShellExecuteA", 0, "open", "cmd", "reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity\" /v \"Locked\" /t REG_DWORD /d 0 /f", NULL, SW_HIDE);
}


// 获取服务列表
std::pair<ENUM_SERVICE_STATUS_PROCESSA*, DWORD> GetSvcList(int ServiceState, int ServiceType)
{
	SC_HANDLE ServicesManager = Call::CallFunction<SC_HANDLE>(ADVAPI32, "OpenSCManagerA", 0, 0, SC_MANAGER_ALL_ACCESS);

	DWORD BytesNeeded, ServicesCount;

	Call::CallFunction<BOOL, SC_HANDLE, SC_ENUM_TYPE, DWORD, DWORD, LPBYTE, DWORD, LPDWORD, LPDWORD, LPDWORD, LPCSTR>
	(ADVAPI32, "EnumServicesStatusExA",
		ServicesManager, SC_ENUM_PROCESS_INFO, ServiceType,
		ServiceState, NULL, 0, &BytesNeeded, &ServicesCount,
		NULL, NULL);

	auto ServicesList = (ENUM_SERVICE_STATUS_PROCESSA*)MemAlloc(BytesNeeded);

	Call::CallFunction<BOOL, SC_HANDLE, SC_ENUM_TYPE, DWORD, DWORD, LPBYTE, DWORD, LPDWORD, LPDWORD, LPDWORD, LPCSTR>
	(ADVAPI32, "EnumServicesStatusExA",
		ServicesManager, SC_ENUM_PROCESS_INFO, ServiceType,
		ServiceState, (PBYTE)ServicesList, BytesNeeded, &BytesNeeded, &ServicesCount,
		NULL, NULL);


	Call::CallFunction(ADVAPI32, "CloseServiceHandle", ServicesManager);

	return std::pair<ENUM_SERVICE_STATUS_PROCESSA*, DWORD>(ServicesList, ServicesCount);
}


// 删除服务
VOID DeleteSvc(LPCSTR ServiceName, BOOL OnlyClose)
{
	auto [ServicesList, ServicesCount] = GetSvcList(SERVICE_ACTIVE, SERVICE_WIN32);
	for (int i = 0; i < ServicesCount; i++)
	{
		if ( std::string(ServicesList[i].lpServiceName) == std::string(ServiceName) && ServicesList[i].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING)
		{
			auto ServiceName0 = ServicesList[i].lpServiceName;
			auto ServicesManager = Call::CallFunction<SC_HANDLE>(ADVAPI32, "OpenSCManagerA", 0, 0, SC_MANAGER_ALL_ACCESS);
			auto Service = Call::CallFunction<SC_HANDLE>(ADVAPI32, "OpenServiceA", ServicesManager, ServiceName, SERVICE_ALL_ACCESS);

			SERVICE_STATUS ServiceStatus;
			Call::CallFunction(ADVAPI32, "ControlService", Service, SERVICE_CONTROL_STOP, &ServiceStatus);
			if (OnlyClose == FALSE)
			{
				Call::CallFunction(ADVAPI32, "DeleteService", Service);
			}
			Call::CallFunction(ADVAPI32, "CloseServiceHandle", ServicesManager);
		}
	}

	MemFree(ServicesList);


}


VOID LockConsole(bool Switch) {
	HWND Wnd = GetConsoleWindow();
	EnableWindow(Wnd, !Switch);
}

VOID ForceReboot()
{
	PrivilegeMgr("SeShutdownPrivilege", true);
	Call::CallFunction<NTSTATUS>(NTDLL, "NtShutdownSystem", ShutdownReboot);
	PrivilegeMgr("SeShutdownPrivilege", false);
}


BOOL IsRunAsAdmin()
{
	BOOL isAdmin = false;
	HANDLE hToken = nullptr;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
		PSID AdminSid = nullptr;

		if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdminSid))
		{
			if (CheckTokenMembership(nullptr, AdminSid, &isAdmin) == FALSE)
			{
				isAdmin = false;
			}

			FreeSid(AdminSid);
		}
		CloseHandle(hToken);
	}
	return isAdmin;
}

int RandomInt(int start, int end)
{
	srand(time(nullptr));
	return start + rand() % (end - start);
}



FFile::~FFile()
{
	CloseFile();
}

BOOL FFile::Exits()
{
	DWORD fileAttributes = GetFileAttributesW(FilePath);
	if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	return true;
}

BOOL FFile::CheckIsDir() const
{
	return IsDir;
}
