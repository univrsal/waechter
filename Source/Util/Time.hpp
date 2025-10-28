#pragma once

#include <chrono>

#include "Types.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

namespace WTime
{
	static WMsec GetEpochMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
} // namespace WTime