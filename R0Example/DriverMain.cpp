#include "ntifs.h"
#include <intrin.h>

#define VMEXIT_KEY 0x20240101ABCD


EXTERN_C
NTKERNELAPI
_IRQL_requires_max_(APC_LEVEL)
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
KeGenericCallDpc(
	_In_ PKDEFERRED_ROUTINE Routine,
	_In_opt_ PVOID Context
);

EXTERN_C
NTKERNELAPI
_IRQL_requires_(DISPATCH_LEVEL)
_IRQL_requires_same_
LOGICAL
KeSignalCallDpcSynchronize(
	_In_ PVOID SystemArgument2
);

EXTERN_C
NTKERNELAPI
_IRQL_requires_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
KeSignalCallDpcDone(
	_In_ PVOID SystemArgument1
);


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



extern "C" unsigned long long HyperVCall(unsigned long long key, VMX_COMMAND cmd, void* data);


EXTERN_C VOID InitAllCore(_In_ struct _KDPC* Dpc,_In_opt_ PVOID DeferredContext,_In_opt_ PVOID SystemArgument1,_In_opt_ PVOID SystemArgument2)
{
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(DeferredContext);

	HyperVCall(VMEXIT_KEY, VMX_COMMAND::INIT_PAGE_TABLE, nullptr);
	
	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);
}



VOID Sleep(int msec)
{
	LARGE_INTEGER delay;
	delay.QuadPart = -10000i64 * msec;
	KeDelayExecutionThread(KernelMode, false, &delay);
}



PVOID KAlloc(UINT32 Size, bool exec) {

	PHYSICAL_ADDRESS MaxAddr = { 0 };
	MaxAddr.QuadPart = -1;
	PVOID addr = MmAllocateContiguousMemory(Size, MaxAddr);
	if (addr)
		RtlSecureZeroMemory(addr, Size);
	else
		return 0;

	if (exec)
	{
		// 设置内存属性为可执行
		PMDL mdl = IoAllocateMdl(addr, Size, FALSE, FALSE, NULL);
		if (!mdl) {
			MmFreeContiguousMemory(addr);
			return 0;
		}

		MmBuildMdlForNonPagedPool(mdl);

		// 将内存属性设置为 PAGE_EXECUTE_READWRITE
		MmProtectMdlSystemAddress(mdl, PAGE_EXECUTE_READWRITE );


		// 释放 Mdl 
		IoFreeMdl(mdl);
	}

	return addr;
}

VOID KFree(VOID* Buffer) {
	// ExFreePoolWithTag(Buffer, 'KVN');
	if (Buffer) MmFreeContiguousMemory((PVOID)Buffer);
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
			// callOldJmpCode = VirtualAlloc(NULL, hookSize + jmpOldCodeLen + 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			callOldJmpCode = KAlloc(hookSize + jmpOldCodeLen + 10,true);
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


		auto oldpageptr = (((UINT64)funAddr) >> 12) << 12;
		// RtlCopyMemory((void*)oldpageptr, fuckPage4k, 4096);
		// cmd.CopyData.DestDirbase = GetDirbase();
		// cmd.CopyData.SrcDirbase = GetDirbase();
		// cmd.CopyData.DestGva = oldpageptr;
		// cmd.CopyData.SrcGva = (UINT64)fuckPage4k;
		// cmd.CopyData.Size = 4096;
		// HyperVCall(VMEXIT_KEY, VMX_COMMAND::COPY_GUEST_VIR, &cmd);

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
		// fuckPage4k = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		fuckPage4k = KAlloc(4096,true);
		memset(fuckPage4k, 0, 4096);
		if (!GetDirbase())
		{
			return false;
		}

		Command cmd = { };
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

		Command cmd = {};
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


HookFunc* openprocess = nullptr;

NTSTATUS MyNtOpenProcess(
	PHANDLE            ProcessHandle,
	ACCESS_MASK        DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PCLIENT_ID         ClientId
)
{
	auto old = openprocess->GetOldPtr();
	// return STATUS_ACCESS_DENIED;
	return ((NTSTATUS(*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PCLIENT_ID))old)(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
}




VOID DriverMapEnter()
{
	DbgPrintEx(77, 0, "R0Example Load .. !\n");

	UINT64 Result = HyperVCall(VMEXIT_KEY, VMX_COMMAND::CHECK_LOAD, 0);
	if (Result == VMEXIT_KEY)
	{
		DbgPrintEx(77, 0, "[+] hyper-v hook is loading... \n");
	}
	else
	{
		DbgPrintEx(77, 0, "[-] hyper-v hook is not loading... \n"); 
		goto endp;
	}


	KeGenericCallDpc(InitAllCore, NULL);
	Sleep(1000);


	DbgPrintEx(77, 0, "[+] hyper-v Init Page Table .. !\n");

	HookFunc hf;
	openprocess = &hf;

	auto res = openprocess->Hook(&NtOpenProcess, &MyNtOpenProcess, 20, 0);
	if (res)
	{
		DbgPrintEx(77, 0, "[+] hyper-v hook NtOpenProcess success .. !\n");
	}
	else
	{
		DbgPrintEx(77, 0, "[-] hyper-v hook NtOpenProcess fail.. !\n");
	}


	Sleep(1000*60*60);
	endp:
	return;

}