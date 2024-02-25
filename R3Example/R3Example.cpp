// R3Example.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <intrin.h>
#include <iostream>
#include <windows.h>


#define VMEXIT_KEY 0x20240101ABCD
// #define VMEXIT_KEY 0xDEADBEEFDEADBEEF

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using u128 = __m128;


using GuestVirtureAddress = u64;
using GuestPhysicalAddress = u64;
using HostVirtureAddress = u64;
using HostPhysicalAddress = u64;

enum class VMX_COMMAND
{
	init_page_tables,
	read_guest_phys,
	write_guest_phys,
	copy_guest_virt,
	get_dirbase,
	translate_virture_address,
	check_is_load,

	// ept
	add_shadow_page,
	add_shadow_page_phys,
	delete_shadow_page,
	unhide_shadow_page,
	disable_page_protection,
	DiablePCID
};

typedef union _Command
{
	struct _copy_phys
	{
		HostPhysicalAddress  phys_addr;
		GuestVirtureAddress buffer;
		u64 size;
	} copy_phys;

	struct _copy_virt
	{
		GuestPhysicalAddress dirbase_src;
		GuestVirtureAddress virt_src;
		GuestPhysicalAddress dirbase_dest;
		GuestVirtureAddress virt_dest;
		u64 size;
	} copy_virt;


	//translate
	struct _translate_virt
	{
		GuestVirtureAddress virt_src;
		GuestPhysicalAddress phys_addr;
	} translate_virt;


	//get_dirbase
	GuestPhysicalAddress dirbase;


	//ept start {
	struct _addShadowPage
	{
		GuestVirtureAddress uVirtualAddrToHook;
		GuestVirtureAddress uPageRead;
		GuestVirtureAddress uPageExecute;
	}addShadowPage;

	struct _addShadowPagePhys
	{
		GuestVirtureAddress uVirtualAddrToHook;
		GuestPhysicalAddress uPageRead;
		GuestPhysicalAddress uPageExecute;

	}addShadowPagePhys;

	struct _deleteShaowPage
	{
		GuestVirtureAddress uVirtualAddrToHook;
	}deleteShaowPage;

	struct _unHideShaowPage
	{
		GuestVirtureAddress uVirtualAddrToHook;
	}unHideShaowPage;

	struct _disablePageProtection
	{
		GuestPhysicalAddress phys_addr;
	}disablePageProtection;
	// } ept end

} Command, * PCommand;


extern "C" unsigned long long HyperVCall(unsigned long long key, VMX_COMMAND cmd, void* data);

u64 InitAllCore()
{
	GROUP_AFFINITY orig_group_affinity;
	GetThreadGroupAffinity(GetCurrentThread(), &orig_group_affinity);
	const auto group_count = GetActiveProcessorGroupCount();

	for (auto group_number = 0u; group_number < group_count; ++group_number)
	{
		const auto processor_count = GetActiveProcessorCount(group_number);
		for (auto processor_number = 0u; processor_number < processor_count; ++processor_number)
		{
			GROUP_AFFINITY group_affinity = { 0 };
			group_affinity.Mask = (KAFFINITY)(1) << processor_number;
			group_affinity.Group = group_number;
			SetThreadGroupAffinity(GetCurrentThread(), &group_affinity, NULL);

			auto result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::init_page_tables, nullptr);;
			if (result != 0)
				return result;
		}
	}

	SetThreadGroupAffinity(GetCurrentThread(), &orig_group_affinity, NULL);
	return 0;
}


int TestFunction(int a,int b)
{
	return a+b;
}

int FuckTestFunction(int a, int b)
{
	return a * b;
}


