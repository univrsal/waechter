//
// Created by usr on 09/12/2025.
//

#pragma once
#include <string>

struct nl_sock;
struct nl_cache;

class WNetLinkSocket
{
	nl_sock* NlSocket{};

public:
	WNetLinkSocket();
	~WNetLinkSocket();

	WNetLinkSocket(WNetLinkSocket const&) = delete;
	WNetLinkSocket& operator=(WNetLinkSocket const&) = delete;

	nl_sock* Get() const { return NlSocket; }
};

class WNetLink
{
	WNetLinkSocket Socket{};
	nl_cache*      Cache{};

public:
	WNetLink();
	~WNetLink();

	bool CreateIfbDevice(std::string const& IfbName) const;
	void DeleteIfbDevice(std::string const& IfbName) const;
};
