#include "vutils.h"





namespace VUtils
{
	Command GetCommand(GuestVirtureAddress command_ptr)
	{
		u64 guest_dirbase;
		__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);

		// 页框号(pml4_pfn) << 12 = 实际物理地址
		guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

		const auto command_page = HV::MapGuestVirt(guest_dirbase, command_ptr);

		return *reinterpret_cast<Command*>(command_page);
	}

	void SetCommand(GuestVirtureAddress command_ptr, Command& command_data)
	{
		u64 guest_dirbase;
		__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);

		// 页框号(pml4_pfn) << 12 = 实际物理地址
		guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

		const auto command_page = HV::MapGuestVirt(guest_dirbase, command_ptr);

		*reinterpret_cast<Command*>(command_page) = command_data;
	}
}
