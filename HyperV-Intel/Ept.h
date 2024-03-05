#pragma once
#include "Defs.h"
#include "Debug.h"
#include "asm.h"
#include "Memory.h"


UINT8 UtilInveptGlobal(ept_pointer eptPoint);

BOOL Is4kPage(ept_pointer eptp, HPA hpa);

void Set2mbTo4kb(ept_pointer eptp, HPA hpa);

