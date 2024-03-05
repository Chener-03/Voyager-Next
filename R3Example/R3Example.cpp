// R3Example.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <intrin.h>
#include <iostream>
#include <windows.h>


#define VMEXIT_KEY 0x20240101ABCD

#define SELF_REF_PML4_IDX 510
#define MAPPING_PML4_IDX 100

#define MAPPING_ADDRESS_BASE 0x0000327FFFE00000
#define SELF_REF_PML4 0xFFFFFF7FBFDFE000

constexpr auto PAGE_4KB = (0x1000);
constexpr auto PAGE_2MB = (0x1000 * 512);
constexpr auto PAGE_1GB = (0x1000 * 512 * 512);

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define __OUT__ 
#define  __IN__

using UINT8 = unsigned char;
using UINT16 = unsigned short;
using UINT32 = unsigned int;
using UINT64 = unsigned long long;
using UINT128 = __m128;


using ULONG64 = UINT64;
using ULONG_PTR = UINT64;
using NTSTATUS = long;
using BOOL = int;
using PVOID = void*;

#define TRUE				1
#define FALSE				0




using GVA = UINT64;
using GPA = UINT64;
using HVA = UINT64;
using HPA = UINT64;



typedef union _Address
{
	UINT64 value;

	struct
	{
		UINT64 offset_4kb : 12;
		UINT64 pt_index : 9;
		UINT64 pd_index : 9;
		UINT64 pdpt_index : 9;
		UINT64 pml4_index : 9;
		UINT64 reserved : 16;
	}Type4KB;

	struct
	{
		UINT64 offset_2mb : 21;
		UINT64 pd_index : 9;
		UINT64 pdpt_index : 9;
		UINT64 pml4_index : 9;
		UINT64 reserved : 16;
	}Type2MB;

	struct
	{
		UINT64 offset_1gb : 30;
		UINT64 pdpt_index : 9;
		UINT64 pml4_index : 9;
		UINT64 reserved : 16;
	}Type1GB;

} Address, * PAddress;


enum class VMX_ROOT_RESULT
{
	SUCCESS,
	INIT_ERROR
};

enum class VMX_COMMAND
{
	CHECK_LOAD,
	INIT_PAGE_TABLE,
	CURRENT_DIRBASE,
	TRANSLATE_GVA2GPA,
	READ_GUEST_PHY,
	WRITE_GUEST_PHY,
	COPY_GUEST_VIR,

	COVER_PAGE_2M_TO_4K,
};


typedef union Command
{
	struct
	{
		UINT64 dirbase;
		GPA gpa;
		GVA gva;
	} TranslateData;

	struct
	{
		UINT64 Dirbase;
	}DirbaseData;


	struct
	{
		UINT64 SrcDirbase;
		UINT64 DestDirbase;
		GPA SrcGpa;
		GVA SrcGva;
		GPA DestGpa;
		GVA DestGva;
		UINT32 Size;
	}CopyData;


	struct
	{
		GPA gpa;
	}ShadowPage;

};



typedef struct _VmContext
{
	UINT64 rax;
	UINT64 rcx;
	UINT64 rdx;
	UINT64 rbx;
	UINT64 rsp;
	UINT64 rbp;
	UINT64 rsi;
	UINT64 rdi;
	UINT64 r8;
	UINT64 r9;
	UINT64 r10;
	UINT64 r11;
	UINT64 r12;
	UINT64 r13;
	UINT64 r14;
	UINT64 r15;
	UINT128 xmm0;
	UINT128 xmm1;
	UINT128 xmm2;
	UINT128 xmm3;
	UINT128 xmm4;
	UINT128 xmm5;
} VmContext, * PVmContext;




extern "C" unsigned long long HyperVCall(unsigned long long key, VMX_COMMAND cmd, void* data);

UINT64 InitAllCore()
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

			auto result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::INIT_PAGE_TABLE, nullptr);
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

int a = 1;


