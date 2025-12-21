/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <unordered_map>
#include <mutex>

#include "Data/TrafficItem.hpp"
#include "Data/SocketItem.hpp"

struct WIpInfoData
{
	std::string Isp{};
	std::string Organization{};
	std::string Asn{};
	std::string Country{};
	std::string CountryCode{};
	std::string City{};
	std::string MapsLink{};

	double Latitude{ 0.0 };
	double Longitude{ 0.0 };

	bool bIsVPN{ false };
	bool bIsProxy{ false };
	bool bIsTor{ false };
	bool bIsDatacenter{ false };
	bool bIsMobile{ false };
	bool bIsPendingRequest{ true };
	int  RiskScore{ 0 };
};

class WIpQueryIntegration
{
	std::mutex Mutex;

	std::unordered_map<std::string, WIpInfoData> IpInfoCache;
	WIpInfoData const*                           CurrentIpInfo{};

	WTrafficItemId CurrentSocketItemId{};

public:
	WIpInfoData const* GetIpInfo(std::string const& IP);

	bool HasIpInfoForIp(std::string const& IP)
	{
		std::lock_guard Lock(Mutex);
		return IpInfoCache.contains(IP);
	}

	void Draw(std::shared_ptr<WSocketItem> Item);
};
