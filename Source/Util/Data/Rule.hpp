/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <spdlog/fmt/fmt.h>

#include "EBPFCommon.h"
#include "Types.hpp"

struct WTrafficItemRules : WTrafficItemRulesBase
{
	WBytesPerSecond UploadLimit{ 0 };
	WBytesPerSecond DownloadLimit{ 0 };

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(UploadSwitch, DownloadSwitch, UploadLimit, DownloadLimit);
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
};
