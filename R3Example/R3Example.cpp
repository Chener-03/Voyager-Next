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
	REPLACE_4K_PAGE,
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
		UINT32 index;
		GPA gpa;
		GVA gva;
		UINT64 dirbase;
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


class HookFunc
{
#define jmpSelfCodeLen 12
#define jmpOldCodeLen 14

public:

	bool Hook(void* funAddr, void* selfFunAddr, UINT32 hookSize, UINT32 hvIndex)
	{
		this->old_fun_addr = funAddr;
		this->new_fun_addr = selfFunAddr;
		this->hook_size = hookSize;
		this->hv_index = hvIndex;

		if (hookSize < jmpSelfCodeLen)
		{
			return false;
		}

		if (!fuckPage4k && !MallocFuckPageMemory())
		{
			return false;
		}

		if (!callOldJmpCode)
		{
			callOldJmpCode = VirtualAlloc(NULL, hookSize + jmpOldCodeLen + 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memset(callOldJmpCode, 0x90, hookSize + jmpOldCodeLen + 10);
		}

		// 获取funAddr的gpa
		Command cmd = { 0 };
		cmd.TranslateData.dirbase = GetDirbase();
		cmd.TranslateData.gva = (GVA)funAddr;
		HyperVCall(VMEXIT_KEY, VMX_COMMAND::TRANSLATE_GVA2GPA, &cmd);
		GPA funAddrGpa = cmd.TranslateData.gpa;


		// 函数所在ept pd转为4kb的pt
		cmd.ShadowPage.gpa = funAddrGpa;
		cmd.ShadowPage.index = hvIndex;
		HyperVCall(VMEXIT_KEY, VMX_COMMAND::COVER_PAGE_2M_TO_4K, &cmd);



		// 获取到 funAddrGpa 这一页的首地址 拷贝到 fuckPage4k
		GPA funPageGpa = (funAddrGpa >> 12) << 12;
		//页面和函数的偏移
		UINT32 offset = funAddrGpa - funPageGpa;
		// copy
		cmd.CopyData.SrcGpa = funPageGpa;
		cmd.CopyData.DestDirbase = GetDirbase();
		cmd.CopyData.DestGva = (GVA)fuckPage4k;
		cmd.CopyData.Size = 4096;
		HyperVCall(VMEXIT_KEY, VMX_COMMAND::READ_GUEST_PHY, &cmd);


		//构建跳转原函数的内存区域
		memcpy(callOldJmpCode, funAddr, hookSize);
		memcpy((void*)((UINT64)callOldJmpCode + hookSize), &jmpOldCode[0], jmpOldCodeLen);
		UINT64 old_ret_pos = (UINT64)fuckPage4k + offset + hookSize;
		memcpy((void*)((UINT64)callOldJmpCode + hookSize + 6), &old_ret_pos, 8);



		/**
		 *	注意 这里不可以使用读指针并且指针地址位于这个4k页面内
		 *	比如 movzx   eax, byte ptr [*]  ；[*]位于这个页面内
		 *		jmp qword ptr [*] ；[*]位于这个页面内
		 *		使用这种会读的时候执行 执行的时候读 ept违规会无限循环
		 *	所以用这种方式
		 *	movabs rax, addr
		 *  push rax
		 *	ret
		 */
		// 修改要hook的函数头部 跳转到我们的函数
		UINT64 fuckPageFuncAddr = (UINT64)fuckPage4k + offset;
		memset((void*)fuckPageFuncAddr, 0x90, hookSize);
		memcpy((void*)fuckPageFuncAddr, &jmpSelfCode[0], jmpSelfCodeLen);
		memcpy((void*)(fuckPageFuncAddr + 2), &selfFunAddr, 8);


		// 挂载fuckpage到ept
		cmd.ShadowPage.dirbase = GetDirbase();
		cmd.ShadowPage.gva = (GVA)fuckPage4k;
		cmd.ShadowPage.gpa = funAddrGpa;
		cmd.ShadowPage.index = hvIndex;
		HyperVCall(VMEXIT_KEY, VMX_COMMAND::REPLACE_4K_PAGE, &cmd);

		return true;
	}

	void* GetOldPtr()
	{
		return callOldJmpCode;
	}

private:

	void* fuckPage4k = nullptr;	// 完全复制一页 用于ept仅执行
	UINT64 fuckPage4kGpa = 0;

	UINT64 dirbase = 0;			// 当前dirbase cache 
	UCHAR jmpSelfCode[13] = "\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\x50\xC3";
	UCHAR jmpOldCode[15] = "\xFF\x25\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF";


	void* callOldJmpCode = nullptr;	// 保存原来修改的字节 后面添加上jmp 调用这个地址来调用原函数

	void* old_fun_addr = nullptr;
	void* new_fun_addr = nullptr;
	UINT32 hook_size = 0;
	UINT32 hv_index = 0;


private:
	bool MallocFuckPageMemory()
	{
		fuckPage4k = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		memset(fuckPage4k, 0, 4096);
		if (!GetDirbase())
		{
			return false;
		}

		Command cmd = { 0 };
		cmd.TranslateData.gva = (UINT64)fuckPage4k;
		cmd.TranslateData.dirbase = GetDirbase();
		HyperVCall(VMEXIT_KEY, VMX_COMMAND::TRANSLATE_GVA2GPA, &cmd);
		if ((cmd.TranslateData.gpa & 0xFFF) == 0)
		{
			fuckPage4kGpa = cmd.TranslateData.gpa;
			return true;
		}
		return false;
	}

	UINT64 GetDirbase()
	{
		if (dirbase)
		{
			return dirbase;
		}

		Command cmd = { 0 };
		cmd.DirbaseData.Dirbase = 0;
		HyperVCall(VMEXIT_KEY, VMX_COMMAND::CURRENT_DIRBASE, &cmd);
		dirbase = cmd.DirbaseData.Dirbase;
		if (!dirbase)
		{
			return 0;
		}
		return dirbase;
	}

};

HookFunc h;

#pragma section(".mysection", execute,read,write)
#pragma section(".s11",read,write)
__declspec(allocate(".s11")) int i[4096];


int TestFunction(int a, int b)
{
	return a + b;
}

int FuckTestFunction(int a, int b)
{
	auto o = h.GetOldPtr();
	auto c = ((int(*)(int, int))o)(1,5);
	return a * b * c;
}

// 丢到最后一个节  下面会读函数内容 防止他俩在同1个页面内 导致eptv无限循环
__declspec(code_seg(".mysection"))
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
	// debug 模式 msvc 会代理一层jmp 所以需要找到实际函数地址 
	UINT64 jmpTestFunAdr = (UINT64)&TestFunction;
	UINT32 offset = *(UINT32*)(jmpTestFunAdr + 1);
	UINT64 TestFunAdr = jmpTestFunAdr + 5 + offset;

	UINT64 jmpFuckFunAdr = (UINT64)&FuckTestFunction;
	UINT32 offset1 = *(UINT32*)(jmpFuckFunAdr + 1);
	UINT64 FuckFunAdr = jmpFuckFunAdr + 5 + offset1;

	printf("===== before hook =====\n");
	printf("[?] TestFunction(2,4) is %d \n", TestFunction(2, 4));
	printf("[?] TestFunction 字节码: \n");
	for (int i = 0; i < 10; ++i)
	{
		unsigned char bt = ((unsigned char*)TestFunAdr)[i];
		printf("%02x ", bt);
	}
	printf("\n");

	auto res = h.Hook((void*)TestFunAdr, (void*)FuckFunAdr, 20, 2);

	printf("===== after hook =====\n");
	while (1)
	{
		system("pause");
		printf("TestFunction(2,4) is %d \n", TestFunction(2, 4));
		system("pause");
		printf("Hook 后TestFunction 字节码: \n");
		for (int i = 0; i < 10; ++i)
		{
			unsigned char bt = ((unsigned char*)TestFunAdr)[i];
			printf("%02x ", bt);
		}
		printf("\n");
	}

	endp:
	system("pause");
	return 0;
}



