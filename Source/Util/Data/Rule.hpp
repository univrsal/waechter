/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <spdlog/fmt/fmt.h>

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
		return WTrafficItemRulesBase{
			.UploadSwitch = UploadSwitch,
			.DownloadSwitch = DownloadSwitch,
			.UploadMark = UploadMark,
			.DownloadQdiscId = DownloadQdiscId,
		};
	}

	std::string ToString() const
	{
		return fmt::format(
			"UploadSwitch={}, DownloadSwitch={}, UploadLimit={}, DownloadLimit={}, UploadMark={}, DownloadQdiscId={}",
			static_cast<int>(UploadSwitch), static_cast<int>(DownloadSwitch), UploadLimit, DownloadLimit, UploadMark,
			DownloadQdiscId);
	}

	bool IsDefault() const
	{
		return UploadSwitch == SS_None && DownloadSwitch == SS_None && UploadMark == 0 && DownloadQdiscId == 0;
	}
};
