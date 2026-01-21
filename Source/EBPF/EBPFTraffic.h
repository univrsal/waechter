/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Ingress/Egress Traffic measuring and shaping
#pragma once
#include "EBPFInternal.h"
#include "EBPFCommon.h"

#ifndef TC_ACT_OK
	#define TC_ACT_OK 0
#endif

// cgroup_skb ingress: capture incoming packet information
SEC("cgroup_skb/ingress")
int cgskb_ingress(struct __sk_buff* Skb)
{
	// Only process IPv4 or IPv6
	__u16 Proto = Skb->protocol;
	if (Proto != bpf_htons(ETH_P_IP) && Proto != bpf_htons(ETH_P_IPV6))
	{
		return SK_PASS;
	}

	__u64 Cookie = bpf_get_socket_cookie(Skb);

	struct WTrafficItemRulesBase* Rules = GetSocketRules(Cookie);
	if (Rules && Rules->DownloadSwitch == SS_Block)
	{
		return SK_DROP;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent2(Cookie, NE_Traffic, false);

	if (SocketEvent)
	{
		__builtin_memset(&SocketEvent->Data.TrafficEventData, 0, sizeof(SocketEvent->Data.TrafficEventData));
		struct WSocketTrafficEventData* TrafficData = &SocketEvent->Data.TrafficEventData;
		TrafficData->Direction = PD_Incoming;
		TrafficData->Bytes = (__u64)Skb->len;
		TrafficData->Timestamp = bpf_ktime_get_ns();

		__u32 Slen = Skb->len;
		__u32 Len = PACKET_HEADER_SIZE;

		if (Slen < PACKET_HEADER_SIZE)
		{
			Len = Slen;
		}

		if (Len > 0)
		{
			bpf_skb_load_bytes(Skb, 0, TrafficData->RawData, Len);
		}

		bpf_ringbuf_submit(SocketEvent, 0);
	}

	return SK_PASS;
}

// cgroup_skb egress: capture outgoing packet information
SEC("cgroup_skb/egress")
int cgskb_egress(struct __sk_buff* Skb)
{
	// Only process IPv4 or IPv6
	__u16 proto = Skb->protocol;
	if (proto != bpf_htons(ETH_P_IP) && proto != bpf_htons(ETH_P_IPV6))
	{
		return SK_PASS;
	}
	__u64 Cookie = bpf_get_socket_cookie(Skb);

	struct WTrafficItemRulesBase* Rules = GetSocketRules(Cookie);
	if (Rules && Rules->UploadSwitch == SS_Block)
	{
		return SK_DROP;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent2(Cookie, NE_Traffic, false);
	if (SocketEvent)
	{
		__builtin_memset(&SocketEvent->Data.TrafficEventData, 0, sizeof(SocketEvent->Data.TrafficEventData));
		struct WSocketTrafficEventData* TrafficData = &SocketEvent->Data.TrafficEventData;
		TrafficData->Direction = PD_Outgoing;
		TrafficData->Bytes = (__u64)Skb->len;
		TrafficData->Timestamp = bpf_ktime_get_ns();

		__u32 Slen = Skb->len;
		__u32 Len = PACKET_HEADER_SIZE;

		if (Slen < PACKET_HEADER_SIZE)
		{
			Len = Slen;
		}

		if (Len > 0)
		{
			bpf_skb_load_bytes(Skb, 0, TrafficData->RawData, Len);
		}

		bpf_ringbuf_submit(SocketEvent, 0);
	}

	return SK_PASS;
}

int const volatile IngressInterfaceId = 0;

SEC("tcx/egress")
int cls_egress(struct __sk_buff* skb)
{
	__u64                         Cookie = bpf_get_socket_cookie(skb);
	struct WTrafficItemRulesBase* Rules = GetSocketRules(Cookie);

	if (Rules && Rules->UploadMark != 0)
	{
		skb->mark = Rules->UploadMark;
	}

	return TC_ACT_OK;
}

#ifndef ETH_P_8021Q
	#define ETH_P_8021Q 0x8100
#endif
#ifndef ETH_P_8021AD
	#define ETH_P_8021AD 0x88A8
#endif
#ifndef IPPROTO_TCP
	#define IPPROTO_TCP 6
#endif
#ifndef IPPROTO_UDP
	#define IPPROTO_UDP 17
#endif

SEC("tcx/egress")
int ifb_cls_egress(struct __sk_buff* skb)
{
	// I'd be lying if I said I know what's going on here
	// but we can't access skb->local_port/skb->remote port
	// so this magic seems to just get the port in a very
	// round-about-way from the packet data itself

	void* Data = (void*)(long)skb->data;
	void* DataEnd = (void*)(long)skb->data_end;

	struct iphdr* IP = NULL;
	__u16         DstPort = 0;
	__u8          IPProto;
	__u32         IPHeaderLen;

	// First, check if data starts with a valid IP header directly
	// This handles raw IP packets from tunnels/ifb devices
	__u8* FirstByte = Data;
	if ((void*)(FirstByte + 1) > DataEnd)
		return TC_ACT_OK;
	u8 Version = (*FirstByte) >> 4;

	if (Version == 4)
	{
		// Starts with IPv4 header directly (raw IP packet)
		IP = Data;
	}
	else if (Version == 6)
	{
		// IPv6 - skip for now
		return TC_ACT_OK;
	}
	else
	{
		// Likely has Ethernet header, check for IPv4
		struct ethhdr* Eth = Data;
		if ((void*)(Eth + 1) > DataEnd)
			return TC_ACT_OK;
		if (Eth->h_proto != bpf_htons(ETH_P_IP))
			return TC_ACT_OK;
		IP = (void*)(Eth + 1);
	}

	// Validate IP header bounds
	if ((void*)(IP + 1) > DataEnd)
		return TC_ACT_OK;

	// Double-check it's IPv4
	if (IP->version != 4)
		return TC_ACT_OK;

	IPProto = IP->protocol;
	IPHeaderLen = IP->ihl * 4;

	// Bounds check for variable IP header length
	if (IPHeaderLen < 20 || IPHeaderLen > 60)
	{
		return TC_ACT_OK;
	}

	// Ensure we can access transport header
	void* Transportheader = (void*)IP + IPHeaderLen;

	if (IPProto == IPPROTO_TCP)
	{
		struct tcphdr* tcp = Transportheader;
		if ((void*)(tcp + 1) > DataEnd)
			return TC_ACT_OK;
		DstPort = bpf_ntohs(tcp->dest);
	}
	else if (IPProto == IPPROTO_UDP)
	{
		struct udphdr* Udp = Transportheader;
		if ((void*)(Udp + 1) > DataEnd)
			return TC_ACT_OK;
		DstPort = bpf_ntohs(Udp->dest);
	}
	else
	{
		return TC_ACT_OK;
	}

	__u16* Mark = bpf_map_lookup_elem(&ingress_port_marks, &DstPort);

	if (Mark)
	{
		skb->mark = *Mark;
	}

	return TC_ACT_OK;
}
