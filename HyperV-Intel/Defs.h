#pragma once
#include <xmmintrin.h>
#include "ia32.hpp"

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

#if WINVER > 1803
using vmexit_handler_t = void(__fastcall*)(PVmContext* context, void* unknown);
#else
using vmexit_handler_t = void(__fastcall*)(PVmContext context, void* unknown);
#endif
