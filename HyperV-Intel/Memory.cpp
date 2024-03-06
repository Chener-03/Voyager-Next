#include "Memory.h"
#include <intrin.h>
#include "VUtils.h"
#include "Debug.h"

pml4e* g_HvSelfRefPml4 = (pml4e*)SELF_REF_PML4;

__declspec(allocate(".pdpt")) pdpte g_pdpt[512];
__declspec(allocate(".pd")) pde g_pd[512];
__declspec(allocate(".pt")) pte g_pt[512];

__declspec(allocate(".sdpt")) ShadowPt g_shadow_pt[MAX_SHADOW_PT_SIZE];
__declspec(allocate(".sdhk")) HookInfo g_hook_info[MAX_SHADOW_PT_SIZE];
bool g_Init_Shadow = false;

void InitShadowData()
{
	if (!g_Init_Shadow)
	{
		DBG::Print("Init ShadowData\n");
		g_Init_Shadow = true;
		for (int i = 0; i < MAX_SHADOW_PT_SIZE; ++i)
		{
			g_shadow_pt[i].use = false;
			g_hook_info[i].use = false;
		}
	}
}


UINT64 GetMapVirtual(UINT16 offset, MAP_MEMTORY_INDEX index)
{
	auto cpuIndex = VUtils::GetCurrentCpuIndex();

	Address virt_addr{ MAPPING_ADDRESS_BASE };

	virt_addr.Type4KB.pt_index = (cpuIndex * MAP_MEMTORY_INDEX_SIZE) + (unsigned)index;

	return virt_addr.value + offset;
}

HVA HpaToHva(HPA HostPhyAddr, MAP_MEMTORY_INDEX index)
{
	auto cpuIndex = VUtils::GetCurrentCpuIndex();

	g_pt[(cpuIndex * MAP_MEMTORY_INDEX_SIZE) + (unsigned)index].page_frame_number = HostPhyAddr >> 12;

	auto address = Address{ HostPhyAddr };

	__invlpg((PVOID)(GetMapVirtual(address.Type4KB.offset_4kb, index)));

	return GetMapVirtual(address.Type4KB.offset_4kb, index);
}

HPA HvaToHpa(HVA HostVirtureAddress)
{
	Address virtAddr = { HostVirtureAddress };
	Address cursor = { (UINT64)g_HvSelfRefPml4 }; 

	pml4e pml4eEntry = ((pml4e*)cursor.value)[virtAddr.Type4KB.pml4_index];

	if (!pml4eEntry.present) return 0;

	// pml4eEntry.pfn << 12 是物理地址
	// 这样可以让 cursor 取到的位虚拟地址 而不需要转换
	cursor.Type4KB.pt_index = virtAddr.Type4KB.pml4_index;
	pdpte pdpteEntry = ((pdpte*)cursor.value)[virtAddr.Type4KB.pdpt_index];
	pdpte_1gb_64 pdpteLargeEntry = ((pdpte_1gb_64*)cursor.value)[virtAddr.Type4KB.pdpt_index];

	if (!pdpteEntry.present) return 0;

	// 1GB
	if (pdpteLargeEntry.large_page) return (pdpteLargeEntry.page_frame_number << 30) + virtAddr.Type1GB.offset_1gb;

	// 转换为虚拟地址..
	cursor.Type2MB.pd_index = virtAddr.Type2MB.pml4_index;
	cursor.Type4KB.pt_index = virtAddr.Type4KB.pdpt_index;
	pde pdeEntry = ((pde*)cursor.value)[virtAddr.Type4KB.pd_index];
	pde_2mb_64 pdeLargeEntry = ((pde_2mb_64*)cursor.value)[virtAddr.Type4KB.pd_index];
	if (!pdeEntry.present) return 0;

	// 2MB
	if (pdeLargeEntry.large_page) return (pdeLargeEntry.page_frame_number << 21) + virtAddr.Type2MB.offset_2mb;


	cursor.Type4KB.pdpt_index = virtAddr.Type4KB.pml4_index;
	cursor.Type4KB.pd_index = virtAddr.Type4KB.pdpt_index;
	cursor.Type4KB.pt_index = virtAddr.Type4KB.pd_index;
	pte pteEntry = ((pte*)cursor.value)[virtAddr.Type4KB.pt_index];

	if (!pteEntry.present) return 0;

	return (pteEntry.page_frame_number << 12) + virtAddr.Type4KB.offset_4kb;
}

