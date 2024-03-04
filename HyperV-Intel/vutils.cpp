#include "VUtils.h"
#include <intrin.h>
#include "Memory.h"




namespace VUtils
{
	Command GetCommand(GVA command_ptr)
	{
		UINT64 guest_dirbase;
		__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);

		guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

		const auto command_page = GvaToHva(guest_dirbase, command_ptr,MAP_MEMTORY_INDEX::P1);

		return *reinterpret_cast<Command*>(command_page);
	}

	void SetCommand(GVA command_ptr, Command& command_data)
	{
		UINT64 guest_dirbase;
		__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);

		guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

		const auto command_page = GvaToHva(guest_dirbase, command_ptr, MAP_MEMTORY_INDEX::P1);

		*reinterpret_cast<Command*>(command_page) = command_data;
	}


	UINT32 GetCurrentCpuIndex()
	{
		cpuid_eax_01 cpuid_value;
		__cpuid((int*)&cpuid_value, 1);
		return cpuid_value.cpuid_additional_information.initial_apic_id;
	}

}
