#include "Winload.h"
#include "Hooks.h"
#include "Hv.h"
#include "Resource.h"
#include "BootMgfw.h"
#include "Defs.h"
#include "ShareData.h"
#include "NtKernel.h"

BOOLEAN HyperVloading = FALSE;
BOOLEAN InstalledHvLoaderHook = FALSE;

BOOLEAN ExtendedAllocation = FALSE;
BOOLEAN HookedHyperV = FALSE;
UINT64 AllocationCount = 0;

BOOLEAN HookNtOsKernel = FALSE;

// logo 下方转圈圈的下面的文字...
PVOID g_BgDisplayStringPtr = NULL;

BlMmAllocatePagesInRangeDef BlMmAllocatePagesInRangeFunc = NULL;

BlMmAllocatePhysicalPagesInRangeDef BlMmAllocatePhysicalPagesInRangeFunc = NULL;

BlMmFreePhysicalPagesDef BlMmFreePhysicalPagesFunc = NULL;

BlMmMapPhysicalAddressExDef BlMmMapPhysicalAddressExFunc = NULL;

BlMmUnmapVirtualAddressExDef BlMmUnmapVirtualAddressExFunc = NULL;

BlUtlCheckSumDef BlUtlCheckSumFunc = NULL;

BlpArchSwitchContext_t g_BlpArchSwitchContext = NULL;


UINT64  BlImgGetPEImageSizeHook(UINT64 a1, UINT64 a2, unsigned int* a3)
{
	DisableInlineHook(&g_Hooks[BlImgGetPEImageSize]);
	UINT64 Result = ((UINT64(*)(UINT64, UINT64, unsigned int*))g_Hooks[BlImgGetPEImageSize].Address)(a1,a2,a3);
	EnableInlineHook(&g_Hooks[BlImgGetPEImageSize]);
	UINT32 image_sz = *(UINT32*)a3;

	// ntoskrnl.exe 大小11M左右，如果大于10M直接给他加20M，放patch的内容
	// hv.exe 大小不方便这里判断 通过 ImgAllocateImageBuffer 来扩展大小
	if (image_sz > 10*1024*1024L)
	{
		*a3 += 20 * 1024 * 1024L;
	}

	return Result;
}


UINT64 EFIAPI BlImgAllocateImageBufferHook(VOID** imageBuffer,UINTN imageSize,UINT32 memoryType,UINT32 attributes,VOID* unused,UINT32 Value)
{

	/**
	 *	调用链： BlLdrLoadImage -- LdrpLoadImage -- BlImgLoadPEImageEx -- ImgpLoadPEImage -- BlImgAllocateImageBuffer
	 *	加载 hv.exe时
	 *	第二次 调用 BlImgAllocateImageBuffer 时候 memoryType 为 0xE0000012(BL_MEMORY_TYPE_APPLICATION 好像是签名驱动的内存... )
	 *	ps: 加载 mcupdate.dll hv.exe kdstub.dll ...  这些顺序都是  D00000A--E0000012--D00000A  固定的..
	 *	在 E0000012 的时候多分配些内存
	 */

	
	if (HyperVloading && !ExtendedAllocation && ++AllocationCount == 2)
	{
		ExtendedAllocation = TRUE;

		EFI_IMAGE_NT_HEADERS64* peHeader = GetPeFileHeader(&g_MapperContext.hvPayloadData[0]);

		if (peHeader)
		{
			imageSize += peHeader->OptionalHeader.SizeOfImage;
			// 将整个hyper-v模块分配为rwx。。。  应该application类型的也行吧 还没试..
			memoryType = BL_MEMORY_ATTRIBUTE_RWX;
		}
	}


	DisableInlineHook(&g_Hooks[WinLoadAllocateImageHook]);
	UINT64 Result = ((ALLOCATE_IMAGE_BUFFER)g_Hooks[WinLoadAllocateImageHook].Address)(imageBuffer,imageSize,memoryType,attributes,unused,Value);
 
	if (!ExtendedAllocation)
		EnableInlineHook(&g_Hooks[WinLoadAllocateImageHook]);


	return Result;
}


