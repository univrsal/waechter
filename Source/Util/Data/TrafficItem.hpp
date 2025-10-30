//
// Created by usr on 30/10/2025.
//

#pragma once

#include "Types.hpp"

using WTrafficItemId = unsigned long long;

struct ITrafficItem
{
	WTrafficItemId  ItemId{};
	WBytesPerSecond DownloadSpeed{};
	WBytesPerSecond UploadSpeed{};
};
