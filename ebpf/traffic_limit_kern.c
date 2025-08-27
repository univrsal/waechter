// SPDX-License-Identifier: GPL-2.0

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("xdp")
int xdp_rate_limit(struct xdp_md* ctx)
{
	return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";
