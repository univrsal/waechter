/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>

#include "TrafficItem.hpp"

class WFilterItem : public ITrafficItem
{
public:
	std::string Name;
	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(ItemId, DownloadSpeed, UploadSpeed, TotalDownloadBytes, TotalUploadBytes, Name);
	}

	[[nodiscard]] ETrafficItemType GetType() const override { return TI_Filter; }
};
