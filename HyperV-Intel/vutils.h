#pragma once
#include "Defs.h"


namespace VUtils
{
	Command GetCommand(GVA command_ptr);

	void SetCommand(GVA command_ptr, Command& command_data);

	UINT32 GetCurrentCpuIndex();

}
