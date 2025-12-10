//
// Created by usr on 09/12/2025.
//

#pragma once
#include <cstdint>
#include <string>

#include "Singleton.hpp"

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

class WNetLink : public TSingleton<WNetLink>
{
	WNetLinkSocket Socket{};
	nl_cache*      Cache{};

public:
	WNetLink();
	~WNetLink();

	bool CreateIfbDevice(std::string const& IfbName) const;
	void DeleteIfbDevice(std::string const& IfbName) const;

	int GetIfIndex(std::string const& IfName);

	bool QdiscHtbReplaceRoot(std::string const& IfName, uint16_t Major, uint16_t DefaultMinor);
	bool ClassHtbReplace(std::string const& IfName, uint16_t ParentMajor, uint16_t ParentMinor, uint16_t ClassMajor,
		uint16_t ClassMinor, uint64_t RateBitsPerSec, uint64_t CeilBitsPerSec);
	bool FilterMatchAllToClassReplace(std::string const& IfName, uint16_t ParentMajor, uint16_t ParentMinor,
		uint16_t Priority, uint16_t FlowClassMajor, uint16_t FlowClassMinor);
};
