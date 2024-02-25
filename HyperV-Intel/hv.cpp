#include "hv.h"
#include "vcruntime_string.h"


namespace HV
{
	PPml4e g_HyperVPlm4e = { (PPml4e)SELF_REF_PML4 };

	__declspec(allocate(".pdpt")) Pdpte g_pdpt[512];
	__declspec(allocate(".pd")) Pde g_pd[512];
	__declspec(allocate(".pt")) Pte g_pt[512];


	HostPhysicalAddress Translate(HostVirtureAddress host_virt)
	{
		VirtualAddress virt_addr = { host_virt };
		VirtualAddress cursor = { (u64)g_HyperVPlm4e };


		if (!reinterpret_cast<PPml4e>(cursor.value)[virt_addr.pml4_index].present)
			return 0;

		cursor.pt_index = virt_addr.pml4_index;
		if (!reinterpret_cast<PPdpte>(cursor.value)[virt_addr.pdpt_index].present)
			return 0;

		// handle 1gb large page...
		if (reinterpret_cast<PPdpte>(cursor.value)[virt_addr.pdpt_index].large_page)
			return (reinterpret_cast<PPdpte>(cursor.value)[virt_addr.pdpt_index].pfn << 12) + virt_addr.offset_1gb;


		cursor.pd_index = virt_addr.pml4_index;
		cursor.pt_index = virt_addr.pdpt_index;
		if (!reinterpret_cast<PPde>(cursor.value)[virt_addr.pd_index].present)
			return 0;

		// handle 2mb large page...
		if (reinterpret_cast<PPde>(cursor.value)[virt_addr.pd_index].large_page)
			return (reinterpret_cast<PPde>(cursor.value)[virt_addr.pd_index].pfn << 12) + virt_addr.offset_2mb;



		cursor.pdpt_index = virt_addr.pml4_index;
		cursor.pd_index = virt_addr.pdpt_index;
		cursor.pt_index = virt_addr.pd_index;
		if (!reinterpret_cast<PPte>(cursor.value)[virt_addr.pt_index].present)
			return 0;

		return (reinterpret_cast<PPte>(cursor.value)[virt_addr.pt_index].pfn << 12) + virt_addr.offset_4kb;


	}

	u64 TranslateGuestPhysical(GuestPhysicalAddress phys_addr, MapType map_type)
	{
		ept_pointer eptp;
		PhysicalAddress guest_phys{ phys_addr };
		__vmx_vmread(VMCS_CTRL_EPT_POINTER, (size_t*)&eptp);

		const auto epml4 = reinterpret_cast<ept_pml4e*>(MapPage(eptp.page_frame_number << 12, map_type));

		const auto epdpt_large =
			reinterpret_cast<ept_pdpte_1gb*>(MapPage(
				epml4[guest_phys.pml4_index].page_frame_number << 12, map_type));

		// handle 1gb page...
		if (epdpt_large[guest_phys.pdpt_index].large_page)
			return (epdpt_large[guest_phys.pdpt_index].page_frame_number
				* 0x1000 * 0x200 * 0x200) + EPT_LARGE_PDPTE_OFFSET(phys_addr);

		const auto epdpt =
			reinterpret_cast<ept_pdpte*>(epdpt_large);

		const auto epd_large =
			reinterpret_cast<epde_2mb*>(MapPage(
				epdpt[guest_phys.pdpt_index].page_frame_number << 12, map_type));

		// handle 2mb page...
		if (epd_large[guest_phys.pd_index].large_page)
			return (epd_large[guest_phys.pd_index].page_frame_number
				* 0x1000 * 0x200) + EPT_LARGE_PDE_OFFSET(phys_addr);

		const auto epd =
			reinterpret_cast<ept_pde*>(epd_large);

		const auto ept =
			reinterpret_cast<ept_pte*>(MapPage(
				epd[guest_phys.pd_index].page_frame_number << 12, map_type));

		auto result = ept[guest_phys.pt_index].page_frame_number << 12;
		return result;
	}

