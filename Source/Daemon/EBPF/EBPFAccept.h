//
// Created by usr on 08/11/2025.
//

#pragma once
#include "EBPFInternal.h"

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
