#include "Mapper.h" 
#include "KUtils.h"
#include "Pe.h"
#include "../UEFI/EUtils.h"


VOID MapSysFile()
{
	PVOID data = nullptr;
	UINT32 size = 0;
	auto res = ReadFile((char*) & g_PostCallMapperContext.postCallLoadSysPath[0], &data, &size);
	if (!res || size == 0 || data == nullptr)
	{
		DbgPrintEx(77, 0, "MapSysFile Load File Fail!\n");
		return;
	}


	// У��peͷ
	auto header = GetNtHeaders(data);
	if (!header)
	{
		goto ENDP;
	}


	// չ�����ڴ�
	auto memsize = header->OptionalHeader.SizeOfImage + 0x200;

	auto memptr = KAlloc(memsize, true);
	if (!memptr)
	{
		goto ENDP;
	}

	memcpy(memptr, data, header->OptionalHeader.SizeOfHeaders);
	const PIMAGE_SECTION_HEADER current_image_section = IMAGE_FIRST_SECTION(header);
	for (auto i = 0; i < header->FileHeader.NumberOfSections; ++i) {
		if ((current_image_section[i].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) > 0)
			continue;
		auto local_section = reinterpret_cast<void*>(reinterpret_cast<UINT64>(memptr) + current_image_section[i].VirtualAddress);
		memcpy(local_section, reinterpret_cast<void*>(reinterpret_cast<UINT64>(data) + current_image_section[i].PointerToRawData), current_image_section[i].SizeOfRawData);
	}

	// �޸�reload
	FixRelocImage(memptr);

	FixSecurityCookie(memptr, (UINT64)g_PostCallMapperContext.KernelModuleBase);


// �޸�import
// call ep

	
ENDP:
	if (data)
	{
		KFree(data);
	}
}