#pragma once
#include "Utils.h"




//获取固件路径 \??\GLOBALROOT\ArcName\multi(0)disk(0)rdisk(0)partition(1)
/*	\??\：这是一个命名空间前缀，用于表示“Object Manager Namespace”。
	GLOBALROOT\ArcName\：这是一个引导设备的命名空间，用于表示 ARC（Advanced RISC Computing）路径。
	multi(0)disk(0)rdisk(0)partition(1)：这部分描述了具体的设备和分区信息。
	multi(0)：表示多处理器系统，数字 0 表示第一个处理器。
	disk(0)：表示第一个磁盘。
	rdisk(0)：表示第一个物理磁盘（raw disk）。
	partition(1)：表示第一个分区。*/
std::string GetBootfwPath();

// 去除数字签名验证
void BootNoCertPatch(PVOID image, UINT32 size);

void PatchBootPe(std::pair<PVOID, UINT32>& bootImg, std::pair<PVOID, UINT32>& bootKitImg, PEFile* bootPe, PEFile* bootkitPe);

