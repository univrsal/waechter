//
// Created by usr on 28/10/2025.
//

#pragma once
#include <spdlog/fmt/fmt.h>

#include "Types.hpp"

enum ETrafficUnit
{
	TU_Bps,
	TU_KiBps,
	TU_MiBps,
	TU_GiBps,
	TU_Auto
};

enum EStorageUnit
{
	SU_Bytes,
	SU_KiB,
	SU_MiB,
	SU_GiB,
	SU_Auto
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
	static WBytesPerSecond ConvertToBps(double Value, ETrafficUnit ToUnit)
	{
		switch (ToUnit)
		{
			case TU_Bps:
				return Value;
			case TU_KiBps:
				return Value WKiB;
			case TU_MiBps:
				return Value WMiB;
			case TU_GiBps:
				return Value WGiB;
			case TU_Auto:
			default:
				assert(false);
				return Value;
		}
	}
};

class WStorageFormat
{
public:
	static std::string AutoFormat(WBytes Bytes_)
	{
		auto Bytes = static_cast<double>(Bytes_); // should be fine
		if (Bytes < 1024)
		{
			return fmt::format("{:.2f} B", Bytes);
		}
		if (Bytes < 1024 * 1024)
		{
			return fmt::format("{:.2f} KiB", Bytes / 1024.0);
		}
		if (Bytes < 1024 * 1024 * 1024)
		{
			return fmt::format("{:.2f} MiB", Bytes / (1024.0 * 1024.0));
		}
		return fmt::format("{:.2f} GiB", Bytes / (1024.0 * 1024.0 * 1024.0));
	}

	static std::string Format(WBytes Bytes_, EStorageUnit Unit)
	{
		auto Bytes = static_cast<double>(Bytes_); // should be fine
		switch (Unit)
		{
			case SU_Bytes:
				return fmt::format("{:.2f} B", Bytes);
			case SU_KiB:
				return fmt::format("{:.2f} KiB", Bytes / 1024.0);
			case SU_MiB:
				return fmt::format("{:.2f} MiB", Bytes / (1024.0 * 1024.0));
			case SU_GiB:
				return fmt::format("{:.2f} GiB", Bytes / (1024.0 * 1024.0 * 1024.0));
			case SU_Auto:
				return AutoFormat(Bytes_);
			default:
				return fmt::format("{:.2f} B", Bytes);
		}
	}
};