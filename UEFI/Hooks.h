#pragma once
#include "Pch.h"
#include "EUtils.h"


typedef void* PVOID;

enum HOOK_METHOD_ID
{
	BootMgfwShitHook = 0,
	WinLoadImageShitHook,
	WinLoadAllocateImageHook,
	BgpGxParseBitmap,
	BlImgGetPEImageSize,
	BlUtlCopySmBiosTable
};

typedef struct _INLINE_HOOK
{
	unsigned char Code[14];
	unsigned char JmpCode[14];

	void* Address;
	void* HookAddress;
} INLINE_HOOK, * PINLINE_HOOK_T;

extern INLINE_HOOK g_Hooks[50];

VOID MakeInlineHook(PINLINE_HOOK_T Hook, VOID* HookFrom, VOID* HookTo, BOOLEAN Install);
VOID EnableInlineHook(PINLINE_HOOK_T Hook);
VOID DisableInlineHook(PINLINE_HOOK_T Hook);
