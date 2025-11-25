//
// Created by usr on 25/11/2025.
//

#pragma once
#include "EBPFCommon.h"
#include "Types.hpp"

struct WTrafficItemRules : WTrafficItemRulesBase
{
	WBytesPerSecond UploadLimit{ -1 };
	WBytesPerSecond DownloadLimit{ -1 };

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(UploadSwitch, DownloadSwitch, UploadLimit, DownloadLimit);
	}
};
