/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <ctime>
#include <cassert>
#include <string_view>

#include "spdlog/fmt/chrono.h"

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

class WStringFormat
{
public:
	static std::string Trim(std::string const& Str)
	{
		size_t const First = Str.find_first_not_of(' ');
		if (First == std::string::npos)
		{
			return "";
		}
		size_t const Last = Str.find_last_not_of(' ');
		return Str.substr(First, (Last - First + 1));
	}
};

class WTimeFormat
{
public:
	static std::string FormatUnixTime(WSec UnixTime, std::string_view Format = "{:%Y-%m-%d %H:%M:%S}")
	{
		std::time_t const TimeT = UnixTime;
		return fmt::format(fmt::runtime(std::string{ Format }), fmt::localtime(TimeT));
	}
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

	static double ConvertFromBps(double Value, ETrafficUnit ToUnit)
	{
		switch (ToUnit)
		{
			case TU_Bps:
				return Value;
			case TU_KiBps:
				return Value / 1024.0;
			case TU_MiBps:
				return Value / (1024.0 * 1024.0);
			case TU_GiBps:
				return Value / (1024.0 * 1024.0 * 1024.0);
			case TU_Auto:
			default:
				assert(false);
				// ReSharper disable once CppDFAUnreachableCode
				return Value;
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
				// ReSharper disable once CppDFAUnreachableCode
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
		auto Bytes = static_cast<double>(Bytes_);
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