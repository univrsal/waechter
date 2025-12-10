// Created by usr on 09/12/2025.
//

#include "NetLink.hpp"

#include <optional>
#include <spdlog/spdlog.h>
#include <array>
#include <cstring>
#include <linux/if.h>
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/rtnetlink.h>
#include <linux/if_ether.h>
#include <netlink/socket.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/cache.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/class.h>
#include <netlink/route/tc.h>
#include <netlink/route/qdisc/htb.h>
#include <netlink/route/cls/u32.h>

namespace
{

	std::optional<int> GetInterfaceIndex(nl_cache* Cache, std::string const& IfName)
	{
		if (!Cache)
		{
			spdlog::error("Netlink cache not initialized when querying '{}'", IfName);
			return std::nullopt;
		}

		int IfIndex = rtnl_link_name2i(Cache, IfName.c_str());
		if (IfIndex <= 0)
		{
			spdlog::error("Interface '{}' not found in netlink cache", IfName);
			return std::nullopt;
		}
		return IfIndex;
	}

	bool EnsureLinkCache(nl_sock* Sock, nl_cache*& Cache)
	{
		if (Cache)
		{
			return true;
		}

		int Err = rtnl_link_alloc_cache(Sock, AF_UNSPEC, &Cache);
		if (Err < 0)
		{
			spdlog::error("rtnl_link_alloc_cache failed: {}", nl_geterror(Err));
			return false;
		}
		return true;
	}

} // namespace

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

int WNetLink::GetIfIndex(std::string const& IfName)
{
	int Result, Err;

	Err = rtnl_link_alloc_cache(Socket.Get(), AF_UNSPEC, &Cache);
	if (Err < 0)
	{
		fprintf(stderr, "rtnl_link_alloc_cache: %s\n", nl_geterror(Err));
		return -1;
	}

	Result = rtnl_link_name2i(Cache, IfName.c_str());

	if (Result == 0)
	{
		return -1;
	}
	return Result;
}

bool WNetLink::QdiscHtbReplaceRoot(std::string const& IfName, uint16_t Major, uint16_t DefaultMinor)
{
	int IfIndex = GetIfIndex(IfName.c_str());
	if (IfIndex < 0)
	{
		spdlog::error("Interface '{}' not found", IfName);
		return false;
	}

	auto Link = rtnl_link_get(Cache, IfIndex);
	if (!Link)
	{
		spdlog::error("Failed to get link for interface '{}'", IfName);
		return false;
	}

	uint32_t Handle = TC_H_MAKE(Major, 0);
	uint32_t DefCls = TC_H_MAKE(Major, DefaultMinor);

	rtnl_qdisc* Qdisc = rtnl_qdisc_alloc();
	if (!Qdisc)
	{
		return false;
	}

	rtnl_tc_set_link(TC_CAST(Qdisc), Link);
	rtnl_tc_set_parent(TC_CAST(Qdisc), TC_H_ROOT);
	rtnl_tc_set_handle(TC_CAST(Qdisc), Handle);
	rtnl_tc_set_kind(TC_CAST(Qdisc), "htb");

	// Build message so we can add HTB-specific options (default class)
	nl_msg* Msg = nlmsg_alloc();
	if (!Msg)
	{
		rtnl_qdisc_put(Qdisc);
		return false;
	}

	int Err = rtnl_qdisc_build_add_request(Qdisc,
		NLM_F_CREATE | NLM_F_REPLACE, // behaves like "qdisc replace"
		&Msg);
	if (Err < 0)
	{
		spdlog::error("Failed to build qdisc add request for interface '{}'", IfName);
		nlmsg_free(Msg);
		rtnl_qdisc_put(Qdisc);
		return false;
	}

	// HTB "global" options: set default class.
	struct tc_htb_glob HtbGlob{};
	HtbGlob.version = TC_HTB_PROTOVER;
	HtbGlob.rate2quantum = 10;
	HtbGlob.defcls = DefCls;
	HtbGlob.debug = 0;
	HtbGlob.direct_pkts = 0;

	struct nlattr* Opts = nla_nest_start(Msg, TCA_OPTIONS);
	if (!Opts)
	{
		nlmsg_free(Msg);
		rtnl_qdisc_put(Qdisc);
		return false;
	}

	if (nla_put(Msg, TCA_HTB_INIT, sizeof(HtbGlob), &HtbGlob) < 0)
	{
		nlmsg_free(Msg);
		rtnl_qdisc_put(Qdisc);
		return false;
	}

	nla_nest_end(Msg, Opts);

	Err = nl_send_auto(Socket.Get(), Msg);
	nlmsg_free(Msg);
	rtnl_qdisc_put(Qdisc);
	if (Err < 0)
	{
		spdlog::error("Failed to send qdisc add request for interface '{}': {}", IfName, nl_geterror(Err));
		return false;
	}

	Err = nl_wait_for_ack(Socket.Get());
	if (Err < 0)
	{
		spdlog::error("Failed to receive ack for qdisc add request for interface '{}': {}", IfName, nl_geterror(Err));
		return false;
	}

	return true;
}

