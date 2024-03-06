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
					Set2mbTo4kb(eptp, cmd.ShadowPage.gpa,cmd.ShadowPage.index);
				}

				guest_registers->rax = (UINT64)VMX_ROOT_RESULT::SUCCESS;
				VUtils::SetCommand(guest_registers->r8, cmd);
			}


			if ((VMX_COMMAND)guest_registers->rdx == VMX_COMMAND::REPLACE_4K_PAGE)
			{
				Command cmd = VUtils::GetCommand(guest_registers->r8);

				ept_pointer eptp;
				__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
				/**
				 *	获取pt 保存原来的pt
				 *	pt pfn改为传入虚拟地址的pa
				 *	修改属性为只执行
				 */
				if (Is4kPage(eptp, cmd.ShadowPage.gpa))
				{
					HPA oldhpa = GpaToHpa(cmd.ShadowPage.gpa, MAP_MEMTORY_INDEX::P2);

					GPA gpa = GvaToGpa(cmd.ShadowPage.dirbase, cmd.ShadowPage.gva, MAP_MEMTORY_INDEX::P3);
					HPA hpa = GpaToHpa(gpa, MAP_MEMTORY_INDEX::P2);   // 一般情况下 hpa和gpa都一样

					auto oldPt = GetEptPt(eptp, cmd.ShadowPage.gpa);
		
					auto info = &g_hook_info[cmd.ShadowPage.index];

					if (info->use == false)
					{
						info->use = true;
						info->hook_pt_phy_address = oldhpa;
						info->old_pfn = oldPt.page_frame_number;
						info->new_pfn = hpa >> 12;
						SetEptPtAttr(eptp, cmd.ShadowPage.gpa, hpa >> 12, true);
					}

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
				DBG::Print("ept_violation: pa is %llx\n", fault_pa);
				DBG::Print("ept_violation: va is %llx\n", fault_va);

				for (int i = 0; i < MAX_SHADOW_PT_SIZE; ++i)
				{
					auto info = g_hook_info[i];
					if (info.use && ((info.hook_pt_phy_address>>12) == (fault_pa>>12)))
					{
						const auto read_failure = (exit_qualification.read_access) && (!exit_qualification.ept_readable);
						const auto write_failure = (exit_qualification.write_access) && (!exit_qualification.ept_writeable);
						const auto exec_failure = (exit_qualification.execute_access) && (!exit_qualification.ept_executable);
						if (read_failure)
						{
							DBG::Print("error by read_failure\n");
						}
						if (write_failure)
						{
							DBG::Print("error by write_failure\n");
						}
						if (exec_failure)
						{
							DBG::Print("error by exec_failure\n");
						}


						if (read_failure || write_failure)
						{
							SetEptPtAttr(eptp, fault_pa, info.old_pfn, false);
							DBG::Print("chang to old \n");
						}
						else if(exec_failure)
						{
							SetEptPtAttr(eptp, fault_pa, info.new_pfn, true);
							DBG::Print("chang to new \n");
						}
						break;
					}
				}
				
			}
			
		}

	}

	((vmexit_handler_t)((UINT64)(&vmexit_handler) - g_HvContext.VmExitHandlerRva))(context, unknown);
}