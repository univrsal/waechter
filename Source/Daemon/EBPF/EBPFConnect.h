//
// Created by usr on 08/11/2025.
//

// Tracks sockets connecting

#include "EBPFInternal.h"

SEC("fentry/tcp_set_state")
int BPF_PROG(on_tcp_set_state, struct sock* Sk, int Newstate)
{
	// TODO: Do we need this if we have connect4/6 hooks?
	if (Newstate != TCP_CLOSE)
		return 0;

	struct WSocketEvent* Event = MakeSocketEvent(bpf_get_socket_cookie(Sk), NE_SocketClosed);

	if (Event)
	{
		bpf_ringbuf_submit(Event, 0);
	}
	return 0;
}

SEC("cgroup/connect4")
int on_connect4(struct bpf_sock_addr* Ctx)
{
	__u64 Cookie = bpf_get_socket_cookie(Ctx);
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
