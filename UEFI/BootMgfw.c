#include "BootMgfw.h"
#include "Hooks.h" 
#include "Winload.h"
#include "ShareData.h"
#include "SmbiosDef.h"



EFI_TIME Nanoseconds100ToEfiTime(UINT64 Nanoseconds100)
{
	EFI_TIME EfiTime;

	// Convert 100 nanoseconds to seconds
	UINT64 Seconds = Nanoseconds100 / 10000000;

	// January 1, 1601 是 UEFI 时间戳的起始时间
	EfiTime.Year = 1601;
	EfiTime.Month = 1;
	EfiTime.Day = 1;

	// 计算秒数对应的年、月、日、时、分、秒
	EfiTime.Second = (UINT8)(Seconds % 60);
	Seconds /= 60; // 转换为分钟
	EfiTime.Minute = (UINT8)(Seconds % 60);
	Seconds /= 60; // 转换为小时
	EfiTime.Hour = (UINT8)(Seconds % 24);
	Seconds /= 24; // 转换为天

	// 计算年、月、日
	UINT32 Days = (UINT32)Seconds;
	while (Days > 365) {
		if ((EfiTime.Year % 4 == 0 && EfiTime.Year % 100 != 0) || (EfiTime.Year % 400 == 0)) {
			if (Days >= 366) {
				Days -= 366;
				EfiTime.Year++;
			}
		}
		else {
			Days -= 365;
			EfiTime.Year++;
		}
	}

	// 计算月份和日期
	static const UINT8 DaysPerMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	for (EfiTime.Month = 1; EfiTime.Month <= 12; EfiTime.Month++) {
		UINT8 DaysInMonth = DaysPerMonth[EfiTime.Month];
		if (EfiTime.Month == 2 && ((EfiTime.Year % 4 == 0 && EfiTime.Year % 100 != 0) || (EfiTime.Year % 400 == 0))) {
			// 闰年二月
			DaysInMonth = 29;
		}

		if (Days < DaysInMonth) {
			EfiTime.Day = (UINT8)(Days + 1);
			break;
		}

		Days -= DaysInMonth;
	}

	return EfiTime;
}

EFI_STATUS EFIAPI ModifyFileTime(CHAR16* path,FILE_BASIC_INFORMATION fbi)
{
	EFI_STATUS Status;

	// Locate the Simple File System Protocol
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
	Status = gBS->LocateProtocol(
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		(VOID**)&FileSystem
	);
	if (EFI_ERROR(Status)) {
		Print(L"MFT:Locate SimpleFileSystem Protocol Fail %r\n", Status);
		return Status;
	}

	// Open the file
	EFI_FILE_PROTOCOL* File;
	Status = FileSystem->OpenVolume(FileSystem, &File);
	if (EFI_ERROR(Status)) {
		Print(L"MFT:Open the Volume %r\n", Status);
		return Status;
	}


	// Create a new file (or open an existing one)
	Status = File->Open(
		File,
		&File,
		path,
		EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
		0
	);
	if (EFI_ERROR(Status)) {
		Print(L"MFT:Open the file %r\n", Status);
		return Status;
	}


	// Get the existing file info
	EFI_FILE_INFO* FileInfo;
	UINTN FileInfoSize = 0;
	Status = File->GetInfo(
		File,
		&gEfiFileInfoGuid,
		&FileInfoSize,
		NULL
	);


	if (Status == EFI_BUFFER_TOO_SMALL) {
		Status = gBS->AllocatePool(EfiLoaderData, FileInfoSize, (VOID**)&FileInfo);
		if (EFI_ERROR(Status)) {
			File->Close(File);
			Print(L"MFT:AllocatePool %r\n", Status);
			return Status;
		}

		// Get the file info
		Status = File->GetInfo(
			File,
			&gEfiFileInfoGuid,
			&FileInfoSize,
			FileInfo
		);
		if (EFI_ERROR(Status)) {
			gBS->FreePool(FileInfo);
			File->Close(File);
			Print(L"MFT:Get the file info %r\n", Status);
			return Status;
		}

		// Modify the timestamp in the FileInfo structure (modification time in this example)
		EFI_TIME changeTime = Nanoseconds100ToEfiTime(fbi.ChangeTime.QuadPart);
		FileInfo->ModificationTime = changeTime;
		EFI_TIME createTime = Nanoseconds100ToEfiTime(fbi.CreationTime.QuadPart);
		FileInfo->CreateTime = createTime;
		EFI_TIME accessTime = Nanoseconds100ToEfiTime(fbi.LastAccessTime.QuadPart);
		FileInfo->LastAccessTime = accessTime;


		// Set the modified file info
		Status = File->SetInfo(
			File,
			&gEfiFileInfoGuid,
			FileInfoSize,
			FileInfo
		);

		gBS->FreePool(FileInfo);
	}

	// Close the file
	File->Close(File);
	return Status;
}

