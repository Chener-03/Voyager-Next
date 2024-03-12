#include "Ept.h"

UINT8 UtilInveptGlobal(ept_pointer eptPoint) {
	InvEptDescriptor desc = {};
	desc.ept_pointer = eptPoint.flags;
	desc.reserved1 = 0;
	return AsmInvept(InvEptType::kGlobalInvalidation, &desc);
}

BOOL Is4kPage(ept_pointer eptp, GPA gpa)
{
	DBG::Print("HPA is %llx\n", gpa);

	Address gpaAddr = { gpa };

	UINT64 epml4_base_pa = eptp.page_frame_number << 12;
	UINT64 epml4_base_va = HpaToHva(epml4_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pml4e eplm4e = ((ept_pml4e*)epml4_base_va)[gpaAddr.Type4KB.pml4_index];

	UINT64 pdpt_base_pa = eplm4e.page_frame_number << 12;
	UINT64 pdpt_base_va = HpaToHva(pdpt_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pdpte pdpt = ((ept_pdpte*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];
	pdpte_1gb_64 pdpt_large = ((pdpte_1gb_64*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];

	if (pdpt_large.large_page)
	{
		return false;
	}

	UINT64 pd_base_pa = pdpt.page_frame_number << 12;
	UINT64 pd_base_va = HpaToHva(pd_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pde& pd = ((ept_pde*)pd_base_va)[gpaAddr.Type4KB.pd_index];
	epde_2mb& pd_large = ((epde_2mb*)pd_base_va)[gpaAddr.Type4KB.pd_index];

	if (pd_large.large_page)
	{
		DBG::Print("HPA : %llx  is not 4kb page\n", gpa);
		return false;
	}
	else
	{
		DBG::Print("HPA : %llx  is 4kb page\n", gpa);
		return true;
	} 
}


void Set2mbTo4kb(ept_pointer eptp, GPA gpa,UINT32 saveIndex)
{
	gpa = (gpa >> 21) << 21;
	Address gpaAddr = { gpa };

	UINT64 epml4_base_pa = eptp.page_frame_number << 12;
	UINT64 epml4_base_va = HpaToHva(epml4_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pml4e eplm4e = ((ept_pml4e*)epml4_base_va)[gpaAddr.Type4KB.pml4_index];

	UINT64 pdpt_base_pa = eplm4e.page_frame_number << 12;
	UINT64 pdpt_base_va = HpaToHva(pdpt_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pdpte pdpt = ((ept_pdpte*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];
	ept_pdpte_1gb pdpt_large = ((ept_pdpte_1gb*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];

	if (pdpt_large.large_page)
	{
		return;
	}

	UINT64 pd_base_pa = pdpt.page_frame_number << 12;
	UINT64 pd_base_va = HpaToHva(pd_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pde& pd = ((ept_pde*)pd_base_va)[gpaAddr.Type4KB.pd_index];
	epde_2mb& pd_large = ((epde_2mb*)pd_base_va)[gpaAddr.Type4KB.pd_index];
	
	if (pd_large.large_page)
	{
		ShadowPt* shadowPtVa = &g_shadow_pt[saveIndex];
		if (shadowPtVa->use)
		{
			return;
		}
		shadowPtVa->use = true;
		shadowPtVa->old_pde.flags = pd.flags; // 保存旧的2m页
		UINT64 ptPa = HvaToHpa((HVA)(&shadowPtVa->shadow_pte[0]));

		const auto ept_base = (ept_pte*)(HpaToHva((ptPa >> 12) << 12, MAP_MEMTORY_INDEX::P2));

		// 初始化pt表
		for (UINT32 i = 0; i < 512; i++)
		{
			ept_pte& pt = ept_base[i];
			pt.flags = 0;
			pt.read_access = 1;
			pt.write_access = 1;
			pt.execute_access = 1;

			pt.user_mode_execute = pd_large.user_mode_execute;
			pt.ignore_pat = pd_large.ignore_pat;
			pt.memory_type = pd_large.memory_type;
			pt.suppress_ve = pd_large.suppress_ve;

			pt.page_frame_number = (gpa + i * PAGE_4KB) >> 12;
		}

		pd_large.large_page = 0;
		pd.flags = 0;
		pd.read_access = 1;
		pd.write_access = 1;
		pd.execute_access = 1;
		pd.user_mode_execute = 1;
		pd.page_frame_number = ptPa >> 12;
		UtilInveptGlobal(eptp);
		DBG::Print("Success spilt 2m to 4k! \n");
	}

}




BOOL SetEptPtAttr(ept_pointer eptp, GPA gpa, UINT64 pfn, bool canExec)
{
	Address gpaAddr = { gpa };

	UINT64 epml4_base_pa = eptp.page_frame_number << 12;
	UINT64 epml4_base_va = HpaToHva(epml4_base_pa, MAP_MEMTORY_INDEX::P2);

	ept_pml4e eplm4e = ((ept_pml4e*)epml4_base_va)[gpaAddr.Type4KB.pml4_index];

	UINT64 pdpt_base_pa = eplm4e.page_frame_number << 12;

	UINT64 pdpt_base_va = HpaToHva(pdpt_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pdpte pdpt = ((ept_pdpte*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];
	pdpte_1gb_64 pdpt_large = ((pdpte_1gb_64*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];

	if (pdpt_large.large_page)
	{
		return false;
	}

	UINT64 pd_base_pa = pdpt.page_frame_number << 12;
	UINT64 pd_base_va = HpaToHva(pd_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pde& pd = ((ept_pde*)pd_base_va)[gpaAddr.Type4KB.pd_index];
	epde_2mb& pd_large = ((epde_2mb*)pd_base_va)[gpaAddr.Type4KB.pd_index];

	if (pd_large.large_page)
	{
		return false;
	}

	UINT64 pt_base_pa = pd.page_frame_number << 12;
	UINT64 pt_base_va = HpaToHva(pt_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pte& pt = ((ept_pte*)pt_base_va)[gpaAddr.Type4KB.pt_index];
	pt.page_frame_number = pfn;
	if (canExec)
	{
		pt.execute_access = 1;
		pt.read_access = 0;
		pt.write_access = 0;
	}
	else
	{
		pt.execute_access = 0;
		pt.read_access = 1;
		pt.write_access = 1;
	}
	UtilInveptGlobal(eptp);
	return true;
}

ept_pte GetEptPt(ept_pointer eptp, GPA gpa)
{
	Address gpaAddr = { gpa };

	UINT64 epml4_base_pa = eptp.page_frame_number << 12;
	UINT64 epml4_base_va = HpaToHva(epml4_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pml4e eplm4e = ((ept_pml4e*)epml4_base_va)[gpaAddr.Type4KB.pml4_index];

	UINT64 pdpt_base_pa = eplm4e.page_frame_number << 12;
	UINT64 pdpt_base_va = HpaToHva(pdpt_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pdpte pdpt = ((ept_pdpte*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];
	pdpte_1gb_64 pdpt_large = ((pdpte_1gb_64*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];

	if (pdpt_large.large_page)
	{
		return {};
	}

	UINT64 pd_base_pa = pdpt.page_frame_number << 12;
	UINT64 pd_base_va = HpaToHva(pd_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pde& pd = ((ept_pde*)pd_base_va)[gpaAddr.Type4KB.pd_index];
	epde_2mb& pd_large = ((epde_2mb*)pd_base_va)[gpaAddr.Type4KB.pd_index];

	if (pd_large.large_page)
	{
		return {};
	}

	UINT64 pt_base_pa = pd.page_frame_number << 12;
	UINT64 pt_base_va = HpaToHva(pt_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pte pt = ((ept_pte*)pt_base_va)[gpaAddr.Type4KB.pt_index];
	return pt;
}



