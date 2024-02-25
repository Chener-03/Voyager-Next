
#include "Pch.h"
#include "BootMgfw.h" 
#include "ShareData.h"

CHAR8* gEfiCallerBaseName = "VoyagerNext";
const UINT32 _gUefiDriverRevision = 0xA00;


#pragma data_seg(".data")
__declspec(dllexport)  MapperContext g_MapperContext = { 0 };
#pragma data_seg()


EFI_STATUS EFIAPI UefiUnload(EFI_HANDLE ImageHandle)
{
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
	gImageHandle = ImageHandle;
	gST->ConOut->ClearScreen(gST->ConOut);

	RestoreBootMgfw();

	UefiSetResolution(2560, 1440);
	// UefiSetResolution(1280, 720);

	Print(L"magic is %d \n",g_MapperContext.test);
	Print(L"winver is %d \n",WINVER);
	Print(L"(If the magic/winver does not match, it indicates that the efi file is incorrect.)\n\n");
	Print(L"Loading...  3sec \n");
	gBS->Stall(SEC_TO_MS(1));
	Print(L"Loading...  2sec \n");
	gBS->Stall(SEC_TO_MS(1));
	Print(L"Loading...  1sec \n");
	gBS->Stall(SEC_TO_MS(1));

	InstallBootMgfwHooks(ImageHandle);

	return EFI_SUCCESS;
}