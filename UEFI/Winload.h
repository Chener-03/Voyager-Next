#pragma once
#include "Pch.h"
#include "Defs.h"


#if WINVER == 2202


#define ALLOCATE_IMAGE_BUFFER_SIG "\xE8\x00\x00\x00\x00\x8B\xD8\x85\xC0\x0F\x88\xCF\x00\x00\x00\x48\x8B"
#define ALLOCATE_IMAGE_BUFFER_MASK "x????xxxxxxx???xx"

#define BG_DISPLAY_STRING_SIG "\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\xC8\xE8\x00\x00\x00\x00\x48"
#define BG_DISPLAY_STRING_MASK "x??xxx????xxxx????x"

#define BGP_GX_PARSE_BITMAP_SIG "\xE8\x00\x00\x00\x00\x8B\xD8\x85\xC0\x78\x1D\x48\x8B\x4C"
#define BGP_GX_PARSE_BITMAP_MASK "x????xxxxxxxxx"


#define BLP_ARCH_SWITCH_CONTEXT_SIG "40\x53\x48\x83\xEC\x20\x48\x8B\x15\x00\x00\x00\x00\x48\x8D\x05"
#define BLP_ARCH_SWITCH_CONTEXT_MASK "xxxxxxxxx????xxx"


// 48 03 D1 E8 ? ? ? ? ? 8B ? E8 ? ? ? ? 85 C0
#define MI_FREE_KERNEL_PAD_SECTIONS_SIG "\x48\x03\xD1\xE8\x00\x00\x00\x00\x00\x8B\x00\xE8\x00\x00\x00\x00\x85\xC0"
#define MI_FREE_KERNEL_PAD_SECTIONS_MASK "xxxx?????x?x????xx"


#define MI_WRITE_PROTECT_SYSTEM_IMAGE_SIG  "\xE8\x00\x00\x00\x00\xF0\xFF\x0D\x00\x00\x00\x00\x48\x8B\xCE\xE8\x00\x00\x00\x00\x85\xC0"
#define MI_WRITE_PROTECT_SYSTEM_IMAGE_MASK "x????xxx????xxxx????xx"



typedef UINT64(EFIAPI* ALLOCATE_IMAGE_BUFFER)(VOID** imageBuffer, UINTN imageSize, UINT32 memoryType,
	UINT32 attributes, VOID* unused, UINT32 Value);


typedef EFI_STATUS(EFIAPI* LDR_LOAD_IMAGE)(int a1,const CHAR16* ImagePath,const CHAR16* ModuleName,int ImageSize,int a5,int a6,__int64 lplpTableEntry,
	__int64 a8,__int64 a9,int a10,int a11,int a12,int a13,int a14,__int64 a15,__int64 a16);


#endif


#if WINVER == 2302
/**
	查找 __int64 __fastcall BlImgAllocateImageBuffer(__int64 *a1, __int64 a2, unsigned int a3, unsigned int a4, int a5, int a6)
	.text:00000001800AACFC 45 33 C9                      xor     r9d, r9d
	.text:00000001800AACFF 41 8B D6                      mov     edx, r14d
	.text:00000001800AAD02 41 B8 0A 00 00 D0             mov     r8d, 0D000000Ah
	.text:00000001800AAD08 E8 D3 9C FF FF                call    BlImgAllocateImageBuffer
	.text:00000001800AAD08
	.text:00000001800AAD0D 8B D8                         mov     ebx, eax
	.text:00000001800AAD0F 85 C0                         test    eax, eax
	.text:00000001800AAD11 78 7D                         js      short loc_1800AAD90
	.text:00000001800AAD11
	.text:00000001800AAD13 21 7C 24 28                   and     dword ptr [rsp+70h+var_48], edi
	.text:00000001800AAD17 45 33 C0                      xor     r8d, r8d
	.text:00000001800AAD1A 48 21 7C 24 20                and     [rsp+70h+var_50], rdi
 */