HPA GpaToHpa(GPA GuestPhyAddr, MAP_MEMTORY_INDEX index)
{
	ept_pointer eptp;
	__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);

	Address gpa{ GuestPhyAddr };

	UINT64 epml4_base_pa = eptp.page_frame_number << 12;
	UINT64 epml4_base_va = HpaToHva(epml4_base_pa, index);

	ept_pml4e eplm4e = ((ept_pml4e*)epml4_base_va)[gpa.Type4KB.pml4_index];

	UINT64 pdpt_base_pa = eplm4e.page_frame_number << 12;
	UINT64 pdpt_base_va = HpaToHva(pdpt_base_pa, index);


	ept_pdpte pdpt = ((ept_pdpte*)pdpt_base_va)[gpa.Type4KB.pdpt_index];
	pdpte_1gb_64 pdpt_large = ((pdpte_1gb_64*)pdpt_base_va)[gpa.Type1GB.pdpt_index];

	// 1GB
	if (pdpt_large.large_page)
	{
		return (pdpt_large.page_frame_number << 30) + gpa.Type1GB.offset_1gb;
	}

	UINT64 pd_base_pa = pdpt.page_frame_number << 12;
	UINT64 pd_base_va = HpaToHva(pd_base_pa, index);


	ept_pde pd = ((ept_pde*)pd_base_va)[gpa.Type4KB.pd_index];
	pde_2mb_64 pd_large = ((pde_2mb_64*)pd_base_va)[gpa.Type2MB.pd_index];

	// 2MB
	if (pd_large.large_page)
	{
		return (pd_large.page_frame_number << 21) + gpa.Type2MB.offset_2mb;
	}

	UINT64 pt_base_pa = pd.page_frame_number << 12;
	UINT64 pt_base_va = HpaToHva(pt_base_pa, index);

	ept_pte pt = ((ept_pte*)pt_base_va)[gpa.Type4KB.pt_index];
	return (pt.page_frame_number << 12) + gpa.Type4KB.offset_4kb;
}

HVA GpaToHva(GPA GuestPhyAddr, MAP_MEMTORY_INDEX index)
{
	const auto host_phys = GpaToHpa(GuestPhyAddr, index);

	if (!host_phys)
		return 0;

	return HpaToHva(host_phys, index);
}

GPA GvaToGpa(UINT64 Dirbase, GVA GuestVirAddr, MAP_MEMTORY_INDEX index)
{
	Address va{ GuestVirAddr };
	pml4e* pml4_base = (pml4e*)GpaToHva(Dirbase, index);

	auto pml4 = pml4_base[va.Type4KB.pml4_index];

	if (!pml4.present) return 0;

	pdpte* pdpt_base = (pdpte*)GpaToHva(pml4.page_frame_number << 12, index);

	pdpte pdpt = pdpt_base[va.Type4KB.pdpt_index];
	pdpte_1gb_64 pdpt_large = ((pdpte_1gb_64*)pdpt_base)[va.Type4KB.pdpt_index];
	if (!pdpt_base[va.Type4KB.pdpt_index].present) return 0;

	// 1GB
	if (pdpt_large.large_page)
		return (pdpt_large.page_frame_number << 30) + va.Type1GB.offset_1gb;

	pde* pde_base = (pde*)GpaToHva(pdpt.page_frame_number << 12, index);

	pde pd = pde_base[va.Type4KB.pd_index];
	pde_2mb_64 pd_large = ((pde_2mb_64*)pde_base)[va.Type2MB.pd_index];

	if (!pd.present) return 0;

	// 2MB
	if (pd_large.large_page)
		return (pd_large.page_frame_number << 21) + va.Type2MB.offset_2mb;

	pte* pt_base = (pte*)GpaToHva(pd.page_frame_number << 12, index);

	pte pt = pt_base[va.Type4KB.pt_index];

	if (!pt.present) return 0;

	return (pt.page_frame_number << 12) + va.Type4KB.offset_4kb;
}



HVA GvaToHva(UINT64 Dirbase, GVA GuestVirAddr, MAP_MEMTORY_INDEX index)
{
	const auto guest_phys = GvaToGpa(Dirbase, GuestVirAddr, index);

	if (!guest_phys)
		return 0;

	return GpaToHva(guest_phys, index);
}


