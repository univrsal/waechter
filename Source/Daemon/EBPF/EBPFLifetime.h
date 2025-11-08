//
// Created by usr on 08/11/2025.
//

// Tracks socket creation/destruction events.

#pragma once
#include "EBPFInternal.h"

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
	__u64 Key = (__u64)Socket;

	struct WSharedSocketData Data = {};
	Data.Pid = Tgid;
	Data.Cookie = Cookie;
	Data.Family = Socket->family;

	bpf_map_update_elem(&shared_socket_data_map, &Key, &Data, BPF_ANY);

	struct WSocketEvent* SocketEvent = MakeSocketEvent(Cookie, NE_SocketCreate);
	if (SocketEvent)
	{
		SocketEvent->Data.SocketCreateEventData.Protocol = Socket->protocol;

		bpf_ringbuf_submit(SocketEvent, 0);
	}
	return WCG_ALLOW;
}
