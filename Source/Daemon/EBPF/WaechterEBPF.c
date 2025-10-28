#include "EBPFInternal.h"
#define EBPF_COMMON
#include "EBPFCommon.h"

// stores socket identity (PID/TGID + cgroup) per socket cookie
struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, PACKET_RING_SIZE);
} socket_event_ring SEC(".maps");

static __always_inline struct WSocketEvent* MakeSocketEvent(__u64 Cookie, __u8 EventType)
{
	if (Cookie == 0)
	{
		// No point in processing this as we want to map cookie <-> pid/tgid
		return NULL;
	}

	struct WSocketEvent* SocketEvent = (struct WSocketEvent*)bpf_ringbuf_reserve(&socket_event_ring, sizeof(struct WSocketEvent), 0);
	if (!SocketEvent)
	{
		bpf_printk("push_socket_event: reserve NULL cookie=%llu event=%u\n", Cookie, EventType);
		return NULL;
	}
	__builtin_memset(&SocketEvent->Data, 0, sizeof(struct WSocketEventData));

	SocketEvent->CgroupId = bpf_get_current_cgroup_id();
	SocketEvent->PidTgId = bpf_get_current_pid_tgid();
	SocketEvent->Cookie = Cookie;
	SocketEvent->EventType = EventType;

	bpf_printk("PushSocketEvent: Cookie=%llu Event=%u PidTgId=%llu cgroup=%llu\n",
		SocketEvent->Cookie, SocketEvent->EventType, SocketEvent->PidTgId, SocketEvent->CgroupId);

	return SocketEvent;
}

SEC("cgroup/sock_create")
int on_sock_create(struct bpf_sock* Socket)
{
	__u64 Cookie = bpf_get_socket_cookie(Socket);
	bpf_printk("OnSocketCreate: Cookie=%llu\n", Cookie);

	__u64 PidTgid = bpf_get_current_pid_tgid();
	__u32 Tgid = (__u32)(PidTgid >> 32);
	if (Tgid == 0)
	{
		return WCG_ALLOW;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketCreate);
	if (SocketEvent)
	{
		bpf_ringbuf_submit(SocketEvent, 0);
	}
	return WCG_ALLOW;
}

SEC("cgroup/connect4")
int on_connect4(struct bpf_sock_addr* Ctx)
{
	__u64 Cookie = bpf_get_socket_cookie(Ctx);
	bpf_printk("OnConnect4: Cookie=%llu\n", Cookie);

	__u64 PidTgid = bpf_get_current_pid_tgid();
	__u32 Tgid = (__u32)(PidTgid >> 32);
	if (Tgid == 0)
	{
		return WCG_ALLOW;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketConnect_4);

	if (SocketEvent)
	{
		SocketEvent->Data.ConnectEventData.UserFamily = Ctx->user_family;
		SocketEvent->Data.ConnectEventData.Protocol = Ctx->protocol;
		SocketEvent->Data.ConnectEventData.UserPort = bpf_ntohs(Ctx->user_port);

		SocketEvent->Data.ConnectEventData.Addr4 = Ctx->user_ip4;
		bpf_ringbuf_submit(SocketEvent, 0);
	}
	return WCG_ALLOW;
}

SEC("cgroup/connect6")
int on_connect6(struct bpf_sock_addr* Ctx)
{
	__u64 Cookie = bpf_get_socket_cookie(Ctx);
	bpf_printk("on_connect6: cookie=%llu\n", Cookie);

	__u64 PidTgid = bpf_get_current_pid_tgid();
	__u32 Tgid = (__u32)(PidTgid >> 32);
	if (Tgid == 0)
	{
		return WCG_ALLOW;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketConnect_6);
	if (SocketEvent)
	{
		SocketEvent->Data.ConnectEventData.UserFamily = Ctx->user_family;
		SocketEvent->Data.ConnectEventData.Protocol = Ctx->protocol;
		SocketEvent->Data.ConnectEventData.UserPort = bpf_ntohs(Ctx->user_port);
		SocketEvent->Data.ConnectEventData.Addr6[0] = Ctx->user_ip6[0];
		SocketEvent->Data.ConnectEventData.Addr6[1] = Ctx->user_ip6[1];
		SocketEvent->Data.ConnectEventData.Addr6[2] = Ctx->user_ip6[2];
		SocketEvent->Data.ConnectEventData.Addr6[3] = Ctx->user_ip6[3];

		bpf_ringbuf_submit(SocketEvent, 0);
	}
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
	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketAccept);
	if (SocketEvent)
	{
		bpf_ringbuf_submit(SocketEvent, 0);
	}
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
	__u64 Cookie = bpf_get_socket_cookie(Skb);

	bpf_printk("cgroup_skb/egress: cookie=%llu\n", Cookie);
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

// cgroup_skb ingress: capture incoming packet information
SEC("cgroup_skb/ingress")
int cgskb_ingress(struct __sk_buff* Skb)
{
	__u64 Cookie = bpf_get_socket_cookie(Skb);
	bpf_printk("cgroup_skb/ingress: cookie=%llu\n", Cookie);
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

char LICENSE[] SEC("license") = "Dual BSD/GPL";