#define ALLOCATE_IMAGE_BUFFER_SIG "\xE8\x00\x00\x00\x00\x8B\xD8\x85\xC0\x78\x00\x21\x7C\x24\x00\x45\x33\xC0"
#define ALLOCATE_IMAGE_BUFFER_MASK "x????xxxxx?xxx?xxx"



 /*
	 查找 BgDisplayString
	 (B9 ? ? 00 00 E8 ? ? ? ? 48 8B C8 E8)
	 (B9 ? ? 00 00 E8 ? ? ? ? 48 8B C8 E8 ? ? ? ? 48)
	 .text:0000000180021C1D 75 12                         jnz     short loc_180021C31
	 .text:0000000180021C1D
	 .text:0000000180021C1F B9 16 27 00 00                mov     ecx, 2716h
	 .text:0000000180021C24 E8 63 E0 08 00                call    BlResourceFindMessage
	 .text:0000000180021C24
	 .text:0000000180021C29 48 8B C8                      mov     rcx, rax
	 .text:0000000180021C2C E8 C7 50 13 00                call    BgDisplayString
	 .text:0000000180021C2C
	 .text:0000000180021C31
	 .text:0000000180021C31                               loc_180021C31:                          ; CODE XREF: OslDisplayUpdateBackground+E9↑j
	 .text:0000000180021C31 48 83 C4 20                   add     rsp, 20h
	 .text:0000000180021C35 41 5E                         pop     r14
	 .text:0000000180021C37 5F                            pop     rdi
	 .text:0000000180021C38 5E                            pop     rsi
	 .text:0000000180021C39 5B                            pop     rbx
	 .text:0000000180021C3A 5D                            pop     rbp
  */
#define BG_DISPLAY_STRING_SIG "\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\xC8\xE8\x00\x00\x00\x00\x48"
#define BG_DISPLAY_STRING_MASK "x??xxx????xxxx????x"




  /*
	  查找 BgpGxParseBitmap
	  （E8 ? ? ? ? 8B D8 85 C0 78 1D 48 8B 4C）
	  .text:0000000180156CB4                               loc_180156CB4:                          ; CODE XREF: BgDisplayBackgroundImage+4A↑j
	  .text:0000000180156CB4 48 83 64 24 30 00             and     [rsp+28h+arg_0], 0
	  .text:0000000180156CBA 48 8D 54 24 30                lea     rdx, [rsp+28h+arg_0]
	  .text:0000000180156CBF E8 9C 2A 00 00                call    BgpGxParseBitmap
	  .text:0000000180156CBF
	  .text:0000000180156CC4 8B D8                         mov     ebx, eax
	  .text:0000000180156CC6 85 C0                         test    eax, eax
	  .text:0000000180156CC8 78 1D                         js      short loc_180156CE7
   */
#define BGP_GX_PARSE_BITMAP_SIG "\xE8\x00\x00\x00\x00\x8B\xD8\x85\xC0\x78\x1D\x48\x8B\x4C"
#define BGP_GX_PARSE_BITMAP_MASK "x????xxxxxxxxx"




#define BLP_ARCH_SWITCH_CONTEXT_SIG "\x40\x53\x48\x83\xEC\x20\x48\x8B\x15\x00\x00\x1D\x00"
#define BLP_ARCH_SWITCH_CONTEXT_MASK "xxxxxxxxx??xx"


// 48 03 D1 E8 ? ? ? ? ? 8B ? E8 ? ? ? ? 85 C0
// MiInitializeDriverImages call  MiFreeKernelPadSections
#define MI_FREE_KERNEL_PAD_SECTIONS_SIG "\x48\x03\xD1\xE8\x00\x00\x00\x00\x00\x8B\x00\xE8\x00\x00\x00\x00\x85\xC0"
#define MI_FREE_KERNEL_PAD_SECTIONS_MASK "xxxx?????x?x????xx"

//  MiInitSystem call MiWriteProtectSystemImages
#define MI_WRITE_PROTECT_SYSTEM_IMAGE_SIG  "\xE8\x00\x00\x00\x00\xF0\xFF\x0D\x00\x00\x00\x00\x48\x8B\xCE"
#define MI_WRITE_PROTECT_SYSTEM_IMAGE_MASK "x????xxx????xxx"



typedef UINT64(EFIAPI* ALLOCATE_IMAGE_BUFFER)(VOID** imageBuffer, UINTN imageSize, UINT32 memoryType,
	UINT32 attributes, VOID* unused, UINT32 Value);


