#pragma once
#include "Defs.h"
#include "vcruntime_string.h"


// 留着空位 用来同一个函数多个地址需要映射的时候  不打架..
enum MAP_MEMTORY_INDEX
{
	P1 = 0,
	P2,
	P3
};
#define MAP_MEMTORY_INDEX_SIZE 3

#pragma section(".pdpt", read, write)
#pragma section(".pd", read, write)
#pragma section(".pt", read, write)
#pragma section(".sdpt", read, write)

extern ShadowPt g_shadow_pt[10];


UINT64 GetMapVirtual(UINT16 offset, MAP_MEMTORY_INDEX index);

HVA HpaToHva(HPA HostPhyAddr, MAP_MEMTORY_INDEX index);

HPA HvaToHpa(HVA HostVirtureAddress);

HPA GpaToHpa(GPA GuestPhyAddr, MAP_MEMTORY_INDEX index);


HVA GpaToHva(GPA GuestPhyAddr, MAP_MEMTORY_INDEX index);


GPA GvaToGpa(UINT64 Dirbase, GVA GuestVirAddr, MAP_MEMTORY_INDEX index);


HVA GvaToHva(UINT64 Dirbase, GVA GuestVirAddr, MAP_MEMTORY_INDEX index);


BOOL InitPageTable();

BOOL ReadGuestPhy(__IN__ UINT64 Dirbase, __IN__ GPA ReadPhyAddr, __OUT__ GVA WriteVirAddr, __IN__ UINT32 Size);

BOOL WriteGuestPhy(UINT64 Dirbase, GPA WritePhyAddr, GVA DataVirAddr, UINT32 Size);

BOOL CopyGuestVirt(UINT64 SrcDirbase, GVA SrcVa, UINT64 DestDirbase, GVA DestVa, UINT32 Size);
