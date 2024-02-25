#pragma once
#include "types.h"



#define SELF_REF_PML4_IDX 510
#define MAPPING_PML4_IDX 100

#define MAPPING_ADDRESS_BASE 0x0000327FFFE00000
/*
 * PML4E self-ref. address;
 * Use index of PAGE_SELF_REF_PML4E_INDEX;
 *
 *
 *                                 |<-- PTE operate start here
 *                                 |           |<-- PDE operate start here
 *                                 |           |           |<-- PDPE operate start here
 * 1111 1111 1111 1111. 1111 1111 0.111 1111 10.11 1111 110.1 1111 1110. 0000 0000 0000
 * -------------------| -----------|-----------|-----------|-----------| --------------
 *    F    F    F    F|    F    F  |  7    F   | B    F    |D    F    E|    0    0    0
 * -------------------| -----------|-----------|-----------|-----------| --------------
 *         Sign Extend|       PML4E|       PDPE|        PDE|        PTE|         Offset
 * -------------------| -----------|-----------|-----------|-----------| --------------
 *                    |            |           |           |           |
 *
 */
#define SELF_REF_PML4 0xFFFFFF7FBFDFE000


#define EPT_LARGE_PDPTE_OFFSET(_) (((u64)(_)) & ((0x1000 * 0x200 * 0x200) - 1))
#define EPT_LARGE_PDE_OFFSET(_) (((u64)(_)) & ((0x1000 * 0x200) - 1))

#pragma section(".pdpt", read, write)
#pragma section(".pd", read, write)
#pragma section(".pt", read, write)
 



namespace HV
{

    typedef union _Pml4e
    {
        u64 value;
        struct
        {
            u64 present : 1;
            u64 writeable : 1;
            u64 user_supervisor : 1;
            u64 page_write_through : 1;
            u64 page_cache : 1;
            u64 accessed : 1;
            u64 ignore_1 : 1;
            u64 page_size : 1;
            u64 ignore_2 : 4;
            u64 pfn : 36;
            u64 reserved : 4;
            u64 ignore_3 : 11;
            u64 nx : 1;
        };
    } Pml4e, * PPml4e;

    typedef union _Pdpte
    {
        u64 value;
        struct
        {
            u64 present : 1;
            u64 rw : 1;
            u64 user_supervisor : 1;
            u64 page_write_through : 1;
            u64 page_cache : 1;
            u64 accessed : 1;
            u64 ignore_1 : 1;
            u64 large_page : 1;
            u64 ignore_2 : 4;
            u64 pfn : 36;
            u64 reserved : 4;
            u64 ignore_3 : 11;
            u64 nx : 1;
        };
    } Pdpte, * PPdpte;

    typedef union _Pde
    {
        u64 value;
        struct
        {
            u64 present : 1;
            u64 rw : 1;
            u64 user_supervisor : 1;
            u64 page_write_through : 1;
            u64 page_cache : 1;
            u64 accessed : 1;
            u64 ignore_1 : 1;
            u64 large_page : 1;
            u64 ignore_2 : 4;
            u64 pfn : 36;
            u64 reserved : 4;
            u64 ignore_3 : 11;
            u64 nx : 1;
        };
    } Pde, * PPde;

    typedef union _Pte
    {
        u64 value;
        struct
        {
            u64 present : 1;
            u64 rw : 1;
            u64 user_supervisor : 1;
            u64 page_write_through : 1;
            u64 page_cache : 1;
            u64 accessed : 1;
            u64 dirty : 1;
            u64 access_type : 1;
            u64 global : 1;
            u64 ignore_2 : 3;
            u64 pfn : 36;
            u64 reserved : 4;
            u64 ignore_3 : 7;
            u64 pk : 4;
            u64 nx : 1;
        };
    } Pte, * PPte;


    typedef union _VirtualAddress
    {
        u64 value;
        struct
        {
            u64 offset_4kb : 12;
            u64 pt_index : 9;
            u64 pd_index : 9;
            u64 pdpt_index : 9;
            u64 pml4_index : 9;
            u64 reserved : 16;
        };

        struct
        {
            u64 offset_2mb : 21;
            u64 pd_index : 9;
            u64 pdpt_index : 9;
            u64 pml4_index : 9;
            u64 reserved : 16;
        };

        struct
        {
            u64 offset_1gb : 30;
            u64 pdpt_index : 9;
            u64 pml4_index : 9;
            u64 reserved : 16;
        };

    } VirtualAddress, * PVirtualAddress;
    using PhysicalAddress = VirtualAddress;

    extern PPml4e g_HyperVPlm4e;

    extern Pdpte g_pdpt[512];
    extern Pde g_pd[512];
    extern Pte g_pt[512];

    // Host中将虚拟地址转换为 Host的物理地址
    HostPhysicalAddress Translate(HostVirtureAddress host_virt);
    // 将（Guest）物理地址翻译成（Host）的物理地址
    u64 TranslateGuestPhysical(GuestPhysicalAddress guest_phys, MapType map_type = MapType::map_src);
    // 将（Guest）虚拟地址翻译成（Host）的虚拟地址
    u64 TranslateGuestVirtual(GuestPhysicalAddress dirbase, GuestVirtureAddress guest_virt, MapType map_type = MapType::map_src);


    u64 MapPage(HostPhysicalAddress phys_addr, MapType map_type = MapType::map_src) ;
    u64 GetMapVirtual(u16 offset = 0u, MapType map_type = MapType::map_src);


	VMX_ROOT_RESULT InitPageTable();
    u64 MapGuestPhys(GuestPhysicalAddress phys_addr, MapType map_type = MapType::map_src) ;
    u64 MapGuestVirt(GuestPhysicalAddress dirbase, GuestVirtureAddress virt_addr, MapType map_type = MapType::map_src) ;



    VMX_ROOT_RESULT ReadGuestPhys(GuestPhysicalAddress dirbase, GuestPhysicalAddress guest_phys, GuestVirtureAddress guest_virt, u64 size);
    VMX_ROOT_RESULT WriteGuestPhys(GuestPhysicalAddress dirbase, GuestPhysicalAddress guest_phys, GuestVirtureAddress guest_virt, u64 size);
    VMX_ROOT_RESULT CopyGuestVirt(GuestPhysicalAddress dirbase_src, GuestVirtureAddress virt_src, GuestVirtureAddress dirbase_dest, GuestVirtureAddress virt_dest, u64 size);

}

	