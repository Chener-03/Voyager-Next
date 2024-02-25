#include "asm.h"
#include "ept.h"
#include "hv.h"




SharedShadowHookData ShadowHookData;
ShadowPte shadow[MAX_HOOKS];



VmxStatus UtilInveptGlobal(ept_pointer eptPoint) {
	InvEptDescriptor desc = {};
	desc.ept_pointer = eptPoint.flags;
	desc.reserved1 = 0;

	return static_cast<VmxStatus>(AsmInvept(InvEptType::kGlobalInvalidation, &desc));
}

HookInformation* ShpFindPatchInfoByPage(SharedShadowHookData* shared_sh_data, void* address) 
{
	BOOL haveFound = FALSE;
	int i;
	for (i = 0; i < MAX_HOOKS; i++)
	{
		if (PAGE_ALIGN(shared_sh_data->hooks[i].patch_address) == PAGE_ALIGN(address))
		{
			haveFound = TRUE;
			break;
		}
	}
	if (haveFound)
	{
		return &shared_sh_data->hooks[i];
	}
	else
	{
		return nullptr;
	}
}

HookInformation* ShpFindPatchInfoBy2MPage(SharedShadowHookData* shared_sh_data, void* address)
{
	BOOL haveFound = FALSE;
	int i;
	for (i = 0; i < MAX_HOOKS; i++)
	{
		if (PAGE_ALIGN2M(shared_sh_data->hooks[i].patch_address) == PAGE_ALIGN2M(address))
		{
			haveFound = TRUE;
			break;
		}
	}
	if (haveFound)
	{
		return &shared_sh_data->hooks[i];
	}
	else
	{
		return nullptr;
	}
}

HookInformation* ShpFindPatchInfoByAddress(SharedShadowHookData* shared_sh_data, void* address) 
{
	BOOL haveFound = FALSE;
	int i;
	for (i = 0; i < MAX_HOOKS; i++)
	{
		if (shared_sh_data->hooks[i].patch_address == address)
		{
			haveFound = TRUE;
			break;
		}
	}
	if (haveFound)
	{
		return &shared_sh_data->hooks[i];
	}
	else
	{
		return nullptr;
	}
}
#define EPT_PD_MASK                         (~((ULONG64)(0x200000 - 1)))

void split_2mb_to_4kb(ept_pointer eptp,GuestPhysicalAddress guest_pa,HostPhysicalAddress host_pa)
{
	int hookNum = 0;
	u64 ptPhyAddr = 0;
	
	HV::PhysicalAddress guest_phys{ guest_pa }; 
	const auto epml4 = reinterpret_cast<ept_pml4e*>(
		HV::MapPage(eptp.page_frame_number << 12, MapType::map_src));

	const auto epdpt_large =
		reinterpret_cast<ept_pdpte_1gb*>(HV::MapPage(
			epml4[guest_phys.pml4_index].page_frame_number << 12, MapType::map_src));

	// handle 1g page...
	if (epdpt_large[guest_phys.pdpt_index].large_page)
	{
		return;
	}
	const auto epdpt =
		reinterpret_cast<ept_pdpte*>(epdpt_large);

	const auto epd_large =
		reinterpret_cast<epde_2mb*>(HV::MapPage(
			epdpt[guest_phys.pdpt_index].page_frame_number << 12, MapType::map_src));

	// handle 2mb page...
	if (epd_large[guest_phys.pd_index].large_page)
	{
		for (hookNum = 0; hookNum < MAX_HOOKS; hookNum++)
		{
			PShadowPte pShadowPte = (PShadowPte)&shadow[hookNum];
			if ((pShadowPte->pa_base_for_2m & EPT_PD_MASK) == guest_pa) //it already recorded
			{
				ptPhyAddr = HV::Translate((HostVirtureAddress)pShadowPte->shadowPte);
				epd_large[guest_phys.pd_index].large_page = FALSE;
				const auto epd =
					reinterpret_cast<ept_pde*>(epd_large);
				epd[guest_phys.pd_index].execute_access = 1;
				epd[guest_phys.pd_index].read_access = 1;
				epd[guest_phys.pd_index].write_access = 1;
				epd[guest_phys.pd_index].reserved1 = 0;
				epd[guest_phys.pd_index].reserved2 = 0;
				epd[guest_phys.pd_index].reserved3 = 0;
				epd[guest_phys.pd_index].reserved4 = 0;
				epd[guest_phys.pd_index].page_frame_number = ptPhyAddr >> 12; //store
				UtilInveptGlobal(eptp);
				return;
			}
		}

		for (hookNum = 0; hookNum < MAX_HOOKS; hookNum++)
		{
			PShadowPte pShadowPte = (PShadowPte)&shadow[hookNum];
			if (pShadowPte->pa_base_for_2m == 0) //search for an empty place
			{
				pShadowPte->pa_base_for_2m = guest_pa;
				ptPhyAddr = HV::Translate((HostVirtureAddress)pShadowPte->shadowPte);
				break;
			}
		}
		
	
		//init
		ept_pte epte = { 0 };
		epte.flags = 0;
		epte.read_access = 1;
		epte.write_access = 1;
		epte.execute_access = 1;

		const auto epd =
			reinterpret_cast<ept_pde*>(epd_large);
		ept_pde newEpde = { 0 };
		newEpde.flags = 0;
		newEpde.execute_access = 1;
		newEpde.read_access = 1;
		newEpde.write_access = 1;
		newEpde.page_frame_number = ptPhyAddr >> 12; //store the phy addr

		const auto ept =
			reinterpret_cast<ept_pte*>(HV::MapPage(
				newEpde.page_frame_number << 12, MapType::map_src));
		for (u64 i = 0; i < 512; i++)
		{
			epte.page_frame_number = (host_pa + i * PAGE_4KB) >> 12;
			ept[i] = epte;
			//dbg::print("index:%d point to:%llx\n",i,epte.page_frame_number << 12);
		}
		epd[guest_phys.pd_index].flags = newEpde.flags;
		UtilInveptGlobal(eptp);
	}
}





