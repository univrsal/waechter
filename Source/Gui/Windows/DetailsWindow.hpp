/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <string>

#if WAECHTER_WITH_IPQUERY_IO_API
	#include "Util/IpQueryIntegration.hpp"
#endif

class WTrafficTree;

class WDetailsWindow
{
	std::shared_ptr<WTrafficTree> Tree{};

#if WAECHTER_WITH_IPQUERY_IO_API
	WIpQueryIntegration IpQueryIntegration{};
	WIpInfoData const*  CurrentIpInfo{ nullptr };
#endif

	void DrawSystemDetails();
	void DrawApplicationDetails();
	void DrawProcessDetails();
	void DrawSocketDetails();
	void DrawTupleDetails();

	std::string FormattedUptime{};

public:
	explicit WDetailsWindow();
	void Draw();
};
