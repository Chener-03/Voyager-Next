#include "Ept.h"

UINT8 UtilInveptGlobal(ept_pointer eptPoint) {
	InvEptDescriptor desc = {};
	desc.ept_pointer = eptPoint.flags;
	desc.reserved1 = 0;
	return AsmInvept(InvEptType::kGlobalInvalidation, &desc);
}

BOOL Is4kPage(ept_pointer eptp, HPA hpa)
{
	DBG::Print("HPA is %llx\n", hpa);
	hpa = (hpa >> 21) << 21;
	Address gpaAddr = { hpa };

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
	pde_2mb_64& pd_large = ((pde_2mb_64*)pd_base_va)[gpaAddr.Type4KB.pd_index];

	if (pd_large.large_page)
	{
		DBG::Print("HPA : %llx  is not 4kb page\n", hpa);
		return false;
	}
	else
	{
		DBG::Print("HPA : %llx  is 4kb page\n", hpa);
		return true;
	} 
}


void Set2mbTo4kb(ept_pointer eptp, HPA hpa)
{
	hpa = (hpa >> 21) << 21;
	Address gpaAddr = { hpa };

	UINT64 epml4_base_pa = eptp.page_frame_number << 12;
	UINT64 epml4_base_va = HpaToHva(epml4_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pml4e eplm4e = ((ept_pml4e*)epml4_base_va)[gpaAddr.Type4KB.pml4_index];

	UINT64 pdpt_base_pa = eplm4e.page_frame_number << 12;
	UINT64 pdpt_base_va = HpaToHva(pdpt_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pdpte pdpt = ((ept_pdpte*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];
	pdpte_1gb_64 pdpt_large = ((pdpte_1gb_64*)pdpt_base_va)[gpaAddr.Type4KB.pdpt_index];

	if (pdpt_large.large_page)
	{
		return;
	}

	UINT64 pd_base_pa = pdpt.page_frame_number << 12;
	UINT64 pd_base_va = HpaToHva(pd_base_pa, MAP_MEMTORY_INDEX::P1);

	ept_pde& pd = ((ept_pde*)pd_base_va)[gpaAddr.Type4KB.pd_index];
	pde_2mb_64& pd_large = ((pde_2mb_64*)pd_base_va)[gpaAddr.Type4KB.pd_index];

	if (pd_large.large_page)
	{
		ShadowPt* shadowPtVa = &g_shadow_pt[0];
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
			pt.page_frame_number = (hpa + i * PAGE_4KB) >> 12;
		}

		pd_large.large_page = 0;
		pd.flags = 0;
		pd.read_access = 1;
		pd.write_access = 1;
		pd.execute_access = 1;
		pd.page_frame_number = ptPa >> 12;
		UtilInveptGlobal(eptp);
		DBG::Print("Success spilt 2m to 4k! \n");
	}

}