void merge_4kb_to_2mb(ept_pointer eptp, GuestPhysicalAddress guest_pa, HostPhysicalAddress host_pa)
{
	int hookNum = 0;
	u64 ptPhyAddr = 0;

	HV::PhysicalAddress guest_phys{ guest_pa };
	const auto epml4 = reinterpret_cast<ept_pml4e*>(
		HV::MapPage(eptp.page_frame_number << 12, MapType::map_src));

	const auto epdpt_large =
		reinterpret_cast<ept_pdpte_1gb*>(HV::MapPage(
			epml4[guest_phys.pml4_index].page_frame_number << 12, MapType::map_src));

	// handle 1g page...
	if (epdpt_large[guest_phys.pdpt_index].large_page)
		return;

	const auto epdpt =
		reinterpret_cast<ept_pdpte*>(epdpt_large);

	const auto epd_large =
		reinterpret_cast<epde_2mb*>(HV::MapPage(
			epdpt[guest_phys.pdpt_index].page_frame_number << 12, MapType::map_src));

	// handle 2mb page...
	if (epd_large[guest_phys.pd_index].large_page)
		return;

	const auto epd =
		reinterpret_cast<ept_pde*>(epd_large);

	const auto ept =
		reinterpret_cast<ept_pte*>(HV::MapPage(
			epd[guest_phys.pd_index].page_frame_number << 12, MapType::map_src));

	epde_2mb epde = { 0 };
	epde.flags = 0;
	epde.execute_access = 1;
	epde.large_page = 1;
	epde.read_access = 1;
	epde.write_access = 1;

	epde.page_frame_number = host_pa >> 21;
	epd_large[guest_phys.pd_index].flags = epde.flags;
	//begin to search
	UtilInveptGlobal(eptp);
	for (hookNum = 0; hookNum < MAX_HOOKS; hookNum++)
	{
		PShadowPte pShadowPte = (PShadowPte)&shadow[hookNum];
		if ((pShadowPte->pa_base_for_2m & EPT_PD_MASK) == (guest_pa & EPT_PD_MASK))
		{
			pShadowPte->pa_base_for_2m = 0; //marked as unused
			break;
		}
	}

}

void changeEPTAttribute(ept_pointer eptp, GuestPhysicalAddress guest_pa, bool bCanExecute)
{
	HV::PhysicalAddress guest_phys{ guest_pa };
	const auto epml4 = reinterpret_cast<ept_pml4e*>(
		HV::MapPage(eptp.page_frame_number << 12, MapType::map_src));

	const auto epdpt_large =
		reinterpret_cast<ept_pdpte_1gb*>(HV::MapPage(
			epml4[guest_phys.pml4_index].page_frame_number << 12, MapType::map_src));

	// handle 1gb page...
	if (epdpt_large[guest_phys.pdpt_index].large_page)
	{
		return;
	}

	const auto epdpt =
		reinterpret_cast<ept_pdpte*>(epdpt_large);

	const auto epd_large =
		reinterpret_cast<epde_2mb*>(HV::MapPage(
			epdpt[guest_phys.pdpt_index].page_frame_number << 12, MapType::map_src));

	// handle 2mb page...
	if (epd_large[guest_phys.pd_index].large_page)
	{
		return;
	}

	const auto epd =
		reinterpret_cast<ept_pde*>(epd_large);

	const auto ept =
		reinterpret_cast<ept_pte*>(HV::MapPage(
			epd[guest_phys.pd_index].page_frame_number << 12, MapType::map_src));

	if (bCanExecute)
	{
		ept_pte newEpte = ept[guest_phys.pt_index];
		newEpte.execute_access = 1;
		newEpte.read_access = 0;
		newEpte.write_access = 0;
		ept[guest_phys.pt_index].flags = newEpte.flags;
	}
	else
	{
		ept_pte newEpte = ept[guest_phys.pt_index];
		newEpte.execute_access = 0;
		newEpte.read_access = 1;
		newEpte.write_access = 1;
		ept[guest_phys.pt_index].flags = newEpte.flags;
	}
	UtilInveptGlobal(eptp);
}