int main()
{
	u64 Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::check_is_load, 0);
	// if (Result == VMEXIT_KEY)
	// {
	// 	printf("[+] hyper-v hook is loading... \n");
	// }
	// else
	// {
	// 	printf("[-] hyper-v hook is not loading... \n");
	// 	goto endp;
	// }
	// system("pause");



	// 测试init
	Result = InitAllCore();
	if (Result == 0)
	{
		printf("[+] hyper-v page init success... \n");
	}
	else
	{
		printf("[-] hyper-v page init fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");



	
	Command cmd;

	unsigned long long test_int = 20240225;
	//测试 获取 dirbase
	cmd.dirbase = 0;
	u64 dirbase = 0;
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::get_dirbase, &cmd);
	if (Result == 0 && cmd.dirbase != 0)
	{
		dirbase = cmd.dirbase;
		printf("[+] hyper-v get dirbase success, current proc disbase is %llx \n", dirbase);
	}
	else
	{
		printf("[-] hyper-v get dirbase fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");



	//测试 复制虚拟空间
	cmd.copy_virt.dirbase_src = dirbase;
	cmd.copy_virt.dirbase_dest = dirbase;
	cmd.copy_virt.virt_src = (GuestVirtureAddress)&test_int;
	unsigned long long test_copy_dest = 0;
	cmd.copy_virt.virt_dest = (GuestVirtureAddress)&test_copy_dest;
	cmd.copy_virt.size = sizeof(unsigned long long);
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::copy_guest_virt, &cmd);
	if (Result == 0 && test_copy_dest == test_int)
	{
		printf("[+] hyper-v copy guest virt success, copy data is %lld \n", test_copy_dest);
	}
	else
	{
		printf("[-] hyper-v copy guest virt fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");



	// 测试翻译物理地址
	cmd.translate_virt.virt_src = (GuestVirtureAddress)&test_int;
	cmd.translate_virt.phys_addr = 0;
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::translate_virture_address, &cmd);
	GuestPhysicalAddress test_int_phy_addr = 0;
	if (Result == 0 && cmd.translate_virt.phys_addr != 0)
	{
		test_int_phy_addr = cmd.translate_virt.phys_addr;
		printf("[+] hyper-v translate_virt success, test_int phy addr is %llx \n", test_int_phy_addr);
	}
	else
	{
		printf("[-] hyper-v translate_virt fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");




	// 测试读取物理地址
	u64 test_read_phy_addr = 0;
	cmd.copy_phys.phys_addr = test_int_phy_addr;
	cmd.copy_phys.buffer = (GuestVirtureAddress)&test_read_phy_addr;
	cmd.copy_phys.size = sizeof(unsigned long long);
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::read_guest_phys, &cmd);
	if (Result == 0 && test_read_phy_addr == test_int)
	{
		printf("[+] hyper-v read_guest_phys success, read data is %lld \n", test_read_phy_addr);
	}
	else
	{
		printf("[-] hyper-v read_guest_phys fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");



	// 测试写物理地址
	unsigned long long test_write_phy_int = 2024666666;
	cmd.copy_phys.phys_addr = test_int_phy_addr;
	cmd.copy_phys.buffer = (GuestVirtureAddress)&test_write_phy_int;
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::write_guest_phys, &cmd);
	if (Result == 0 && test_int == test_write_phy_int)
	{
		printf("[+] hyper-v write_guest_phys success, read data is %lld \n", test_int);
	}
	else
	{
		printf("[-] hyper-v write_guest_phys fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");



	
	// hook测试函数

#define ALIGN_TO_4KB_UP(address) (((address) + 4095) & ~4095)	// 向后对齐4K
#define ALIGN_TO_4KB_DOWN(address) ((address) & ~4095)			// 向前对齐4K


	printf("TestFunction(2,4) is %d \n", TestFunction(2, 4));
	printf("TestFunction 字节码: \n");
	for (int i = 0; i < 10; ++i)
	{
		unsigned char bt = ((unsigned char*)&TestFunction)[i];
		printf("%02x ", bt);
	}
	printf("\n");

	void* realpage = malloc(0x1000);
	memcpy(realpage, (void*)ALIGN_TO_4KB_DOWN((u64)&TestFunction), 0x1000);
	printf("[+] success copy realpage \n");


	UINT32 delta = (UINT64)&FuckTestFunction - (UINT64) & TestFunction - 5 ;
	printf("delta is %x \n", delta);


	/*{
		// *(UINT32*)((UINT64)&TestFunction + 1) = delta;
		cmd.copy_virt.dirbase_dest = dirbase;
		cmd.copy_virt.dirbase_src = dirbase;
		cmd.copy_virt.size = 4;
		cmd.copy_virt.virt_src = (GuestVirtureAddress)&delta;
		cmd.copy_virt.virt_dest = (GuestVirtureAddress)((UINT64)&TestFunction + 1);
		HyperVCall(VMEXIT_KEY, VMX_COMMAND::copy_guest_virt, &cmd);
	}*/

	//printf("TestFunction(2,4) is %d \n", TestFunction(2, 4));

	// printf("TestFunction 字节码: \n");
	// for (int i = 0; i < 10; ++i)
	// {
	// 	unsigned char bt = ((unsigned char*)&TestFunction)[i];
	// 	printf("%02x ", bt);
	// }

	// printf("TestFunction(2,4) is %d \n", TestFunction(2, 4));


	void* fuckpage = malloc(0x1000);
	memcpy(fuckpage, (void*)ALIGN_TO_4KB_DOWN((u64)&TestFunction), 0x1000);
	printf("[+] success copy fuckpage \n");

	UINT32 dt = (UINT64)&TestFunction - (UINT64)ALIGN_TO_4KB_DOWN((u64)&TestFunction);

	*(UINT32*)((UINT64)fuckpage + dt + 1) = delta;


	system("pause");

	cmd.addShadowPage.uPageRead = (GuestVirtureAddress)realpage;
	cmd.addShadowPage.uPageExecute = (GuestVirtureAddress)fuckpage;
	cmd.addShadowPage.uVirtualAddrToHook = (GuestVirtureAddress)ALIGN_TO_4KB_DOWN((u64)&TestFunction);

	{
		GROUP_AFFINITY orig_group_affinity;
		GetThreadGroupAffinity(GetCurrentThread(), &orig_group_affinity);
		const auto group_count = GetActiveProcessorGroupCount();

		for (auto group_number = 0u; group_number < group_count; ++group_number)
		{
			const auto processor_count = GetActiveProcessorCount(group_number);
			for (auto processor_number = 0u; processor_number < processor_count; ++processor_number)
			{
				GROUP_AFFINITY group_affinity = { 0 };
				group_affinity.Mask = (KAFFINITY)(1) << processor_number;
				group_affinity.Group = group_number;
				SetThreadGroupAffinity(GetCurrentThread(), &group_affinity, NULL);

				Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::add_shadow_page, &cmd); 
			}
		}

		SetThreadGroupAffinity(GetCurrentThread(), &orig_group_affinity, NULL);
	}

	
	printf("HyperVCall Result is %d\n", Result);


	printf("TestFunction add shaow page end 字节码: \n");
	for (int i = 0; i < 10; ++i)
	{
		unsigned char bt = ((unsigned char*)&TestFunction)[i];
		printf("%02x ", bt);
	}

	printf("TestFunction(2,4) is %d \n", TestFunction(2, 4));

	cmd.deleteShaowPage.uVirtualAddrToHook = (GuestVirtureAddress)ALIGN_TO_4KB_DOWN((u64)&TestFunction);
	HyperVCall(VMEXIT_KEY, VMX_COMMAND::delete_shadow_page, &cmd);

	endp:
	system("pause");
	return 0;
}
