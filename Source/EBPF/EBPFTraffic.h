/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
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

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_Traffic);

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

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_Traffic);
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
	void* data = (void*)(long)skb->data;
	void* data_end = (void*)(long)skb->data_end;

	struct iphdr* ip = NULL;
	__u16         dst_port = 0;
	__u8          ip_proto;
	__u32         ip_hdr_len;

	// First, check if data starts with a valid IP header directly
	// This handles raw IP packets from tunnels/ifb devices
	__u8* first_byte = data;
	if ((void*)(first_byte + 1) > data_end)
		return TC_ACT_OK;
	u8 version = (*first_byte) >> 4;

	if (version == 4)
	{
		// Starts with IPv4 header directly (raw IP packet)
		ip = data;
	}
	else if (version == 6)
	{
		// IPv6 - skip for now
		return TC_ACT_OK;
	}
	else
	{
		// Likely has Ethernet header, check for IPv4
		struct ethhdr* eth = data;
		if ((void*)(eth + 1) > data_end)
			return TC_ACT_OK;
		if (eth->h_proto != bpf_htons(ETH_P_IP))
			return TC_ACT_OK;
		ip = (void*)(eth + 1);
	}

	// Validate IP header bounds
	if ((void*)(ip + 1) > data_end)
		return TC_ACT_OK;

	// Double-check it's IPv4
	if (ip->version != 4)
		return TC_ACT_OK;

	ip_proto = ip->protocol;
	ip_hdr_len = ip->ihl * 4;

	// Bounds check for variable IP header length
	if (ip_hdr_len < 20 || ip_hdr_len > 60)
		return TC_ACT_OK;

	// Ensure we can access transport header
	void* transport_hdr = (void*)ip + ip_hdr_len;

	if (ip_proto == IPPROTO_TCP)
	{
		struct tcphdr* tcp = transport_hdr;
		if ((void*)(tcp + 1) > data_end)
			return TC_ACT_OK;
		dst_port = bpf_ntohs(tcp->dest);
	}
	else if (ip_proto == IPPROTO_UDP)
	{
		struct udphdr* udp = transport_hdr;
		if ((void*)(udp + 1) > data_end)
			return TC_ACT_OK;
		dst_port = bpf_ntohs(udp->dest);
	}
	else
	{
		return TC_ACT_OK;
	}

	__u16* Mark = bpf_map_lookup_elem(&ingress_port_marks, &dst_port);

	if (Mark)
	{
		bpf_printk("dst_port=%u mark=%i", dst_port, *Mark);
		skb->mark = *Mark;
	}

	return TC_ACT_OK;
}

SEC("tcx/ingress")
int cls_ingress(struct __sk_buff* ctx)
{
	return TC_ACT_OK;
}
