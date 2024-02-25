#pragma once
#include "Pch.h"



#if WINVER == 2302

/**
	.text:00000000100BC504 48 8B C4                      mov     rax, rsp
	.text:00000000100BC507 48 89 58 20                   mov     [rax+20h], rbx
	.text:00000000100BC50B 44 89 40 18                   mov     [rax+18h], r8d
	.text:00000000100BC50F 48 89 50 10                   mov     [rax+10h], rdx
	.text:00000000100BC513 48 89 48 08                   mov     [rax+8], rcx
	.text:00000000100BC517 55                            push    rbp
	.text:00000000100BC518 56                            push    rsi
	.text:00000000100BC519 57                            push    rdi
	.text:00000000100BC51A 41 54                         push    r12
	.text:00000000100BC51C 41 55                         push    r13
	.text:00000000100BC51E 41 56                         push    r14
	.text:00000000100BC520 41 57                         push    r15
	.text:00000000100BC522 48 8D 68 A9                   lea     rbp, [rax-57h]
 */
 // 48 8B C4 48 89 58 20 44 89 40 18 48 89 50 10 48 89 48 08 55 56 57 41 54 41 55 41 56 41 57 48 8D 68 A9

#define START_BOOT_APPLICATION_SIG "\x48\x8B\xC4\x48\x89\x58\x20\x44\x89\x40\x18\x48\x89\x50\x10\x48\x89\x48\x08\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\x68\xA9"
#define START_BOOT_APPLICATION_MASK "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

#endif


static_assert(sizeof(START_BOOT_APPLICATION_SIG) == sizeof(START_BOOT_APPLICATION_MASK), "signature and mask size's dont match...");

#define WINDOWS_BOOTMGFW_PATH L"\\efi\\microsoft\\boot\\bootmgfw.efi"
#define WINDOWS_BOOTMGFW_BACKUP_PATH L"\\efi\\microsoft\\boot\\bootmgfw.efi.backup"


typedef EFI_STATUS(EFIAPI* IMG_ARCH_START_BOOT_APPLICATION)(VOID*, VOID*, UINT32, UINT8, VOID*);






EFI_STATUS EFIAPI RestoreBootMgfw(VOID);


EFI_STATUS EFIAPI InstallBootMgfwHooks(EFI_HANDLE BootMgfwPath);


EFI_STATUS EFIAPI ArchStartBootApplicationHook(VOID* AppEntry, VOID* ImageBase, UINT32 ImageSize, UINT8 BootOption, VOID* ReturnArgs);