#if WINVER == 2302
EFI_STATUS EFIAPI BlLdrLoadImageHook
(
	int a1, int a2, CHAR16* ImagePath, CHAR16* ModuleName, UINT64 ImageSize,
	int a6, int a7, PPLDR_DATA_TABLE_ENTRY lplpTableEntry, UINT64 a9, UINT64 a10,
	int a11, int a12, int a13, int a14, int a15, UINT64 a16, UINT64 a17
)
{
	/*
	 * 进入顺序
	 *	hv.exe -> hv.exe -> ntoskrnl.exe
	 */


	if (!StrCmp(ModuleName, L"hv.exe"))
	{ 
		HyperVloading = TRUE;
	}

	DisableInlineHook(&g_Hooks[WinLoadImageShitHook]);
	EFI_STATUS Result = ((LDR_LOAD_IMAGE)g_Hooks[WinLoadImageShitHook].Address)(a1, a2, ImagePath, ModuleName, ImageSize, 
		a6, a7, lplpTableEntry, 
		a9, a10, a11, a12, a13, a14, a15, a16, a17);


 
	if (!StrCmp(ModuleName, L"ntoskrnl.exe") && !HookNtOsKernel)
	{
		HookNtOsKernel = TRUE; 

		PLDR_DATA_TABLE_ENTRY TableEntry = *lplpTableEntry;

		// SpoofBuildNumber test
		if (0)
		{
			BOOLEAN SpoofBuildNumber = FALSE;
			UINT64 NtBuildNumber =
				(UINT64)FindPattern(TableEntry->ModuleBase, TableEntry->SizeOfImage,
				                    "\xC7\x81\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x41", "xx????????xx") + 6;

			if (NtBuildNumber != 6)
			{
				*(UINT16*)NtBuildNumber = 0x17134;
				SpoofBuildNumber = TRUE;
			}

			if (SpoofBuildNumber)
			{
				UINT64 CmpEditionVersion = (UINT64)FindPattern(TableEntry->ModuleBase, TableEntry->SizeOfImage,
				                                               "\x48\x83\x3D\x00\x00\x00\x00\x00\x74\x00\x33\xFF",
				                                               "xxx?????x?xx") + 8;
				if (CmpEditionVersion != 0x8)
				{
					*(UINT8*)CmpEditionVersion = 0x75;
				}
			}
		}

		// hook KiSystemStartup test
		if (0)
		{
			// 48 83 EC 38 4C 89 7C 24 30 4C 8B FC 48 ? ? ? ? ? ? B9
			PVOID adr = FindPattern(TableEntry->ModuleBase, TableEntry->SizeOfImage,
				"\x48\x83\xEC\x38\x4C\x89\x7C\x24\x30\x4C\x8B\xFC\x48\x00\x00\x00\x00\x00\x00\xB9",
				"xxxxxxxxxxxxx??????x");
		}


		// hook IoInitSystem test
		if (0)
		{
			// 40 53 48 83 EC 20 48 8D 05 33 B4 11 00
			auto IoInitSystemPattner = FindPattern(TableEntry->ModuleBase, TableEntry->SizeOfImage,
				"\x40\x53\x48\x83\xEC\x20\x48\x8D\x05\x33\xB4\x11\x00",
				"xxxxxxxxxxxxx");
		}


		// BlMmAllocatePagesInRange分配内存测试
		if (0)
		{
			void* PageBuffer = 0;
			UINT64 BufferSize = 20000;
			BlMmAllocatePagesInRangeFunc(&PageBuffer, BufferSize, 0xE0000012, 0x424000, 0, 0, 0);
		}



		// patch kernelos
		{
			PVOID MiFreeKernelPadSections = FindPattern(TableEntry->ModuleBase, TableEntry->SizeOfImage, MI_FREE_KERNEL_PAD_SECTIONS_SIG, MI_FREE_KERNEL_PAD_SECTIONS_MASK);
			if (MiFreeKernelPadSections)
			{
				unsigned char nop[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
				UINT64 offset = 0;
				if (WINVER == 2302)
				{
					offset = 3;
				}
				MemCopy((PVOID)((UINT64)MiFreeKernelPadSections + offset), &nop, 5);
			}


			PVOID MiWriteProtectSystemImages = FindPattern(TableEntry->ModuleBase, TableEntry->SizeOfImage,MI_WRITE_PROTECT_SYSTEM_IMAGE_SIG,MI_WRITE_PROTECT_SYSTEM_IMAGE_MASK);
			if (MiWriteProtectSystemImages)
			{
				unsigned char nop[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
				MemCopy(MiWriteProtectSystemImages, &nop, 5);
			}

		}


		// NT OS KERNEL Address
		PVOID kernel_os_image_base = TableEntry->ModuleBase;


		// PAD4 Section
		EFI_IMAGE_SECTION_HEADER* pad4Header = FindMapPeSectionByName(kernel_os_image_base, "Pad4");
		if (pad4Header)
		{

			PVOID lsp = (PVOID)((UINT64)kernel_os_image_base + pad4Header->VirtualAddress);

			UINT64 peImgSz = GetPeFileImageSize(&g_MapperContext.PostCallSysData[0]);
			if (peImgSz != 0)
			{
				MapPeImage(&g_MapperContext.PostCallSysData[0], lsp, kernel_os_image_base);
			}

			// set g_PostCallMapper
			PostCallMapperContext pcmc = { 0 };
			pcmc.KernelModuleBase = kernel_os_image_base;
			VOID* KeQueryPerformanceCounter = GetExport(kernel_os_image_base, "KeQueryPerformanceCounter");
			pcmc.KeQueryPerformanceCounterFunAddress = KeQueryPerformanceCounter;
			pcmc.KeQueryPerformanceCounterParam = 0;

			void* gmp_export_addr = GetExportByIndex(lsp, 0);
			if (gmp_export_addr)
			{
				MemCopy(gmp_export_addr, &pcmc, sizeof(PostCallMapperContext));
			}


			//KeQueryPerformanceCounter
			{
				//E8 ? ? ? ? 33 C9 E8 ? ? ? ? BB ? ? ? ? 48 89
				UINT64 CallKeQueryPerformanceCounterPattner = FindPattern(TableEntry->ModuleBase, TableEntry->SizeOfImage,
					"\xE8\x00\x00\x00\x00\x33\xC9\xE8\x00\x00\x00\x00\xBB\x00\x00\x00\x00\x48\x89",
					"x????xxx????x????xx");
				unsigned char* CallKeQueryPerformanceCounterPtr = (unsigned char*)((UINT64)CallKeQueryPerformanceCounterPattner + 7);
				EFI_IMAGE_NT_HEADERS64* head = GetPeFileHeader(lsp);
				UINT64 postCallEp = (UINT64)lsp + head->OptionalHeader.AddressOfEntryPoint;
				INT32 CallOffset = (UINT64)postCallEp - ((UINT64)CallKeQueryPerformanceCounterPtr + 5);
				*(INT32*)&CallKeQueryPerformanceCounterPtr[1] = CallOffset;
			}
		}

	}

 


	if (!StrCmp(ModuleName, L"hv.exe") && HookedHyperV == FALSE)
	{
		HookedHyperV = TRUE;

		PLDR_DATA_TABLE_ENTRY TableEntry = *lplpTableEntry;
		MapHypervImage((VOID*)TableEntry->ModuleBase, TableEntry->SizeOfImage);

		//扩展ldr SizeOfImage 大小为patch后的大小
		TableEntry->SizeOfImage = GetPeFileHeader((VOID*)TableEntry->ModuleBase)->OptionalHeader.SizeOfImage;
	}




	if (!HookNtOsKernel) EnableInlineHook(&g_Hooks[WinLoadImageShitHook]);

	return Result;
}
#endif


UINT64 BgpGxParseBitmapHook(UINT64 a1, unsigned int** a2)
{
	DisableInlineHook(&g_Hooks[BgpGxParseBitmap]);
	// __int64 __fastcall BgpGxParseBitmap(__int64 a1, unsigned int **a2)
	UINT64 Result = ((UINT64(*)(UINT64, unsigned int**))g_Hooks[BgpGxParseBitmap].Address)(&boot_logo_bytes[0], a2);
	EnableInlineHook(&g_Hooks[BgpGxParseBitmap]);

	 // __int64 __fastcall BgDisplayString(int a1)
	if (g_BgDisplayStringPtr)
	{
		((UINT64(*)(int))g_BgDisplayStringPtr)(L"2024 HAPPY NEW YEAR.");
	}
	return Result;
}


VOID ChangeLogo(VOID* ImageBase, UINT32 ImageSize)
{
	// winload.efi

	VOID* BgDisplayStringPattner = FindPattern(ImageBase, ImageSize, BG_DISPLAY_STRING_SIG, BG_DISPLAY_STRING_MASK);
	VOID* BgDisplayStringPtr = RESOLVE_RVA(BgDisplayStringPattner + 13, 5, 1);
	g_BgDisplayStringPtr = BgDisplayStringPtr;

	VOID* BgpGxParseBitmapPattner = FindPattern(ImageBase, ImageSize, BGP_GX_PARSE_BITMAP_SIG, BGP_GX_PARSE_BITMAP_MASK);
	VOID* BgpGxParseBitmapPtr = RESOLVE_RVA(BgpGxParseBitmapPattner, 5, 1);

	MakeInlineHook(&g_Hooks[BgpGxParseBitmap], BgpGxParseBitmapPtr, &BgpGxParseBitmapHook, TRUE);
}


