#pragma once
#include "hv.h"


namespace VUtils
{
	Command GetCommand(GuestVirtureAddress command_ptr);

	void SetCommand(GuestVirtureAddress command_ptr, Command& command_data);
}
