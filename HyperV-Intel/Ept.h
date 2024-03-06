#pragma once
#include "Defs.h"
#include "Debug.h"
#include "asm.h"
#include "Memory.h"


UINT8 UtilInveptGlobal(ept_pointer eptPoint);

BOOL Is4kPage(ept_pointer eptp, GPA gpa);

void Set2mbTo4kb(ept_pointer eptp, GPA gpa, UINT32 saveIndex);

BOOL SetEptPtAttr(ept_pointer eptp, GPA gpa, UINT64 pfn, bool canExec);

ept_pte GetEptPt(ept_pointer eptp, GPA gpa);

