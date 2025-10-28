//
// Created by usr on 28/10/2025.
//

#pragma once

#include "Types.hpp"

#include <fmt/format.h>

enum ETrafficUnit
{
	TU_Bps,
	TU_KiBps,
	TU_MiBps,
	TU_GiBps,
	TU_Auto
};

class WTrafficFormat
{
public:
	static std::string AutoFormat(WBytesPerSecond Speed)
	{
		if (Speed < 1024)
		{
			return fmt::format("{:.2f} B/s", Speed);
		}
		if (Speed < 1024 * 1024)
		{
			return fmt::format("{:.2f} KiB/s", Speed / 1024.0);
		}
		if (Speed < 1024 * 1024 * 1024)
		{
			return fmt::format("{:.2f} MiB/s", Speed / (1024.0 * 1024.0));
		}
		return fmt::format("{:.2f} GiB/s", Speed / (1024.0 * 1024.0 * 1024.0));
	}

	static std::string Format(WBytesPerSecond Speed, ETrafficUnit Unit)
	{
		switch (Unit)
		{
			case TU_Bps:
				return fmt::format("{:.2f} B/s", Speed);
			case TU_KiBps:
				return fmt::format("{:.2f} KiB/s", Speed / 1024.0);
			case TU_MiBps:
				return fmt::format("{:.2f} MiB/s", Speed / (1024.0 * 1024.0));
			case TU_GiBps:
				return fmt::format("{:.2f} GiB/s", Speed / (1024.0 * 1024.0 * 1024.0));
			case TU_Auto:
				return AutoFormat(Speed);
			default:
				return fmt::format("{:.2f} B/s", Speed);
		}
	}
};