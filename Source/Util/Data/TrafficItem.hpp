//
// Created by usr on 30/10/2025.
//

#pragma once

#include "Types.hpp"

using WTrafficItemId = unsigned long long;

enum ETrafficItemType : uint8_t
{
	TI_System,
	TI_Application,
	TI_Process,
	TI_Socket
};

struct ITrafficItem
{
	WTrafficItemId  ItemId{};
	WBytesPerSecond DownloadSpeed{};
	WBytesPerSecond UploadSpeed{};
};
