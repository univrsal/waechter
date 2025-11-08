#include "EBPFInternal.h"

#include "EBPFTraffic.h"
#include "EBPFConnect.h"

SEC("fentry/tcp_set_state")
int BPF_PROG(on_tcp_set_state, struct sock* Sk, int Newstate)
{
	if (Newstate != TCP_CLOSE)
		return 0;

	struct WSocketEvent* Event = MakeSocketEvent(bpf_get_socket_cookie(Sk), NE_SocketClosed);

	if (Event)
	{
		bpf_ringbuf_submit(Event, 0);
	}
	return 0;
}

SEC("fentry/inet_sock_destruct")
int BPF_PROG(on_inet_sock_destruct, struct sock* Sk)
{
	struct WSocketEvent* Event = MakeSocketEvent(bpf_get_socket_cookie(Sk), NE_SocketClosed);

	if (Event)
	{
		bpf_ringbuf_submit(Event, 0);
	}
	return 0;
}

SEC("cgroup/sock_create")
int on_sock_create(struct bpf_sock* Socket)
{
	__u64 Cookie = bpf_get_socket_cookie(Socket);
	__u64 PidTgid = bpf_get_current_pid_tgid();
	__u32 Tgid = (__u32)(PidTgid >> 32);

	if (Socket->type != SOCK_STREAM && Socket->type != SOCK_DGRAM)
	{
		return WCG_ALLOW;
	}

	if (Tgid == 0)
	{
		return WCG_ALLOW;
	}

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketCreate);
	if (SocketEvent)
	{
		SocketEvent->Data.SocketCreateEventData.Protocol = Socket->protocol;

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

	__u64                Cookie = bpf_get_socket_cookie(Socket);
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

// Capture UDP destination for unconnected sockets (sendto/sendmsg)
SEC("cgroup/sendmsg4")
int on_sendmsg4(struct bpf_sock_addr* Ctx)
{
	// // Only userspace-initiated sends (non-kernel TGID)
	// __u64 PidTgid = bpf_get_current_pid_tgid();
	// __u32 Tgid = (__u32)(PidTgid >> 32);
	// if (Tgid == 0)
	// 	return WCG_ALLOW;
	//
	// __u64                Cookie = bpf_get_socket_cookie(Ctx);
	// struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketConnect_4);
	// if (SocketEvent)
	// {
	// 	SocketEvent->Data.ConnectEventData.UserFamily = Ctx->user_family;
	// 	// Protocol is IPPROTO_UDP for this hook
	// 	SocketEvent->Data.ConnectEventData.Protocol = Ctx->protocol;
	// 	SocketEvent->Data.ConnectEventData.UserPort = bpf_ntohs(Ctx->user_port);
	// 	SocketEvent->Data.ConnectEventData.Addr4 = Ctx->user_ip4;
	// 	bpf_ringbuf_submit(SocketEvent, 0);
	// }
	return WCG_ALLOW;
}

SEC("cgroup/sendmsg6")
int on_sendmsg6(struct bpf_sock_addr* Ctx)
{
	// __u64 PidTgid = bpf_get_current_pid_tgid();
	// __u32 Tgid = (__u32)(PidTgid >> 32);
	// if (Tgid == 0)
	// 	return WCG_ALLOW;
	//
	// __u64                Cookie = bpf_get_socket_cookie(Ctx);
	// struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketConnect_6);
	// if (SocketEvent)
	// {
	// 	SocketEvent->Data.ConnectEventData.UserFamily = Ctx->user_family;
	// 	SocketEvent->Data.ConnectEventData.Protocol = Ctx->protocol;
	// 	SocketEvent->Data.ConnectEventData.UserPort = bpf_ntohs(Ctx->user_port);
	// 	SocketEvent->Data.ConnectEventData.Addr6[0] = Ctx->user_ip6[0];
	// 	SocketEvent->Data.ConnectEventData.Addr6[1] = Ctx->user_ip6[1];
	// 	SocketEvent->Data.ConnectEventData.Addr6[2] = Ctx->user_ip6[2];
	// 	SocketEvent->Data.ConnectEventData.Addr6[3] = Ctx->user_ip6[3];
	// 	bpf_ringbuf_submit(SocketEvent, 0);
	// }
	return WCG_ALLOW;
}

char LICENSE[] SEC("license") = "GPL";