BOOL InitPageTable()
{
	InitShadowData();

	HPA pdpt_phys = HvaToHpa((HVA)g_pdpt);
	DBG::Print("InitPageTable: pdpt_phys is %llx \n", pdpt_phys);

	HPA pd_phys = HvaToHpa((HVA)g_pd);
	DBG::Print("InitPageTable: pd_phys is %llx \n", pd_phys);

	HPA pt_phys = HvaToHpa((HVA)g_pt);
	DBG::Print("InitPageTable: pt_phys is %llx \n", pt_phys);

	HPA shadow_pt = HvaToHpa((HVA)&g_shadow_pt[0]);
	DBG::Print("InitPageTable: shadow_pt is %llx \n", shadow_pt);

	HPA hookinfo = HvaToHpa((HVA)&g_hook_info[0]);
	DBG::Print("InitPageTable: hookinfo is %llx \n", hookinfo);

	if (!pdpt_phys || !pd_phys || !pt_phys || !shadow_pt || !hookinfo)
		return FALSE;

	// 设置映射页表项。。。
	{
		g_HvSelfRefPml4[MAPPING_PML4_IDX].present = true;
		g_HvSelfRefPml4[MAPPING_PML4_IDX].page_frame_number = pdpt_phys >> 12;
		g_HvSelfRefPml4[MAPPING_PML4_IDX].supervisor = false;
		g_HvSelfRefPml4[MAPPING_PML4_IDX].write = true;

		g_pdpt[511].present = true;
		g_pdpt[511].page_frame_number = pd_phys >> 12;
		g_pdpt[511].supervisor = false;
		g_pdpt[511].write = true;

		g_pd[511].present = true;
		g_pd[511].page_frame_number = pt_phys >> 12;
		g_pd[511].supervisor = false;
		g_pd[511].write = true;
	}


	//每个核心都有自己的页面，可以用来映射
	//物理内存到虚拟内存：^）
	{
		for (auto idx = 0; idx < 512; ++idx)
		{
			g_pt[idx].present = true;
			g_pt[idx].supervisor = false;
			g_pt[idx].write = true;
		}
	}


	pml4e* mapped_pml4 = reinterpret_cast<pml4e*>(HpaToHva(__readcr3(), MAP_MEMTORY_INDEX::P1));


	// 检查以确保翻译有效。。。
	if (HvaToHpa((HVA)mapped_pml4) != __readcr3())
		return FALSE;

	// 检查以确保self-ref pml4e有效。。。
	if (mapped_pml4[SELF_REF_PML4_IDX].page_frame_number != __readcr3() >> 12)
		return FALSE;

	// 检查以确保映射pml4e有效。。。
	if (mapped_pml4[MAPPING_PML4_IDX].page_frame_number != pdpt_phys >> 12)
		return FALSE;


	return TRUE;


}



BOOL ReadGuestPhy(__IN__ UINT64 Dirbase, __IN__ GPA ReadPhyAddr, __OUT__ GVA WriteVirAddr, __IN__ UINT32 Size)
{
	while (Size)
	{
		auto dest_current_size = PAGE_4KB - Address{ WriteVirAddr }.Type4KB.offset_4kb;

		if (Size < dest_current_size)
			dest_current_size = Size;

		auto src_current_size = PAGE_4KB - Address{ ReadPhyAddr }.Type4KB.offset_4kb;

		if (Size < src_current_size)
			src_current_size = Size;

		auto current_size = min(dest_current_size, src_current_size);

		HVA mapped_dest = GvaToHva(Dirbase, WriteVirAddr, MAP_MEMTORY_INDEX::P2);

		if (!mapped_dest)
			return FALSE;

		HVA mapped_src = GpaToHva(ReadPhyAddr, MAP_MEMTORY_INDEX::P1);

		if (!mapped_src)
			return FALSE;

		memcpy((PVOID)mapped_dest, (PVOID)mapped_src, current_size);
		ReadPhyAddr += current_size;
		WriteVirAddr += current_size;
		Size -= current_size;
	}
	return TRUE;
}

BOOL WriteGuestPhy(UINT64 Dirbase, GPA WritePhyAddr, GVA DataVirAddr, UINT32 Size)
{
	while (Size)
	{
		auto dest_current_size = PAGE_4KB - Address{ DataVirAddr }.Type4KB.offset_4kb;

		if (Size < dest_current_size)
			dest_current_size = Size;

		auto src_current_size = PAGE_4KB - Address{ WritePhyAddr }.Type4KB.offset_4kb;

		if (Size < src_current_size)
			src_current_size = Size;

		auto current_size = min(dest_current_size, src_current_size);

		HVA mapped_src = GvaToHva(Dirbase, DataVirAddr, MAP_MEMTORY_INDEX::P1);

		if (!mapped_src)
			return FALSE;

		HVA mapped_dest = GpaToHva(WritePhyAddr, MAP_MEMTORY_INDEX::P2);

		if (!mapped_src)
			return FALSE;

		memcpy((PVOID)mapped_dest, (PVOID)mapped_src, current_size);
		WritePhyAddr += current_size;
		DataVirAddr += current_size;
		Size -= current_size;
	}

	return TRUE;
}

BOOL CopyGuestVirt(UINT64 SrcDirbase, GVA SrcVa, UINT64 DestDirbase, GVA DestVa, UINT32 Size)
{
	while (Size)
	{
		auto dest_size = PAGE_4KB - Address{ DestVa }.Type4KB.offset_4kb;
		if (Size < dest_size)
			dest_size = Size;

		auto src_size = PAGE_4KB - Address{ SrcVa }.Type4KB.offset_4kb;
		if (Size < src_size)
			src_size = Size;

		const auto mapped_src = GvaToHva(SrcDirbase, SrcVa, MAP_MEMTORY_INDEX::P1);

		if (!mapped_src)
			return FALSE;

		const auto mapped_dest = GvaToHva(DestDirbase, DestVa, MAP_MEMTORY_INDEX::P2);

		if (!mapped_dest)
			return FALSE;

		auto current_size = min(dest_size, src_size);

		memcpy((PVOID)mapped_dest, (PVOID)mapped_src, current_size);

		SrcVa += current_size;
		DestVa += current_size;
		Size -= current_size;
	}
	return TRUE;
}