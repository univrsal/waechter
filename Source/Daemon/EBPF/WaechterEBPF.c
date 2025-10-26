// If CO-RE/BTF is available, prefer vmlinux.h over UAPI headers to avoid redefinitions.
#if __has_include("vmlinux.h")
	#include "vmlinux.h"
	#include <bpf/bpf_helpers.h>
	#include <bpf/bpf_tracing.h>
	#include <bpf/bpf_core_read.h>
	#include <bpf/bpf_endian.h>
	#define HAVE_VMLINUX 1
#else
	#include <linux/types.h>
	#include <linux/bpf.h>
	#include <bpf/bpf_helpers.h>
	#include <bpf/bpf_endian.h>
#endif

#include "EBPFCommon.h"

#ifndef PACKET_RING_SIZE
	// Default ring buffer capacity in bytes (1 MiB). Can be overridden via compile-time define.
	#define PACKET_RING_SIZE (1 << 20)
#endif

// Fallbacks for return codes when linux/bpf.h is not included
#ifndef SK_PASS
	#define SK_PASS 1
#endif
#ifndef XDP_PASS
	#define XDP_PASS 2
#endif

struct WPacketData
{
	__u8  RawData[PACKET_HEADER_SIZE];
	__u64 Cookie;
	__u64 PidTgId;
	__u64 CgroupId;
	__u64 Bytes;
	__u64 Timestamp; // kernel timestamp (ns) to help detect drops or ordering in userspace
	__u8  Direction;
};

struct WSocketEvent
{
	__u8  EventType; // enum ENetEventType
	__u64 Cookie;
	__u64 PidTgId;
	__u64 CgroupId;
};

// stores socket identity (PID/TGID + cgroup) per socket cookie
struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, PACKET_RING_SIZE);
} socket_event_ring SEC(".maps");

// stores raw packet_data entries for userspace to consume.
struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, PACKET_RING_SIZE);
} packet_ring SEC(".maps");

static __always_inline void PushSocketEvent(__u64 Cookie, __u8 EventType)
{
	if (Cookie == 0)
	{
		// No point in processing this as we want to map cookie <-> pid/tgid
		return;
	}

	struct WSocketEvent* SocketEvent = (struct WSocketEvent*)bpf_ringbuf_reserve(&socket_event_ring, sizeof(struct WSocketEvent), 0);
	if (!SocketEvent)
	{
		bpf_printk("push_socket_event: reserve NULL cookie=%llu event=%u\n", Cookie, EventType);
		return;
	}

	SocketEvent->CgroupId = bpf_get_current_cgroup_id();
	SocketEvent->PidTgId = bpf_get_current_pid_tgid();
	SocketEvent->Cookie = Cookie;
	SocketEvent->EventType = EventType;

	bpf_printk("PushSocketEvent: Cookie=%llu Event=%u PidTgId=%llu cgroup=%llu\n",
		SocketEvent->Cookie, SocketEvent->EventType, SocketEvent->PidTgId, SocketEvent->CgroupId);

	bpf_ringbuf_submit(SocketEvent, 0);
}

SEC("cgroup/sock_create")
int on_sock_create(struct bpf_sock* Socket)
{
	__u64 Cookie = bpf_get_socket_cookie(Socket);
	bpf_printk("OnSocketCreate: Cookie=%llu\n", Cookie);
	PushSocketEvent(Cookie, NE_SocketCreate);
	return WCG_ALLOW;
}

SEC("cgroup/connect4")
int on_connect4(struct bpf_sock_addr* Ctx)
{
	__u64 Cookie = bpf_get_socket_cookie(Ctx);
	bpf_printk("OnConnect4: Cookie=%llu\n", Cookie);
	PushSocketEvent(Cookie, NE_SocketConnect_4);
	return WCG_ALLOW;
}

SEC("cgroup/connect6")
int on_connect6(struct bpf_sock_addr* Ctx)
{
	__u64 Cookie = bpf_get_socket_cookie(Ctx);
	bpf_printk("on_connect6: cookie=%llu\n", Cookie);
	PushSocketEvent(Cookie, NE_SocketConnect_6);
	return WCG_ALLOW;
}

#ifdef HAVE_VMLINUX
SEC("lsm/socket_accept")
int BPF_PROG(socket_accept, struct socket* Sock, struct socket* NewSock)
{
	struct sock* Socket = NewSock->sk; // preserve trusted pointer type
	if (!Socket)
	{
		bpf_printk("SocketAccept: Invalid new socket\n");
		return WLSM_ALLOW;
	}

	__u64 Cookie = bpf_get_socket_cookie(Socket);
	bpf_printk("SocketAccept: Cookie=%llu\n", Cookie);
	PushSocketEvent(Cookie, NE_SocketAccept);
	return WLSM_ALLOW;
}
#else
	#error "LSM socket_accept hook requires CO-RE support with vmlinux.h"
#endif

SEC("xdp")
int xdp_waechter(struct xdp_md* Ctx)
{
	WUNUSED(Ctx);
	return XDP_PASS;
}

// cgroup_skb egress: capture outgoing packet information
SEC("cgroup_skb/egress")
int cgskb_egress(struct __sk_buff* Skb)
{
	struct WPacketData* PacketData = (struct WPacketData*)bpf_ringbuf_reserve(&packet_ring, sizeof(struct WPacketData), 0);

	if (PacketData)
	{
		__builtin_memset(PacketData, 0, sizeof(*PacketData));

		PacketData->Cookie = bpf_get_socket_cookie(Skb);
		PacketData->PidTgId = bpf_get_current_pid_tgid();
		PacketData->CgroupId = bpf_get_current_cgroup_id();
		PacketData->Direction = PD_Outgoing;
		PacketData->Bytes = (__u64)Skb->len;
		PacketData->Timestamp = bpf_ktime_get_ns();

		__u32 Slen = Skb->len;
		__u32 Len = PACKET_HEADER_SIZE;

		if (Slen < PACKET_HEADER_SIZE)
		{
			Len = Slen;
		}

		if (Len > 0)
		{
			bpf_skb_load_bytes(Skb, 0, PacketData->RawData, Len);
		}

		bpf_ringbuf_submit(PacketData, 0);
	}

	return SK_PASS;
}

// cgroup_skb ingress: capture incoming packet information
SEC("cgroup_skb/ingress")
int cgskb_ingress(struct __sk_buff* Skb)
{
	struct WPacketData* PacketData = (struct WPacketData*)bpf_ringbuf_reserve(&packet_ring, sizeof(struct WPacketData), 0);

	if (PacketData)
	{
		__builtin_memset(PacketData, 0, sizeof(*PacketData));

		PacketData->Cookie = bpf_get_socket_cookie(Skb);
		PacketData->PidTgId = bpf_get_current_pid_tgid();
		PacketData->CgroupId = bpf_get_current_cgroup_id();
		PacketData->Direction = PD_Incoming;
		PacketData->Bytes = (__u64)Skb->len;
		PacketData->Timestamp = bpf_ktime_get_ns();

		__u32 Slen = Skb->len;
		__u32 Len = PACKET_HEADER_SIZE;

		if (Slen < PACKET_HEADER_SIZE)
		{
			Len = Slen;
		}

		if (Len > 0)
		{
			bpf_skb_load_bytes(Skb, 0, PacketData->RawData, Len);
		}

		// Submit the filled entry to the ring buffer. If the ring is full, reserve would have returned NULL.
		bpf_ringbuf_submit(PacketData, 0);
	}

	return SK_PASS;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