EFI_STATUS EFIAPI RestoreBootMgfw(VOID)
{
	UINTN HandleCount = NULL;
	EFI_STATUS Result;
	EFI_HANDLE* Handles = NULL;
	EFI_FILE_HANDLE VolumeHandle;
	EFI_FILE_HANDLE BootMgfwHandle;
	EFI_FILE_IO_INTERFACE* FileSystem = NULL;

	if (EFI_ERROR((Result = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleCount, &Handles))))
	{
		Print(L"error getting file system handles -> 0x%p\n", Result);
		return Result;
	}

	for (UINT32 Idx = 0u; Idx < HandleCount; ++Idx)
	{
		if (EFI_ERROR((Result = gBS->OpenProtocol(Handles[Idx], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL))))
		{
			Print(L"error opening protocol -> 0x%p\n", Result);
			return Result;
		}

		if (EFI_ERROR((Result = FileSystem->OpenVolume(FileSystem, &VolumeHandle))))
		{
			Print(L"error opening file system -> 0x%p\n", Result);
			return Result;
		}

		

		if (!EFI_ERROR((Result = VolumeHandle->Open(VolumeHandle, &BootMgfwHandle, g_MapperContext.bootmgfw.path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY))))
		{
			VolumeHandle->Close(VolumeHandle);
			EFI_FILE_PROTOCOL* BootMgfwFile = NULL;
			EFI_DEVICE_PATH* BootMgfwPathProtocol = FileDevicePath(Handles[Idx], g_MapperContext.bootmgfw.path);


			// open bootmgfw as read/write then delete it...
			if (EFI_ERROR((Result = EfiOpenFileByDevicePath(&BootMgfwPathProtocol, &BootMgfwFile, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, NULL))))
			{
				Print(L"error opening bootmgfw... reason -> %r\n", Result);
				return Result;
			}

			if (EFI_ERROR((Result = BootMgfwFile->Delete(BootMgfwFile))))
			{
				Print(L"error deleting bootmgfw... reason -> %r\n", Result);
				return Result;
			}
			

			// open bootmgfw.efi.backup
			BootMgfwPathProtocol = FileDevicePath(Handles[Idx], g_MapperContext.bootmgfwBackupPath);
			if (EFI_ERROR((Result = EfiOpenFileByDevicePath(&BootMgfwPathProtocol, &BootMgfwFile, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, NULL))))
			{
				Print(L"failed to open backup file... reason -> %r\n", Result);
				return Result;
			}

			EFI_FILE_INFO* FileInfoPtr = NULL;
			UINTN FileInfoSize = NULL;

			// get the size of bootmgfw.efi.backup...
			if (EFI_ERROR((Result = BootMgfwFile->GetInfo(BootMgfwFile, &gEfiFileInfoGuid, &FileInfoSize, NULL))))
			{
				if (Result == EFI_BUFFER_TOO_SMALL)
				{
					gBS->AllocatePool(EfiBootServicesData, FileInfoSize, &FileInfoPtr);
					if (EFI_ERROR(Result = BootMgfwFile->GetInfo(BootMgfwFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfoPtr)))
					{
						Print(L"get backup file information failed... reason -> %r\n", Result);
						return Result;
					}
				}
				else
				{
					Print(L"Failed to get file information... reason -> %r\n", Result);
					return Result;
				}
			}

			VOID* BootMgfwBuffer = NULL;
			UINTN BootMgfwSize = FileInfoPtr->FileSize;
			gBS->AllocatePool(EfiBootServicesData, FileInfoPtr->FileSize, &BootMgfwBuffer);

			// read the backup file into an allocated pool...
			if (EFI_ERROR((Result = BootMgfwFile->Read(BootMgfwFile, &BootMgfwSize, BootMgfwBuffer))))
			{
				Print(L"Failed to read backup file into buffer... reason -> %r\n", Result);
				return Result;
			}

			// delete the backup file...
			// if (EFI_ERROR((Result = BootMgfwFile->Delete(BootMgfwFile))))
			// {
			// 	Print(L"unable to delete backup file... reason -> %r\n", Result);
			// 	return Result;
			// }

			// create a new bootmgfw file...
			BootMgfwPathProtocol = FileDevicePath(Handles[Idx], g_MapperContext.bootmgfw.path);
			if (EFI_ERROR((Result = EfiOpenFileByDevicePath(&BootMgfwPathProtocol, &BootMgfwFile, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, EFI_FILE_SYSTEM))))
			{
				Print(L"unable to create new bootmgfw on disk... reason -> %r\n", Result);
				return Result;
			}

			// write the data from the backup file to the new bootmgfw file...
			BootMgfwSize = FileInfoPtr->FileSize;
			if (EFI_ERROR((Result = BootMgfwFile->Write(BootMgfwFile, &BootMgfwSize, BootMgfwBuffer))))
			{
				Print(L"unable to write to newly created bootmgfw.efi... reason -> %r\n", Result);
				return Result;
			}

			BootMgfwFile->Close(BootMgfwFile);
			gBS->FreePool(FileInfoPtr);
			gBS->FreePool(BootMgfwBuffer);

			// reset time
			ModifyFileTime(g_MapperContext.bootParent.path, g_MapperContext.bootParent.fbi);
			ModifyFileTime(g_MapperContext.boot.path, g_MapperContext.boot.fbi);
			ModifyFileTime(g_MapperContext.bootmgfw.path, g_MapperContext.bootmgfw.fbi);

			return EFI_SUCCESS;
		}

		if (EFI_ERROR((Result = gBS->CloseProtocol(Handles[Idx], &gEfiSimpleFileSystemProtocolGuid, gImageHandle, NULL))))
		{
			Print(L"error closing protocol -> 0x%p\n", Result);
			return Result;
		}
	}
	gBS->FreePool(Handles);

	return EFI_ABORTED;
}





