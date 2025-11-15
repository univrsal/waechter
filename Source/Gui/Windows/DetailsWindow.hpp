//
// Created by usr on 01/11/2025.
//

#pragma once
#include <string>

#if WAECHTER_WITH_IPQUERY_IO_API
	#include "Util/IpQueryIntegration.hpp"
#endif

class WTrafficTree;

class WDetailsWindow
{
	WTrafficTree* Tree;

#if WAECHTER_WITH_IPQUERY_IO_API
	WIpQueryIntegration IpQueryIntegration{};
	WIpInfoData const*  CurrentIpInfo{ nullptr };
#endif

	void DrawSystemDetails();
	void DrawApplicationDetails();
	void DrawProcessDetails();
	void DrawSocketDetails();

	std::string FormattedUptime{};

public:
	explicit WDetailsWindow(WTrafficTree* Tree_);
	void Draw();
};