void disablePageProtection(ept_pointer eptp, GuestPhysicalAddress guest_pa)
{
	HV::PhysicalAddress guest_phys{ guest_pa };
	const auto epml4 = reinterpret_cast<ept_pml4e*>(
		HV::MapPage(eptp.page_frame_number << 12, MapType::map_src));

	const auto epdpt_large =
		reinterpret_cast<ept_pdpte_1gb*>(HV::MapPage(
			epml4[guest_phys.pml4_index].page_frame_number << 12, MapType::map_src));

	// handle 1gb page...
	if (epdpt_large[guest_phys.pdpt_index].large_page)
	{
		epdpt_large[guest_phys.pdpt_index].execute_access = 1;
		epdpt_large[guest_phys.pdpt_index].read_access = 1;
		epdpt_large[guest_phys.pdpt_index].write_access = 1;
		UtilInveptGlobal(eptp);
		//dbg::print("[changeEPTAttribute]:1GB page\n");
		return;
	}

	const auto epdpt =
		reinterpret_cast<ept_pdpte*>(epdpt_large);

	const auto epd_large =
		reinterpret_cast<epde_2mb*>(HV::MapPage(
			epdpt[guest_phys.pdpt_index].page_frame_number << 12, MapType::map_src));

	// handle 2mb page...
	if (epd_large[guest_phys.pd_index].large_page)
	{
		epd_large[guest_phys.pd_index].execute_access = 1;
		epd_large[guest_phys.pd_index].write_access = 1;
		epd_large[guest_phys.pd_index].read_access = 1;
		UtilInveptGlobal(eptp);
		//dbg::print("[changeEPTAttribute]:2MB page\n");
		return;
	}

	const auto epd =
		reinterpret_cast<ept_pde*>(epd_large);

	const auto ept =
		reinterpret_cast<ept_pte*>(HV::MapPage(
			epd[guest_phys.pd_index].page_frame_number << 12, MapType::map_src));

	ept[guest_phys.pt_index].execute_access = 1;
	ept[guest_phys.pt_index].read_access = 1;
	ept[guest_phys.pt_index].write_access = 1;


	UtilInveptGlobal(eptp);
}

NTSTATUS VoyagerEptAddFakePage(u64 uHookAddr, u64 uPageRead, u64 uPageExec)
{
	PSharedShadowHookData pShadowHook = (PSharedShadowHookData)&ShadowHookData;
	PHookInformation pInfo = NULL;
	for (int i = 0; i < MAX_HOOKS; i++)
	{
		if (pShadowHook->hooks[i].isEnable == TRUE)
		{
			if (pShadowHook->hooks[i].patch_address == (void*)uHookAddr)
			{
				return STATUS_SUCCESS;
			}
		}
	}
	for (int i = 0; i < MAX_HOOKS; i++)
	{
		if (pShadowHook->hooks[i].isEnable == FALSE)
		{
			pInfo = &pShadowHook->hooks[i];
			break;
		}
	}
	if (!pInfo)
	{
		return STATUS_UNSUCCESSFUL;
	}

	pInfo->isEnable = TRUE;
	pInfo->patch_address = (void*)uHookAddr;
	pInfo->pa_base_for_rw = uPageRead;
	pInfo->pa_base_for_exec = uPageExec;

	return STATUS_SUCCESS;
}

NTSTATUS VoyagerEptDelteFakePage(u64 uHookAddr)
{
	PSharedShadowHookData pShadowHook = (PSharedShadowHookData)&ShadowHookData;
	PHookInformation pInfo = NULL;
	pInfo = ShpFindPatchInfoByAddress(pShadowHook, (void*)uHookAddr);
	if (!pInfo)
	{
		return STATUS_UNSUCCESSFUL;
	}

	pInfo->isEnable = FALSE;
	pInfo->patch_address = 0;
	pInfo->pa_base_for_rw = 0;
	pInfo->pa_base_for_exec = 0;

	return STATUS_SUCCESS;
}