EFI_STATUS EFIAPI InstallBootMgfwHooks(EFI_HANDLE ImageHandle)
{
	// 进入 bootmgfw.efi

	EFI_STATUS Result = EFI_SUCCESS;
	EFI_LOADED_IMAGE* BootMgfw = NULL;


	if (EFI_ERROR(Result = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&BootMgfw)))
		return Result;


	VOID* ArchStartBootApplication = FindPattern(BootMgfw->ImageBase,BootMgfw->ImageSize,START_BOOT_APPLICATION_SIG,START_BOOT_APPLICATION_MASK);


	if (!ArchStartBootApplication)
		return EFI_NOT_FOUND;

#if WINVER == 2302
	MakeInlineHook(&g_Hooks[BootMgfwShitHook], ArchStartBootApplication, &ArchStartBootApplicationHook, TRUE);
#endif
	return EFI_SUCCESS;
}


UINT64 BlUtlCopySmBiosTableHook(PSMBIOS3_TABLE_HEADER* HeaderCopy)
{
	DisableInlineHook(&g_Hooks[BlUtlCopySmBiosTable]);
	UINT64 Result = ((UINT64(*)(PSMBIOS3_TABLE_HEADER*))g_Hooks[BlUtlCopySmBiosTable].Address)(HeaderCopy);
	EnableInlineHook(&g_Hooks[BlUtlCopySmBiosTable]);

	if (Result == 0)
	{
		PSMBIOS3_TABLE_HEADER Header = *HeaderCopy;
		
		unsigned char* currentAddress = NULL;

		BlMmMapPhysicalAddressExFunc(&currentAddress, Header->StructureTableAddress, Header->StructureTableMaximumSize, 0, 0);
		UINT64 endAddress = (UINT64)currentAddress + Header->StructureTableMaximumSize;

		while (1)
		{
			SMBIOS_STRUCTURE* header = (SMBIOS_STRUCTURE*)currentAddress;
			if (header->Type == 127 && header->Length == 4)
				break;

			// Process Header
			{
				if (header != 0 || header->Length != 0)
				{
					if (header->Type == 1)
					{
						SMBIOS_TABLE_TYPE1* type1 = (SMBIOS_TABLE_TYPE1*)header;
						type1->Uuid.Data1 = HashFunction(&type1->Uuid.Data1,sizeof(UINT32),g_MapperContext.RandomSeed);
						type1->Uuid.Data2 = (UINT16)HashFunction(&type1->Uuid.Data2,sizeof(UINT16),g_MapperContext.RandomSeed);
						type1->Uuid.Data3 = (UINT16)HashFunction(&type1->Uuid.Data3,sizeof(UINT16),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[0] = (UINT8)HashFunction(&type1->Uuid.Data4[0],sizeof(UINT8),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[1] = (UINT8)HashFunction(&type1->Uuid.Data4[1],sizeof(UINT8),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[2] = (UINT8)HashFunction(&type1->Uuid.Data4[2],sizeof(UINT8),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[3] = (UINT8)HashFunction(&type1->Uuid.Data4[3],sizeof(UINT8),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[4] = (UINT8)HashFunction(&type1->Uuid.Data4[4],sizeof(UINT8),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[5] = (UINT8)HashFunction(&type1->Uuid.Data4[5],sizeof(UINT8),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[6] = (UINT8)HashFunction(&type1->Uuid.Data4[6],sizeof(UINT8),g_MapperContext.RandomSeed);
						type1->Uuid.Data4[7] = (UINT8)HashFunction(&type1->Uuid.Data4[7],sizeof(UINT8),g_MapperContext.RandomSeed); 
					}
				}
			}

			char* ptr = (char*)(currentAddress) + header->Length;
			while (0 != (*ptr | *(ptr + 1))) ptr++;
			ptr += 2;
			if ((UINT64)ptr >= endAddress)
				break;
			currentAddress = (unsigned char*)ptr;
		}

		BlMmUnmapVirtualAddressExFunc(currentAddress, Header->StructureTableMaximumSize, 0);

	}

	return Result;
}



EFI_STATUS EFIAPI ArchStartBootApplicationHook(VOID* AppEntry, VOID* ImageBase, UINT32 ImageSize, UINT8 BootOption, VOID* ReturnArgs)
{
	// ImageBase 是 winload.efi

	DisableInlineHook(&g_Hooks[BootMgfwShitHook]);

	VOID* LdrLoadImage = GetExport(ImageBase, "BlLdrLoadImage");
	VOID* BlImgGetPEImageSizePtr = GetExport(ImageBase, "BlImgGetPEImageSize");
	BlMmAllocatePagesInRangeFunc = (BlMmAllocatePagesInRangeDef)GetExport(ImageBase, "BlMmAllocatePagesInRange");
	BlMmAllocatePhysicalPagesInRangeFunc = (BlMmAllocatePhysicalPagesInRangeDef)GetExport(ImageBase, "BlMmAllocatePhysicalPagesInRange");
	BlMmFreePhysicalPagesFunc = (BlMmFreePhysicalPagesDef)GetExport(ImageBase, "BlMmFreePhysicalPages");
	BlMmMapPhysicalAddressExFunc = (BlMmMapPhysicalAddressExDef)GetExport(ImageBase, "BlMmMapPhysicalAddressEx");
	BlMmUnmapVirtualAddressExFunc = (BlMmUnmapVirtualAddressExDef)GetExport(ImageBase, "BlMmUnmapVirtualAddressEx");
	BlUtlCheckSumFunc = (BlUtlCheckSumDef)GetExport(ImageBase, "BlUtlCheckSum");


	VOID* ImgAllocateImageBuffer = FindPattern(ImageBase,ImageSize,ALLOCATE_IMAGE_BUFFER_SIG,ALLOCATE_IMAGE_BUFFER_MASK);


	// 获取 g_BlpArchSwitchContext;
	PVOID BlpArchSwitchContextPattner = FindPattern(ImageBase,ImageSize, BLP_ARCH_SWITCH_CONTEXT_SIG, BLP_ARCH_SWITCH_CONTEXT_MASK);
	if (BlpArchSwitchContextPattner)
	{
		g_BlpArchSwitchContext = (BlpArchSwitchContext_t)BlpArchSwitchContextPattner;
	}


	// logo
	ChangeLogo(ImageBase,ImageSize);


	// smbios
	if (g_MapperContext.RunWithSpoofer) 
	{
		//	AC DE 48 00 ? ? ? ? E8
		//	.text:000000018010C80C 66 83 61 14 00                and     word ptr [rcx+14h], 0
		//	.text:000000018010C811 C7 41 10 AC DE 48 00          mov     dword ptr[rcx + 10h], 48DEACh
		// 	.text:000000018010C818 48 8D 4D 67                   lea     rcx, [rbp + 57h + arg_0]
		// 	.text:000000018010C81C E8 2B EC FA FF                call    BlUtlCopySmBiosTable
		// 	.text:000000018010C81C
		// 	.text:000000018010C821 8B F8                         mov     edi, eax
		// 	.text:000000018010C823 85 C0                         test    eax, eax

		UINT64 BlUtlCopySmBiosTablePattner = FindPattern(ImageBase, ImageSize, "\xAC\xDE\x48\x00\x00\x00\x00\x00\xE8", "xxxx????x");
		UINT64 BlUtlCopySmBiosTablePtr = RESOLVE_RVA((BlUtlCopySmBiosTablePattner + 8), 5, 1);

		MakeInlineHook(&g_Hooks[BlUtlCopySmBiosTable], BlUtlCopySmBiosTablePtr, &BlUtlCopySmBiosTableHook, TRUE);
	}


	MakeInlineHook(&g_Hooks[WinLoadImageShitHook], LdrLoadImage, &BlLdrLoadImageHook, TRUE);
	MakeInlineHook(&g_Hooks[WinLoadAllocateImageHook], RESOLVE_RVA(ImgAllocateImageBuffer, 5, 1), &BlImgAllocateImageBufferHook, TRUE);
	MakeInlineHook(&g_Hooks[BlImgGetPEImageSize], BlImgGetPEImageSizePtr, &BlImgGetPEImageSizeHook, TRUE);


	return ((IMG_ARCH_START_BOOT_APPLICATION)g_Hooks[BootMgfwShitHook].Address)(AppEntry, ImageBase, ImageSize, BootOption, ReturnArgs);
}

