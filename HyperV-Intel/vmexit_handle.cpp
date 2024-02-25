#include "vutils.h"
#include "hv.h"
#include "ept.h"

HvContext g_HvContext = {0};

#if WINVER > 1803
void vmexit_handler(PVmContext* context, void* unknown)
#else
void vmexit_handler(PVmContext context, void* unknown)
#endif
{
#if WINVER > 1803
	PVmContext guest_registers = *context;
#else
	pcontext_t guest_registers = context;
#endif


	size_t vmexit_reason;
	__vmx_vmread(VMCS_EXIT_REASON, &vmexit_reason);

	if (vmexit_reason == VMX_EXIT_REASON_EXECUTE_CPUID)
	{
		if (guest_registers->rcx == VMEXIT_KEY)
		{
			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::check_is_load)
			{
				guest_registers->rax = VMEXIT_KEY;
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::init_page_tables)
			{
				guest_registers->rax = (u64)HV::InitPageTable();
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::get_dirbase)
			{
				Command command_data = VUtils::GetCommand(guest_registers->r8);

				u64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);

				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;
				command_data.dirbase = guest_dirbase;
				guest_registers->rax = (u64)VMX_ROOT_RESULT::success;

				VUtils::SetCommand(guest_registers->r8, command_data);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::translate_virture_address)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);

				u64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);
				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

				command_data.translate_virt.phys_addr = HV::TranslateGuestVirtual(guest_dirbase, command_data.translate_virt.virt_src);

				guest_registers->rax = (u64)VMX_ROOT_RESULT::success;

				VUtils::SetCommand(guest_registers->r8, command_data);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::read_guest_phys)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);

				u64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);
				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

				guest_registers->rax = (u64)HV::ReadGuestPhys(guest_dirbase,command_data.copy_phys.phys_addr,command_data.copy_phys.buffer,command_data.copy_phys.size);

				VUtils::SetCommand(guest_registers->r8, command_data);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::write_guest_phys)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);

				u64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);
				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

				guest_registers->rax = (u64)HV::WriteGuestPhys(guest_dirbase,command_data.copy_phys.phys_addr,command_data.copy_phys.buffer,command_data.copy_phys.size);

				VUtils::SetCommand(guest_registers->r8, command_data);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::copy_guest_virt)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);
				auto virt_data = command_data.copy_virt;
				guest_registers->rax = (u64)HV::CopyGuestVirt(virt_data.dirbase_src,virt_data.virt_src,
					virt_data.dirbase_dest,virt_data.virt_dest,virt_data.size);
			}

			// ept
			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::add_shadow_page)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);

				u64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);

				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;


				GuestVirtureAddress uAddr = command_data.addShadowPage.uVirtualAddrToHook;
				GuestVirtureAddress virtualRead = command_data.addShadowPage.uPageRead;
				GuestVirtureAddress virtualExecute = command_data.addShadowPage.uPageExecute;

				GuestPhysicalAddress uPageRead = HV::TranslateGuestVirtual(guest_dirbase, virtualRead, MapType::map_src); //save the page for read
				GuestPhysicalAddress uPageExec = HV::TranslateGuestVirtual(guest_dirbase, virtualExecute, MapType::map_src); //save the page for exec

				ept_pointer eptp;
				HV::PhysicalAddress guest_phys{ uPageExec };
				__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
				VoyagerEptAddFakePage(uAddr, uPageRead, uPageExec); //record hook information
				split_2mb_to_4kb(eptp, uPageExec & EPT_PD_MASK, uPageExec & EPT_PD_MASK);
				//it's all 4k page now
				changeEPTAttribute(eptp, uPageExec, true);//set the page attribute to exec only

				guest_registers->rax = (u64)VMX_ROOT_RESULT::success;
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::add_shadow_page_phys)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);
				GuestVirtureAddress uAddr = command_data.addShadowPagePhys.uVirtualAddrToHook;

				GuestPhysicalAddress uPageRead = command_data.addShadowPagePhys.uPageRead; //save the page for read
				GuestPhysicalAddress uPageExec = command_data.addShadowPagePhys.uPageExecute; //save the page for exec

				ept_pointer eptp;
				HV::PhysicalAddress guest_phys{ uPageExec };
				__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
				VoyagerEptAddFakePage(uAddr, uPageRead, uPageExec); //record hook information
				split_2mb_to_4kb(eptp, uPageExec& EPT_PD_MASK, uPageExec& EPT_PD_MASK);
				//it's all 4k page now
				changeEPTAttribute(eptp, uPageExec, true);//set the page attribute to exec only

				guest_registers->rax = (u64)VMX_ROOT_RESULT::success;
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::delete_shadow_page)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);

				u64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);
				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

				u64 uAddr = command_data.deleteShaowPage.uVirtualAddrToHook; //begin to search for the page

				PSharedShadowHookData pShadowHook = (PSharedShadowHookData)&ShadowHookData;
				PHookInformation pInfo = NULL;
				pInfo = ShpFindPatchInfoByAddress(pShadowHook, (void*)uAddr);
				if (pInfo)
				{
					ept_pointer eptp;
					__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
					u64 uPhyExe = pInfo->pa_base_for_exec;
					u64 uPhyRW = pInfo->pa_base_for_rw;
					map_4k(eptp, uPhyExe, uPhyExe); //remap to the exec page,and set page attribute to R/W only
					VoyagerEptDelteFakePage(uAddr); //delete
					pInfo = ShpFindPatchInfoBy2MPage((PSharedShadowHookData)&ShadowHookData, (void*)uAddr);
					if (!pInfo) //if no other hook information in 2M,then delete the resource
					{ 
						merge_4kb_to_2mb(eptp, uPhyExe & EPT_PD_MASK, uPhyExe & EPT_PD_MASK);
					}
					guest_registers->rax = (u64)VMX_ROOT_RESULT::success;
				}
				else
				{
					guest_registers->rax = (u64)VMX_ROOT_RESULT::ept_error;
				}

			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::unhide_shadow_page)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);
				u64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);
				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;

				u64 uAddr = command_data.unHideShaowPage.uVirtualAddrToHook; //begin to search for the page


				PSharedShadowHookData pShadowHook = (PSharedShadowHookData)&ShadowHookData;
				PHookInformation pInfo = NULL;
				pInfo = ShpFindPatchInfoByAddress(pShadowHook, (void*)uAddr);
				if (pInfo)
				{
					ept_pointer eptp;
					__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
					u64 uPhyExe = pInfo->pa_base_for_exec;
					u64 uPhyRW = pInfo->pa_base_for_rw;
					map_4k(eptp, uPhyExe, uPhyExe); //remap to exec,and set page attribute to RWE

					guest_registers->rax = (u64)VMX_ROOT_RESULT::success;
				}
				else
				{
					guest_registers->rax = (u64)VMX_ROOT_RESULT::ept_error;
				}
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::disable_page_protection)
			{
				auto command_data = VUtils::GetCommand(guest_registers->r8);
				GuestPhysicalAddress uPagePhyAddr = command_data.disablePageProtection.phys_addr;
				ept_pointer eptp;
				__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
				disablePageProtection(eptp, uPagePhyAddr);
				guest_registers->rax = (u64)VMX_ROOT_RESULT::success;
			}



			// ÇÐ»»»Øno-root
			size_t rip, exec_len;
			__vmx_vmread(VMCS_GUEST_RIP, &rip);
			__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &exec_len);
			__vmx_vmwrite(VMCS_GUEST_RIP, rip + exec_len);
			return;
		}
	}


	if (vmexit_reason == VMX_EXIT_REASON_EPT_VIOLATION)
	{
		EptViolationQualification exit_qualification;
		__vmx_vmread(VMCS_EXIT_QUALIFICATION, (size_t*)&exit_qualification);
		u64 fault_pa = 0, fault_va = 0;
		__vmx_vmread(VMCS_GUEST_PHYSICAL_ADDRESS, (size_t*)&fault_pa);
		if (exit_qualification.fields.valid_guest_linear_address)
		{
			__vmx_vmread(VMCS_EXIT_GUEST_LINEAR_ADDRESS, (size_t*)&fault_va);
		}
		else
		{
			fault_va = 0;
		}

		if (exit_qualification.fields.ept_readable || exit_qualification.fields.ept_writeable || exit_qualification.fields.ept_executable)
		{
			ept_pointer eptp;

			__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
			// EPT entry is present. Permission violation.
			if (exit_qualification.fields.caused_by_translation)
			{
				bool isHandled = VoyagerHandleEptViolation(&exit_qualification, (void*)fault_va);//replace
				if (isHandled)
				{
					UtilInveptGlobal(eptp);
					return;
				}
			}

		}
	}


	// call original vmexit handler...
	// reinterpret_cast<vmexit_handler_t>( reinterpret_cast<u64>(&vmexit_handler) - g_HvContext.vmexit_handler_rva)(context, unknown);

	((vmexit_handler_t)((u64)(&vmexit_handler) - g_HvContext.vmexit_handler_rva))(context, unknown);
}