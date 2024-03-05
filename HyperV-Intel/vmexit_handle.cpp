#include <intrin.h>
#include "ShareData.h"
#include "Memory.h"
#include "VUtils.h"
#include "Debug.h"
#include "Ept.h"


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
				DBG::Print("init serial output \n");
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

			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::COVER_PAGE_2M_TO_4K)
			{
				Command cmd = VUtils::GetCommand(guest_registers->r8);

				ept_pointer eptp;
				__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);

				if (!Is4kPage(eptp, cmd.ShadowPage.gpa))
				{
					Set2mbTo4kb(eptp, cmd.ShadowPage.gpa);
				}

				guest_registers->rax = (UINT64)VMX_ROOT_RESULT::SUCCESS;
				VUtils::SetCommand(guest_registers->r8, cmd);
			}


			size_t rip, exec_len;
			__vmx_vmread(VMCS_GUEST_RIP, &rip);
			__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &exec_len);
			__vmx_vmwrite(VMCS_GUEST_RIP, rip + exec_len);
			return;
		}
		 
	}


	if (vmexit_reason == VMX_EXIT_REASON_EPT_VIOLATION)
	{
		vmx_exit_qualification_ept_violation exit_qualification;
		__vmx_vmread(VMCS_EXIT_QUALIFICATION, (size_t*)&exit_qualification);
		GPA fault_pa = 0;
		GVA fault_va = 0;
		__vmx_vmread(VMCS_GUEST_PHYSICAL_ADDRESS, (size_t*)&fault_pa);
		if (exit_qualification.valid_guest_linear_address)
		{
			__vmx_vmread(VMCS_EXIT_GUEST_LINEAR_ADDRESS, (size_t*)&fault_va);
		}

		if(exit_qualification.ept_executable || exit_qualification.ept_writeable || exit_qualification.ept_readable)
		{
			if (exit_qualification.caused_by_translation)
			{
				ept_pointer eptp;
				__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
				// todo: process violation


			}
			
		}

	}

	((vmexit_handler_t)((UINT64)(&vmexit_handler) - g_HvContext.VmExitHandlerRva))(context, unknown);
}