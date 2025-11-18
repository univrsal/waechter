//
// Created by usr on 17/11/2025.
//

#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

#include "IPAddress.hpp"
#include "Singleton.hpp"

class WResolver : public TSingleton<WResolver>
{
	std::unordered_map<WIPAddress, std::string> ResolvedAddresses{};

public:
	std::string const& Resolve(WIPAddress const& Address);

	std::mutex                                  ResolvedAddressesMutex;
	std::unordered_map<WIPAddress, std::string> GetResolvedAddresses() const { return ResolvedAddresses; }
};