typedef EFI_STATUS(EFIAPI* LDR_LOAD_IMAGE)(VOID* a1, VOID* a2, CHAR16* ImagePath, UINT64* ImageBasePtr, UINT32* ImageSize,
	VOID* a6, VOID* a7, VOID* a8, VOID* a9, VOID* a10, VOID* a11, VOID* a12, VOID* a13, VOID* a14, VOID* a15, VOID* a16, VOID* a17);


#endif


static_assert(sizeof(ALLOCATE_IMAGE_BUFFER_SIG) == sizeof(ALLOCATE_IMAGE_BUFFER_MASK), "signature and mask do not match size!");
static_assert(sizeof(BG_DISPLAY_STRING_SIG) == sizeof(BG_DISPLAY_STRING_MASK), "signature and mask size's dont match...");
static_assert(sizeof(BGP_GX_PARSE_BITMAP_SIG) == sizeof(BGP_GX_PARSE_BITMAP_MASK), "signature and mask size's dont match...");

// winload 导出函数
typedef UINT64(*BlMmAllocatePagesInRangeDef)(void** Buffer, UINT64 PagesCount, UINT64 Flags, UINT64 AllocateFlags, UINT64, UINT64, UINT64);
extern BlMmAllocatePagesInRangeDef BlMmAllocatePagesInRangeFunc;

typedef UINT64(*BlMmAllocatePhysicalPagesInRangeDef)(void** Buffer, UINT64 PagesCount, UINT64 Flags, UINT64 AllocateFlags, UINT64, UINT64, UINT64);
extern BlMmAllocatePhysicalPagesInRangeDef BlMmAllocatePhysicalPagesInRangeFunc;

typedef EFI_STATUS(*BlMmFreePhysicalPagesDef)(void* PhysicalBuffer);
extern BlMmFreePhysicalPagesDef BlMmFreePhysicalPagesFunc;

typedef EFI_STATUS(*BlMmMapPhysicalAddressExDef)(void** VirtualAddress, void* PhysicalAddress, UINT64 Size, UINT32, UINT32);
extern BlMmMapPhysicalAddressExDef BlMmMapPhysicalAddressExFunc;

typedef EFI_STATUS(*BlMmUnmapVirtualAddressExDef)(void* VirtualAddress, UINT64 Size, UINT32);
extern BlMmUnmapVirtualAddressExDef BlMmUnmapVirtualAddressExFunc;

typedef char(*BlUtlCheckSumDef)(int Key, void* Buffer, UINT32 BufferSize, int Flags);
extern BlUtlCheckSumDef BlUtlCheckSumFunc;

typedef void(__stdcall* BlpArchSwitchContext_t)(int target);
extern BlpArchSwitchContext_t g_BlpArchSwitchContext;



// 加载pe的时候获取大小
UINT64  BlImgGetPEImageSizeHook(UINT64 a1, UINT64 a2, unsigned int* a3);


// >=1709 中，winload 导出了一系列函数，其中包括 BlLdrLoadImage。
// hvloader 调用了 BlLdrLoadImage 来将 Hyper-V 加载到内存中。
// BlLdrLoadImage 在内部调用了 BlImgAllocateImageBuffer 来为 Hyper-V 模块分配内存。
// 通过在 BlImgAllocateImageBuffer 上 hook 来扩展分配的大小，并将整个分配设置为可读写可执行（RWX）。
EFI_STATUS EFIAPI BlImgAllocateImageBufferHook(VOID** imageBuffer,UINTN imageSize,UINT32 memoryType,UINT32 attributes,VOID* unused,UINT32 Value);


/// 23H2中 函数有17个参数
///	BlLdrLoadImage已从winload导出  加载hyper-v （hv.exe）时， HOOK 住并将自己的文件映射到它的内存
#if WINVER == 2302
EFI_STATUS EFIAPI BlLdrLoadImageHook
(
	int a1, int a2, CHAR16* ImagePath, CHAR16* ModuleName, UINT64 ImageSize,
	int a6, int a7, PPLDR_DATA_TABLE_ENTRY lplpTableEntry, UINT64 a9, UINT64 a10,
	int a11, int a12, int a13, int a14, int a15, UINT64 a16, UINT64 a17
);
#endif


VOID ChangeLogo(VOID* ImageBase, UINT32 ImageSize);