bool WNetLink::ClassHtbReplace(std::string const& IfName, uint16_t ParentMajor, uint16_t ParentMinor,
	uint16_t ClassMajor, uint16_t ClassMinor, uint64_t RateBitsPerSec, uint64_t CeilBitsPerSec)
{
	if (!EnsureLinkCache(Socket.Get(), Cache))
	{
		return false;
	}

	auto IfIndexOpt = GetInterfaceIndex(Cache, IfName);
	if (!IfIndexOpt)
	{
		return false;
	}

	rtnl_link* Link = rtnl_link_get(Cache, *IfIndexOpt);
	if (!Link)
	{
		spdlog::error("Failed to get link for interface '{}'", IfName);
		return false;
	}

	rtnl_class* Class = rtnl_class_alloc();
	if (!Class)
	{
		spdlog::error("Failed to allocate rtnl_class for '{}'", IfName);
		rtnl_link_put(Link);
		return false;
	}

	rtnl_tc_set_link(TC_CAST(Class), Link);
	rtnl_tc_set_parent(TC_CAST(Class), TC_H_MAKE(ParentMajor, ParentMinor));
	rtnl_tc_set_handle(TC_CAST(Class), TC_H_MAKE(ClassMajor, ClassMinor));
	rtnl_tc_set_kind(TC_CAST(Class), "htb");
	rtnl_htb_set_rate64(Class, RateBitsPerSec);
	rtnl_htb_set_ceil64(Class, CeilBitsPerSec);

	int Err = rtnl_class_add(Socket.Get(), Class, NLM_F_CREATE | NLM_F_REPLACE);
	rtnl_class_put(Class);
	rtnl_link_put(Link);
	if (Err < 0)
	{
		spdlog::error("Failed to add/replace HTB class on '{}' (1:{}): {}", IfName, ClassMinor, nl_geterror(Err));
		return false;
	}

	return true;
}

bool WNetLink::FilterMatchAllToClassReplace(std::string const& IfName, uint16_t ParentMajor, uint16_t ParentMinor,
	uint16_t Priority, uint16_t FlowClassMajor, uint16_t FlowClassMinor)
{
	if (!EnsureLinkCache(Socket.Get(), Cache))
	{
		return false;
	}

	auto IfIndexOpt = GetInterfaceIndex(Cache, IfName);
	if (!IfIndexOpt)
	{
		return false;
	}

	auto* Link = rtnl_link_get(Cache, *IfIndexOpt);
	if (!Link)
	{
		spdlog::error("Failed to get link for interface '{}'", IfName);
		return false;
	}

	rtnl_cls* Filter = rtnl_cls_alloc();
	if (!Filter)
	{
		rtnl_link_put(Link);
		return false;
	}

	rtnl_tc_set_link(TC_CAST(Filter), Link);
	rtnl_tc_set_parent(TC_CAST(Filter), TC_H_MAKE(ParentMajor, ParentMinor));
	rtnl_tc_set_kind(TC_CAST(Filter), "u32");
	rtnl_tc_set_handle(TC_CAST(Filter), TC_H_MAKE(Priority, 0));

	nl_msg* Msg = nlmsg_alloc();
	if (!Msg)
	{
		rtnl_cls_put(Filter);
		rtnl_link_put(Link);
		return false;
	}

	int Err = rtnl_cls_build_add_request(Filter, NLM_F_CREATE | NLM_F_REPLACE, &Msg);
	if (Err < 0)
	{
		spdlog::error("Failed to build filter request on '{}': {}", IfName, nl_geterror(Err));
		nlmsg_free(Msg);
		rtnl_cls_put(Filter);
		rtnl_link_put(Link);
		return false;
	}

	auto* Tcm = reinterpret_cast<tcmsg*>(NLMSG_DATA(nlmsg_hdr(Msg)));
	Tcm->tcm_info = TC_H_MAKE(Priority, ETH_P_ALL);
	Tcm->tcm_parent = TC_H_MAKE(ParentMajor, ParentMinor);
	Tcm->tcm_handle = TC_H_MAKE(Priority, 0);

	auto* Opts = nla_nest_start(Msg, TCA_OPTIONS);
	if (!Opts)
	{
		nlmsg_free(Msg);
		rtnl_cls_put(Filter);
		rtnl_link_put(Link);
		return false;
	}

	auto NewHandle = TC_H_MAKE(FlowClassMajor, FlowClassMinor);

	std::array<std::byte, sizeof(tc_u32_sel) + sizeof(tc_u32_key)> Selector{};
	auto*                                                          Sel = reinterpret_cast<tc_u32_sel*>(Selector.data());
	auto* Key = reinterpret_cast<tc_u32_key*>(Selector.data() + sizeof(tc_u32_sel));

	Sel->hmask = 0;
	Sel->nkeys = 1;
	Sel->flags = TC_U32_TERMINAL;
	Sel->offmask = 0;
	Sel->off = 0;
	Sel->hoff = 0;
	Sel->offshift = 0;

	Key->mask = 0;
	Key->val = 0;
	Key->off = 0;
	Key->offmask = 0;

	if (nla_put(Msg, TCA_U32_SEL, Selector.size(), Sel) < 0 || nla_put_u32(Msg, TCA_U32_CLASSID, NewHandle) < 0)
	{
		spdlog::error("Failed to encode u32 selector for '{}': flow class 1:{}", IfName, FlowClassMinor);
		nlmsg_free(Msg);
		rtnl_cls_put(Filter);
		rtnl_link_put(Link);
		return false;
	}

	nla_nest_end(Msg, Opts);

	Err = nl_send_auto(Socket.Get(), Msg);
	nlmsg_free(Msg);
	rtnl_cls_put(Filter);
	rtnl_link_put(Link);
	if (Err < 0)
	{
		spdlog::error("Failed to send filter add/replace request on '{}': {}", IfName, nl_geterror(Err));
		return false;
	}

	Err = nl_wait_for_ack(Socket.Get());
	if (Err < 0)
	{
		spdlog::error("Failed to receive ack for filter add/replace request on '{}': {}", IfName, nl_geterror(Err));
		return false;
	}

	return true;
}
