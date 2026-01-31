/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "EBPFCommon.h"
#include "Types.hpp"

enum class ERuleType : uint8_t
{
	Implicit, // set indirectly through a parent rule
	Explicit, // set by user
};

struct WTrafficItemRules : WTrafficItemRulesBase
{
	WBytesPerSecond UploadLimit{ 0 };
	WBytesPerSecond DownloadLimit{ 0 };

	ERuleType RuleType{ ERuleType::Implicit };

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(UploadSwitch, DownloadSwitch, UploadLimit, DownloadLimit, RuleType);
	}

	[[nodiscard]] WTrafficItemRulesBase AsBase() const
	{
		WTrafficItemRulesBase Base;
		Base.UploadSwitch = UploadSwitch;
		Base.DownloadSwitch = DownloadSwitch;
		Base.UploadMark = UploadMark;
		Base.DownloadMark = DownloadMark;

		return Base;
	}

	std::string ToString() const
	{
		return std::format(
			"UploadSwitch={}, DownloadSwitch={}, UploadLimit={}, DownloadLimit={}, UploadMark={}, DownloadMark={}",
			static_cast<int>(UploadSwitch), static_cast<int>(DownloadSwitch), UploadLimit, DownloadLimit, UploadMark,
			DownloadMark);
	}

	bool IsDefault() const
	{
		return UploadSwitch == SS_None && DownloadSwitch == SS_None && UploadMark == 0 && DownloadMark == 0;
	}
};
