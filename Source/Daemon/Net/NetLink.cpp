//
// Created by usr on 09/12/2025.
//

#include "NetLink.hpp"

#include <spdlog/spdlog.h>
#include <netlink/socket.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <linux/if.h>

WNetLinkSocket::WNetLinkSocket()
{
	NlSocket = nl_socket_alloc();
	if (!NlSocket)
	{
		spdlog::critical("Failed to allocate netlink socket");
	}

	if (nl_connect(NlSocket, NETLINK_ROUTE) < 0)
	{
		spdlog::critical("Failed to connect to netlink socket");
	}
}

WNetLinkSocket::~WNetLinkSocket()
{
	if (NlSocket)
	{
		nl_close(NlSocket);
		nl_socket_free(NlSocket);
	}
}

WNetLink::WNetLink()
{
	if (rtnl_link_alloc_cache(Socket.Get(), AF_UNSPEC, &Cache) < 0)
	{
		spdlog::critical("Failed to allocate netlink link cache");
	}
}

WNetLink::~WNetLink()
{
	if (Cache)
	{
		nl_cache_free(Cache);
	}
}

bool WNetLink::CreateIfbDevice(std::string const& IfbName) const
{
	auto* Link = rtnl_link_get_by_name(Cache, IfbName.c_str());
	if (Link)
	{
		rtnl_link_put(Link);
		spdlog::info("IFB device '{}' already exists", IfbName);
		return true;
	}

	auto* NewLink = rtnl_link_alloc();
	if (!NewLink)
	{
		spdlog::error("Failed to allocate new rtnl_link for IFB device '{}'", IfbName);
		return false;
	}

	rtnl_link_set_name(NewLink, IfbName.c_str());

	if (rtnl_link_set_type(NewLink, "ifb") < 0)
	{
		spdlog::error("Failed to set type 'ifb' for device '{}'", IfbName);
		rtnl_link_put(NewLink);
		return false;
	}

	int Err = rtnl_link_add(Socket.Get(), NewLink, NLM_F_CREATE | NLM_F_EXCL);
	rtnl_link_put(NewLink);
	if (Err < 0)
	{
		spdlog::error("Failed to add IFB device '{}': {}", IfbName, nl_geterror(Err));
		return false;
	}

	rtnl_link_set_flags(NewLink, IFF_UP);
	Err = rtnl_link_change(Socket.Get(), NewLink, NewLink, 0);
	if (Err < 0)
	{
		spdlog::error("Failed to set IFB device '{}' up: {}", IfbName, nl_geterror(Err));
		return false;
	}
	return true;
}

void WNetLink::DeleteIfbDevice(std::string const& IfbName) const
{
	auto IfIndex = rtnl_link_name2i(Cache, IfbName.c_str());
	if (IfIndex <= 0)
	{
		spdlog::info("IFB device '{}' does not exist, nothing to delete", IfbName);
		return;
	}
	auto* Link = rtnl_link_get(Cache, IfIndex);
	if (!Link)
	{
		spdlog::error("Failed to get rtnl_link for IFB device '{}' during deletion", IfbName);
		return;
	}
	int Err = rtnl_link_delete(Socket.Get(), Link);
	rtnl_link_put(Link);
	if (Err < 0)
	{
		spdlog::error("Failed to delete IFB device '{}': {}", IfbName, nl_geterror(Err));
	}
}