int main()
{
	UINT64 Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::CHECK_LOAD, 0);
	if (Result == VMEXIT_KEY)
	{
		printf("[+] hyper-v hook is loading... \n");
	}
	else
	{
		printf("[-] hyper-v hook is not loading... \n");
		goto endp;
	}
	system("pause");


	// 测试init
	Result = InitAllCore();
	if (Result == 0)
	{
		printf("[+] hyper-v page init success... \n");
	}
	else
	{
		printf("[-] hyper-v page init fail with code %llx... \n", Result);
		goto endp;
	}
	system("pause");
	 
	
	Command cmd ;
	UINT64 test_int = 20240101;
	UINT64 CurrentDirBase = 0;

	//测试 获取 dirbase
	cmd.DirbaseData.Dirbase = 0;
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::CURRENT_DIRBASE, &cmd);
	if (Result == 0 && cmd.DirbaseData.Dirbase != 0)
	{
		CurrentDirBase = cmd.DirbaseData.Dirbase;
		printf("[+] hyper-v get dirbase success, current proc disbase is %llx \n", CurrentDirBase);
	}
	else
	{
		printf("[-] hyper-v get dirbase fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");



	//测试 复制虚拟空间
	UINT64 test_copy_vir = 0;
	cmd.CopyData.SrcDirbase = CurrentDirBase;
	cmd.CopyData.DestDirbase = CurrentDirBase;
	cmd.CopyData.SrcGva = (UINT64) & test_int;
	cmd.CopyData.DestGva = (UINT64) &test_copy_vir;
	cmd.CopyData.Size = sizeof(UINT64);
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::COPY_GUEST_VIR, &cmd);
	if (Result == 0 && test_copy_vir == test_int)
	{
		printf("[+] hyper-v copy guest virt success, copy data is %lld \n", test_copy_vir);
	}
	else
	{
		printf("[-] hyper-v copy guest virt fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");



	// 测试翻译物理地址
	cmd.TranslateData.dirbase = CurrentDirBase;
	cmd.TranslateData.gva = (UINT64)&test_int;
	cmd.TranslateData.gpa = 0;
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::TRANSLATE_GVA2GPA, &cmd);
	GPA test_int_phy_addr = 0;
	if (Result == 0 && cmd.TranslateData.gpa != 0)
	{
		test_int_phy_addr = cmd.TranslateData.gpa;
		printf("[+] hyper-v translate_virt success, test_int phy addr is %llx \n", test_int_phy_addr);
	}
	else
	{
		printf("[-] hyper-v translate_virt fail with code %lld... \n", Result);
		goto endp;
	}
	system("pause");




	// 测试读取物理地址
	UINT64 test_read_phy_addr = 0;
	cmd.CopyData.DestDirbase = CurrentDirBase;
	cmd.CopyData.DestGva = (UINT64)&test_read_phy_addr;
	cmd.CopyData.SrcGpa = test_int_phy_addr;
	cmd.CopyData.Size = sizeof(UINT64); 
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::READ_GUEST_PHY, &cmd);
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
	cmd.CopyData.SrcDirbase = CurrentDirBase;
	cmd.CopyData.SrcGva = (UINT64)&test_write_phy_int;
	cmd.CopyData.DestGpa = test_int_phy_addr;
	cmd.CopyData.Size = sizeof(UINT64);
	Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::WRITE_GUEST_PHY, &cmd);
	if (Result == 0 && test_int == test_write_phy_int)
	{
		printf("[+] hyper-v write_guest_phys success, write data is %lld \n", test_int);
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

	UINT32 delta = (UINT64)&FuckTestFunction - (UINT64)&TestFunction - 5;
	printf("delta is %x \n", delta);

	void* fuckpage4k = malloc(0x1000);
	memset(fuckpage4k, 0, 0x1000);

	/**
	 *	hook step： (前提 2m分割为4k) 
	 *	1. 获取到 TestFunction 的 pa
	 *	2. pa位移到页面首地址
	 *	3. 从物理页面拷贝4kb 到fuckpage
	 *	4. 把fuckpage 的第二步偏移+1 处4字节地址改成 delta   （vs debug app 每个函数前面会嵌套一个jmp  方便测试hook，release的话需要构建跳转字节） 
	 *	5. 把 pt 的pfn 指向这个物理页面首地址 改为只执行属性
	 *	6. 触发违规后  如果是这个地址  则给他循环恢复为  读写/执行
	 */

	cmd.TranslateData.gva = (UINT64)&TestFunction;
	cmd.TranslateData.dirbase = CurrentDirBase;
	HyperVCall(VMEXIT_KEY, VMX_COMMAND::TRANSLATE_GVA2GPA, &cmd);
	GPA testFunGpa = cmd.TranslateData.gpa;
	printf("TestFun GPA is %llx \n", testFunGpa);
	printf("TestFun GVA is %llx \n", cmd.TranslateData.gva);

	cmd.ShadowPage.gpa = testFunGpa;
	HyperVCall(VMEXIT_KEY, VMX_COMMAND::COVER_PAGE_2M_TO_4K, &cmd);
	printf("Cover TestFun To 4kb Page\n");


	GPA testFunPageAddr = (testFunGpa >> 12) << 12;
	printf("TestFun Page GPA is %llx \n", testFunPageAddr);
	UINT32 offset = testFunGpa - testFunPageAddr;
	printf("TestFun Addr Page offset is %d \n", offset);

	cmd.CopyData.SrcGpa = testFunPageAddr;
	cmd.CopyData.DestDirbase = CurrentDirBase;
	cmd.CopyData.DestGva = (GVA)fuckpage4k;
	cmd.CopyData.Size = 0x1000;
	HyperVCall(VMEXIT_KEY, VMX_COMMAND::READ_GUEST_PHY, &cmd);



	endp:
	system("pause");
	return 0;
}
