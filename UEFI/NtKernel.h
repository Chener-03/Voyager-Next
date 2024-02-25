#pragma once
#include "EUtils.h"
#include "Defs.h"




// 映射pe iamge  （展开节  修复重定位表   修复导入表）
VOID MapPeImage(VOID* PeFilePtr, VOID* PeMemPtr, VOID* KernelOsPtr);