	u64 TranslateGuestVirtual(GuestPhysicalAddress dirbase, GuestVirtureAddress guest_virt, MapType map_type)
	{
		VirtualAddress virt_addr{ guest_virt };
		const auto pml4 =
			reinterpret_cast<Pml4e*>(MapGuestPhys(dirbase, map_type));

		if (!pml4[virt_addr.pml4_index].present)
			return {};

		const auto pdpt =
			reinterpret_cast<Pdpte*>(MapGuestPhys(
				pml4[virt_addr.pml4_index].pfn << 12, map_type));

		if (!pdpt[virt_addr.pdpt_index].present)
			return {};

		// handle 1gb pages...
		if (pdpt[virt_addr.pdpt_index].large_page)
			return (pdpt[virt_addr.pdpt_index].pfn << 12) + virt_addr.offset_1gb;

		const auto pd =
			reinterpret_cast<Pde*>(MapGuestPhys(
				pdpt[virt_addr.pdpt_index].pfn << 12, map_type));

		if (!pd[virt_addr.pd_index].present)
			return {};

		// handle 2mb pages...
		if (pd[virt_addr.pd_index].large_page)
			return (pd[virt_addr.pd_index].pfn << 12) + virt_addr.offset_2mb;

		const auto pt =
			reinterpret_cast<Pte*>(MapGuestPhys(
				pd[virt_addr.pd_index].pfn << 12, map_type));

		if (!pt[virt_addr.pt_index].present)
			return {};

		return (pt[virt_addr.pt_index].pfn << 12) + virt_addr.offset_4kb;
	}


	u64 MapPage(HostPhysicalAddress phys_addr, MapType map_type)
	{
		// 将给定的物理地址映射到虚拟地址空间中，更新页表，并确保 TLB 中的缓存被正确刷新 返回映射后的虚拟地址

		cpuid_eax_01 cpuid_value;
		__cpuid((int*)&cpuid_value, 1);

		g_pt[(cpuid_value.cpuid_additional_information.initial_apic_id * 2) + (unsigned)map_type].pfn = phys_addr >> 12;

		__invlpg(reinterpret_cast<void*>(GetMapVirtual(VirtualAddress{ phys_addr }.offset_4kb, map_type)));

		return GetMapVirtual(VirtualAddress{ phys_addr }.offset_4kb, map_type);
	}


	u64 GetMapVirtual(u16 offset, MapType map_type)
	{
		cpuid_eax_01 cpuid_value;
		__cpuid((int*)&cpuid_value, 1);
		VirtualAddress virt_addr{ MAPPING_ADDRESS_BASE };

		virt_addr.pt_index = (cpuid_value.cpuid_additional_information.initial_apic_id * 2) + (unsigned)map_type;

		return virt_addr.value + offset;
	}


	VMX_ROOT_RESULT InitPageTable()
	{
		HostPhysicalAddress pdpt_phys = Translate(reinterpret_cast<u64>(g_pdpt));

		HostPhysicalAddress pd_phys = Translate(reinterpret_cast<u64>(g_pd));

		HostPhysicalAddress pt_phys = Translate(reinterpret_cast<u64>(g_pt));

		if (!pdpt_phys || !pd_phys || !pt_phys)
			return VMX_ROOT_RESULT::invalid_host_virtual;

		// 设置映射页表项。。。
		{
			g_HyperVPlm4e[MAPPING_PML4_IDX].present = true;
			g_HyperVPlm4e[MAPPING_PML4_IDX].pfn = pdpt_phys >> 12;
			g_HyperVPlm4e[MAPPING_PML4_IDX].user_supervisor = false;
			g_HyperVPlm4e[MAPPING_PML4_IDX].writeable = true;

			g_pdpt[511].present = true;
			g_pdpt[511].pfn = pd_phys >> 12;
			g_pdpt[511].user_supervisor = false;
			g_pdpt[511].rw = true;

			g_pd[511].present = true;
			g_pd[511].pfn = pt_phys >> 12;
			g_pd[511].user_supervisor = false;
			g_pd[511].rw = true;
		}

		//每个核心都有自己的页面，可以用来映射
		//物理内存到虚拟内存：^）
		{
			for (auto idx = 0u; idx < 512; ++idx)
			{
				g_pt[idx].present = true;
				g_pt[idx].user_supervisor = false;
				g_pt[idx].rw = true;
			}
		}


		PPml4e mapped_pml4 = reinterpret_cast<PPml4e>(MapPage(__readcr3()));


		// 检查以确保翻译有效。。。
		if (Translate((u64)mapped_pml4) != __readcr3())
			return VMX_ROOT_RESULT::vmxroot_translate_failure;

		// 检查以确保self-ref pml4e有效。。。
		if (mapped_pml4[SELF_REF_PML4_IDX].pfn != __readcr3() >> 12)
			return VMX_ROOT_RESULT::invalid_self_ref_pml4e;

		// 检查以确保映射pml4e有效。。。
		if (mapped_pml4[MAPPING_PML4_IDX].pfn != pdpt_phys >> 12)
			return VMX_ROOT_RESULT::invalid_mapping_pml4e;


		return VMX_ROOT_RESULT::success;
	}

