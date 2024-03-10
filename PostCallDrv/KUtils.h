#pragma once
#include "Defs.h"
#include "Pch.h"
#include "PEStructs.h"
#include "ShareData.h"



UINT64 AsciiStrCmp(CHAR8* FirstString, CHAR8* SecondString);



VOID Sleep(int msec);


namespace Call
{

	template<typename RetVal = UINT64, typename... Args>
	RetVal CallFunctionAddress(PVOID Address, Args... Vars) {
		typedef RetVal(*FunctionTemplate)(Args...);
		return ((FunctionTemplate)Address)(Vars...);
	}


	template<typename RetVal = UINT64, typename... Args>
	RetVal CallFunction(LPSTR FuncName, Args... Vars)
	{
		auto address = GetExport(g_PostCallMapperContext.KernelModuleBase, FuncName);
		if (g_PostCallMapperContext.KernelModuleBase == NULL || address == NULL)
		{
			return {};
		}
		return ((RetVal(*)(Args...))address)(Vars...);
	}
}



namespace Str
{
	template <typename StrType, typename StrType2>
	bool StrICmp(StrType Str, StrType2 InStr, bool Two)  //two 是否完全相同
	{
		#define ToLower(Char) ((Char >= 'A' && Char <= 'Z') ? (Char + 32) : Char)

		if (!Str || !InStr)
			return false;

		wchar_t c1, c2; do {
			c1 = *Str++; c2 = *InStr++;
			c1 = ToLower(c1); c2 = ToLower(c2);
			if (!c1 && (Two ? !c2 : 1))
				return true;
		} while (c1 == c2);

		return false;
	}

	template <typename StrType>
	int StrLen(StrType Str) {
		if (!Str) return 0;
		StrType Str2 = Str;
		while (*Str2) *Str2++;
		return (int)(Str2 - Str);
	}

	template <typename StrType, typename StrType2>
	void StrCpy(StrType Dst, StrType2 Src) {
		if (!Dst || !Src) return;
		while (*Src) {
			*Dst++ = *Src++;
		} *Dst = 0;
	}
}



PVOID KAlloc(UINT32 Size, bool exec = false);

VOID KFree(VOID* Buffer);


BOOLEAN ReadFile(char* path, __out void** data, __out UINT32* size);



UINT64 GetKernelModuleAddress(const char* module_name);

