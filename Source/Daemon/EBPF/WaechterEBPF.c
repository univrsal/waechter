#include "EBPFInternal.h"

#include "EBPFTraffic.h"
#include "EBPFConnect.h"
#include "EBPFAccept.h"
#include "EBPFLifetime.h"

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
