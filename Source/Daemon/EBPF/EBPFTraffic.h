//
// Created by usr on 08/11/2025.
//

// Ingress/Egress Traffic measuring and shaping
#pragma once
#include "EBPFInternal.h"
#include "EBPFCommon.h"

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