	u64 MapGuestPhys(GuestPhysicalAddress phys_addr, MapType map_type)
	{
		const auto host_phys = TranslateGuestPhysical(phys_addr, map_type);

		if (!host_phys)
			return 0;

		return MapPage(host_phys, map_type);
	}

	u64 MapGuestVirt(GuestPhysicalAddress dirbase, GuestVirtureAddress virt_addr, MapType map_type)
	{
		const auto guest_phys = TranslateGuestVirtual(dirbase, virt_addr, map_type);

		if (!guest_phys)
			return 0;

		return MapGuestPhys(guest_phys, map_type);
	}



	VMX_ROOT_RESULT ReadGuestPhys(GuestPhysicalAddress dirbase, GuestPhysicalAddress guest_phys, GuestVirtureAddress guest_virt, u64 size)
	{
		// 处理对src和dest的页面边界的读取。。。
		while (size)
		{
			auto dest_current_size = PAGE_4KB -
				VirtualAddress{ guest_virt }.offset_4kb;

			if (size < dest_current_size)
				dest_current_size = size;

			auto src_current_size = PAGE_4KB -
				PhysicalAddress{ guest_phys }.offset_4kb;

			if (size < src_current_size)
				src_current_size = size;

			auto current_size =
				min(dest_current_size, src_current_size);

			const auto mapped_dest =
				reinterpret_cast<void*>(
					MapGuestVirt(dirbase, guest_virt, MapType::map_dest));

			if (!mapped_dest)
				return VMX_ROOT_RESULT::invalid_guest_virtual;

			const auto mapped_src =
				reinterpret_cast<void*>(
					MapGuestPhys(guest_phys, MapType::map_src));

			if (!mapped_src)
				return VMX_ROOT_RESULT::invalid_guest_physical;

			memcpy(mapped_dest, mapped_src, current_size);
			guest_phys += current_size;
			guest_virt += current_size;
			size -= current_size;
		}

		return VMX_ROOT_RESULT::success;
	}

	VMX_ROOT_RESULT WriteGuestPhys(GuestPhysicalAddress dirbase, GuestPhysicalAddress guest_phys, GuestVirtureAddress guest_virt, u64 size)
	{
		while (size)
		{
			auto dest_current_size = PAGE_4KB -
				VirtualAddress{ guest_virt }.offset_4kb;

			if (size < dest_current_size)
				dest_current_size = size;

			auto src_current_size = PAGE_4KB -
				PhysicalAddress{ guest_phys }.offset_4kb;

			if (size < src_current_size)
				src_current_size = size;

			auto current_size =
				min(dest_current_size, src_current_size);

			const auto mapped_src =
				reinterpret_cast<void*>(
					MapGuestVirt(dirbase, guest_virt, MapType::map_src));

			if (!mapped_src)
				return VMX_ROOT_RESULT::invalid_guest_virtual;

			const auto mapped_dest =
				reinterpret_cast<void*>(
					MapGuestPhys(guest_phys, MapType::map_dest));

			if (!mapped_src)
				return VMX_ROOT_RESULT::invalid_guest_physical;

			memcpy(mapped_dest, mapped_src, current_size);
			guest_phys += current_size;
			guest_virt += current_size;
			size -= current_size;
		}

		return VMX_ROOT_RESULT::success;
	}

	VMX_ROOT_RESULT CopyGuestVirt(GuestPhysicalAddress dirbase_src, GuestVirtureAddress virt_src, GuestVirtureAddress dirbase_dest, GuestVirtureAddress virt_dest, u64 size)
	{
		while (size)
		{
			auto dest_size = PAGE_4KB - VirtualAddress{ virt_dest }.offset_4kb;
			if (size < dest_size)
				dest_size = size;

			auto src_size = PAGE_4KB - VirtualAddress{ virt_src }.offset_4kb;
			if (size < src_size)
				src_size = size;

			const auto mapped_src =
				reinterpret_cast<void*>(
					MapGuestVirt(dirbase_src, virt_src, MapType::map_src));

			if (!mapped_src)
				return VMX_ROOT_RESULT::invalid_guest_virtual;

			const auto mapped_dest =
				reinterpret_cast<void*>(
					MapGuestVirt(dirbase_dest, virt_dest, MapType::map_dest));

			if (!mapped_dest)
				return VMX_ROOT_RESULT::invalid_guest_virtual;

			auto current_size = min(dest_size, src_size);
			memcpy(mapped_dest, mapped_src, current_size);

			virt_src += current_size;
			virt_dest += current_size;
			size -= current_size;
		}

		return VMX_ROOT_RESULT::success;
	}

}
