//
// Created by usr on 25/11/2025.
//

#pragma once
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
		return WTrafficItemRulesBase{ .UploadSwitch = UploadSwitch, .DownloadSwitch = DownloadSwitch };
	}
};
