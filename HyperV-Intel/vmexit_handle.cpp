#include <intrin.h>
#include "ShareData.h"
#include "Memory.h"
#include "VUtils.h"
#include "Debug.h"

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
			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::CHECK_LOAD)
			{
				guest_registers->rax = VMEXIT_KEY;
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::INIT_PAGE_TABLE)
			{
				DBG::SerialPortInitialize(PORT_NUM, 9600);
				auto res = InitPageTable();
				guest_registers->rax = (UINT64)(res?VMX_ROOT_RESULT::SUCCESS : VMX_ROOT_RESULT::INIT_ERROR);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::CURRENT_DIRBASE)
			{
				Command cmd = VUtils::GetCommand(guest_registers->r8);
				UINT64 guest_dirbase;
				__vmx_vmread(VMCS_GUEST_CR3, &guest_dirbase);
				guest_dirbase = cr3{ guest_dirbase }.pml4_pfn << 12;
				cmd.DirbaseData.Dirbase = guest_dirbase;
				guest_registers->rax = (UINT64)VMX_ROOT_RESULT::SUCCESS;
				VUtils::SetCommand(guest_registers->r8, cmd);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::TRANSLATE_GVA2GPA)
			{
				Command cmd = VUtils::GetCommand(guest_registers->r8);
				GPA gpa = GvaToGpa(cmd.TranslateData.dirbase, cmd.TranslateData.gva, MAP_MEMTORY_INDEX::P1);
				cmd.TranslateData.gpa = gpa;
				guest_registers->rax = (UINT64)VMX_ROOT_RESULT::SUCCESS;
				VUtils::SetCommand(guest_registers->r8, cmd);
			}


			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::READ_GUEST_PHY)
			{
				Command cmd = VUtils::GetCommand(guest_registers->r8);
				ReadGuestPhy(cmd.CopyData.DestDirbase, cmd.CopyData.SrcGpa, cmd.CopyData.DestGva, cmd.CopyData.Size);
				guest_registers->rax = (UINT64)VMX_ROOT_RESULT::SUCCESS;
				VUtils::SetCommand(guest_registers->r8, cmd);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::WRITE_GUEST_PHY)
			{
				Command cmd = VUtils::GetCommand(guest_registers->r8);
				WriteGuestPhy(cmd.CopyData.SrcDirbase, cmd.CopyData.DestGpa, cmd.CopyData.SrcGva, cmd.CopyData.Size);
				guest_registers->rax = (UINT64)VMX_ROOT_RESULT::SUCCESS;
				VUtils::SetCommand(guest_registers->r8, cmd);
			}

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::COPY_GUEST_VIR)
			{
				Command cmd = VUtils::GetCommand(guest_registers->r8);
				CopyGuestVirt(cmd.CopyData.SrcDirbase, cmd.CopyData.SrcGva, cmd.CopyData.DestDirbase, cmd.CopyData.DestGva, cmd.CopyData.Size);
				guest_registers->rax = (UINT64)VMX_ROOT_RESULT::SUCCESS;
				VUtils::SetCommand(guest_registers->r8, cmd);
			}


			size_t rip, exec_len;
			__vmx_vmread(VMCS_GUEST_RIP, &rip);
			__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &exec_len);
			__vmx_vmwrite(VMCS_GUEST_RIP, rip + exec_len);
			return;
		}

		/*if (guest_registers->rcx == VMEXIT_KEY)
		{ 
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
			/*
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
			#1#



			// ÇÐ»»»Øno-root
			size_t rip, exec_len;
			__vmx_vmread(VMCS_GUEST_RIP, &rip);
			__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &exec_len);
			__vmx_vmwrite(VMCS_GUEST_RIP, rip + exec_len);
			return;
		}*/
	}


	if (vmexit_reason == VMX_EXIT_REASON_EPT_VIOLATION)
	{
		
	}

	((vmexit_handler_t)((UINT64)(&vmexit_handler) - g_HvContext.VmExitHandlerRva))(context, unknown);
}