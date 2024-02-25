#pragma once
#include "Utils.h"




//��ȡ�̼�·�� \??\GLOBALROOT\ArcName\multi(0)disk(0)rdisk(0)partition(1)
/*	\??\������һ�������ռ�ǰ׺�����ڱ�ʾ��Object Manager Namespace����
	GLOBALROOT\ArcName\������һ�������豸�������ռ䣬���ڱ�ʾ ARC��Advanced RISC Computing��·����
	multi(0)disk(0)rdisk(0)partition(1)���ⲿ�������˾�����豸�ͷ�����Ϣ��
	multi(0)����ʾ�ദ����ϵͳ������ 0 ��ʾ��һ����������
	disk(0)����ʾ��һ�����̡�
	rdisk(0)����ʾ��һ��������̣�raw disk����
	partition(1)����ʾ��һ��������*/
std::string GetBootfwPath();

// ȥ������ǩ����֤
void BootNoCertPatch(PVOID image, UINT32 size);

void PatchBootPe(std::pair<PVOID, UINT32>& bootImg, std::pair<PVOID, UINT32>& bootKitImg, PEFile* bootPe, PEFile* bootkitPe);

