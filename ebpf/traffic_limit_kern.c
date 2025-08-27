// SPDX-License-Identifier: GPL-2.0
// Simple XDP rate limiter using a token bucket shared per-interface.
// Drops packets when tokens are depleted; tokens refill at RATE_BPS.

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#ifndef RATE_BPS
	#define RATE_BPS 1000000ULL // default: 1 Mbps
#endif

#ifndef BURST_BYTES
	#define BURST_BYTES 65536ULL // default burst size: 64 KiB
#endif

struct bucket
{
	__u64 tokens;  // available bytes
	__u64 last_ns; // last refill time
};

struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct bucket);
} rl SEC(".maps");

static __always_inline void refill(struct bucket* b, __u64 now)
{
	if (!b->last_ns)
	{
		b->last_ns = now;
		b->tokens = BURST_BYTES; // start full
		return;
	}
	if ((__s64)b->last_ns >= now)
		return;
	__u64 elapsed = now - b->last_ns;
	// tokens += RATE_BPS * elapsed_ns / 1e9; clamp to BURST_BYTES
	__u64 add = (RATE_BPS * elapsed) / 1000000000ULL;
	__u64 new_tokens = b->tokens + add;
	if (new_tokens > BURST_BYTES)
		new_tokens = BURST_BYTES;
	b->tokens = new_tokens;
	b->last_ns = now;
}

SEC("xdp")
int xdp_rate_limit(struct xdp_md* ctx)
{
	__u32          key = 0;
	struct bucket* b = bpf_map_lookup_elem(&rl, &key);
	if (!b)
		return XDP_PASS;

	__u64 now = bpf_ktime_get_ns();
	refill(b, now);

	void* data = (void*)(long)ctx->data;
	void* data_end = (void*)(long)ctx->data_end;
	__u64 len = (__u64)((unsigned long)data_end - (unsigned long)data);
	if (b->tokens >= len)
	{
		b->tokens -= len;
		return XDP_PASS;
	}
	return XDP_DROP;
}

char LICENSE[] SEC("license") = "GPL";