bool map_4k(ept_pointer eptp, GuestPhysicalAddress guest_pa, GuestPhysicalAddress host_pa)
{
	HV::PhysicalAddress guest_phys{ guest_pa };
	const auto epml4 = reinterpret_cast<ept_pml4e*>(
		HV::MapPage(eptp.page_frame_number << 12, MapType::map_src));
	const auto epdpt_large =
		reinterpret_cast<ept_pdpte_1gb*>(HV::MapPage(
			epml4[guest_phys.pml4_index].page_frame_number << 12, MapType::map_src));

	// handle 1gb page...
	if (epdpt_large[guest_phys.pdpt_index].large_page)
	{
		return false;
	}

	const auto epdpt =
		reinterpret_cast<ept_pdpte*>(epdpt_large);

	const auto epd_large =
		reinterpret_cast<epde_2mb*>(HV::MapPage(
			epdpt[guest_phys.pdpt_index].page_frame_number << 12, MapType::map_src));

	// handle 2mb page...
	if (epd_large[guest_phys.pd_index].large_page)
	{
		return false;
	}

	const auto epd =
		reinterpret_cast<ept_pde*>(epd_large);

	const auto ept =
		reinterpret_cast<ept_pte*>(HV::MapPage(
			epd[guest_phys.pd_index].page_frame_number << 12, MapType::map_src));

	ept_pte newEpte = { 0 };
	newEpte.flags = 0;
	newEpte.read_access = 1;
	newEpte.write_access = 1;
	newEpte.execute_access = 1;
	newEpte.page_frame_number = host_pa >> 12;

	ept[guest_phys.pt_index].flags = newEpte.flags;
	UtilInveptGlobal(eptp);
	return true;
}

bool VoyagerHandleEptViolation(EptViolationQualification* eptQualification, void* fault_va)
{
	const auto read_failure = (eptQualification->fields.read_access) && (!eptQualification->fields.ept_readable);
	const auto write_failure = (eptQualification->fields.write_access) && (!eptQualification->fields.ept_writeable);
	const auto exec_failure = (eptQualification->fields.execute_access) && (!eptQualification->fields.ept_executable);
	const auto info = ShpFindPatchInfoByPage((PSharedShadowHookData)&ShadowHookData, fault_va);
	if (!info) {
		return false;
	}

	GuestPhysicalAddress uPageExec = info->pa_base_for_exec; //for exec
	GuestPhysicalAddress uPageRead = info->pa_base_for_rw; //for rw

	ept_pointer eptp;
	HV::PhysicalAddress guest_phys{ uPageExec };

	__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);
	const auto epml4 = reinterpret_cast<ept_pml4e*>(
		HV::MapPage(eptp.page_frame_number << 12, MapType::map_src));
	const auto epdpt_large =
		reinterpret_cast<ept_pdpte_1gb*>(HV::MapPage(
			epml4[guest_phys.pml4_index].page_frame_number << 12, MapType::map_src));

	// handle 1gb page...
	if (epdpt_large[guest_phys.pdpt_index].large_page)
	{
		return false;
	}

	const auto epdpt =
		reinterpret_cast<ept_pdpte*>(epdpt_large);

	const auto epd_large =
		reinterpret_cast<epde_2mb*>(HV::MapPage(
			epdpt[guest_phys.pdpt_index].page_frame_number << 12, MapType::map_src));

	// handle 2mb page...
	if (epd_large[guest_phys.pd_index].large_page)
	{
		return false;
	}

	const auto epd =
		reinterpret_cast<ept_pde*>(epd_large);

	const auto ept =
		reinterpret_cast<ept_pte*>(HV::MapPage(
			epd[guest_phys.pd_index].page_frame_number << 12, MapType::map_src));

	if (read_failure || write_failure)
	{
		ept[guest_phys.pt_index].read_access = 1;
		ept[guest_phys.pt_index].write_access = 1;
		ept[guest_phys.pt_index].execute_access = 0;
		ept[guest_phys.pt_index].page_frame_number = uPageRead >> 12;  //load page for rw
		return true;
	}
	else if (exec_failure)
	{
		ept[guest_phys.pt_index].read_access = 0;
		ept[guest_phys.pt_index].write_access = 0;
		ept[guest_phys.pt_index].execute_access = 1;
		ept[guest_phys.pt_index].page_frame_number = uPageExec >> 12;  //load page for exec
		return true;
	}
	return false;
}

