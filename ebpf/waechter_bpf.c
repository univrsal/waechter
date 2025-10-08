// SPDX-License-Identifier: GPL-2.0

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/in6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <stdbool.h>

struct flow_stats
{
	__u64 bytes;
	__u64 packets;
};

struct proc_meta
{
	__u64 pid_tgid;  // upper 32 bits: tgid (PID), lower 32 bits: tid
	__u64 cgroup_id; // cgroup v2 id
	char  comm[16];
};

// Global stats (index 0)
struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct flow_stats);
} global_stats SEC(".maps");

// Per-PID stats
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 16384);
	__type(key, __u32); // PID (tgid)
	__type(value, struct flow_stats);
} pid_stats SEC(".maps");

// Per-cgroup stats
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 16384);
	__type(key, __u64); // cgroup v2 id
	__type(value, struct flow_stats);
} cgroup_stats SEC(".maps");

// Per-connection stats (by socket cookie)
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 65536);
	__type(key, __u64); // socket cookie
	__type(value, struct flow_stats);
} conn_stats SEC(".maps");

// Cookie -> process metadata mapping, filled by cgroup_skb/egress (and optionally other hooks)
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 131072);
	__type(key, __u64); // socket cookie
	__type(value, struct proc_meta);
} cookie_proc_map SEC(".maps");

// Rate limit configuration and state
struct rl_conf
{
	__u64 rate_bps;
	__u64 burst_bytes;
};
struct rl_bucket
{
	__u64 tokens;
	__u64 last_ns;
};

// Global RL config/state (index 0)
struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct rl_conf);
} rl_conf_global SEC(".maps");
struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct rl_bucket);
} rl_bucket_global SEC(".maps");

// Per-PID RL config/state
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 16384);
	__type(key, __u32);
	__type(value, struct rl_conf);
} rl_conf_pid SEC(".maps");
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 16384);
	__type(key, __u32);
	__type(value, struct rl_bucket);
} rl_bucket_pid SEC(".maps");

// Per-cgroup RL config/state
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 16384);
	__type(key, __u64);
	__type(value, struct rl_conf);
} rl_conf_cgroup SEC(".maps");
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 16384);
	__type(key, __u64);
	__type(value, struct rl_bucket);
} rl_bucket_cgroup SEC(".maps");

// Per-connection RL config/state (by cookie)
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 65536);
	__type(key, __u64);
	__type(value, struct rl_conf);
} rl_conf_conn SEC(".maps");
struct
{
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 65536);
	__type(key, __u64);
	__type(value, struct rl_bucket);
} rl_bucket_conn SEC(".maps");

static __always_inline void add_stats(struct flow_stats* s, __u64 bytes)
{
	if (!s)
		return;
	__sync_fetch_and_add(&s->bytes, bytes);
	__sync_fetch_and_add(&s->packets, 1);
}

// Helper to attribute packet to socket via sk_lookup and then to PID/cgroup via cookie map
static __always_inline void attribute_and_count(struct xdp_md* ctx, __u64 bytes)
{
	// XDP cannot safely/portably use cookie helpers; count global only here.
	__u32              gk = 0;
	struct flow_stats* gs = bpf_map_lookup_elem(&global_stats, &gk);
	add_stats(gs, bytes);
}

static __always_inline int consume_tokens(struct rl_bucket* b, const struct rl_conf* c, __u64 now, __u64 bytes)
{
	if (!b || !c || c->rate_bps == 0 || c->burst_bytes == 0)
		return 1; // no limit configured
	if (!b->last_ns)
	{
		b->last_ns = now;
		b->tokens = c->burst_bytes;
	}
	// Refill
	__u64 elapsed = now - b->last_ns;
	if ((__s64)elapsed > 0)
	{
		__u64 add = (c->rate_bps * elapsed) / 1000000000ULL;
		__u64 new_tokens = b->tokens + add;
		if (new_tokens > c->burst_bytes)
			new_tokens = c->burst_bytes;
		b->tokens = new_tokens;
		b->last_ns = now;
	}
	if (b->tokens >= bytes)
	{
		b->tokens -= bytes;
		return 1; // allow
	}
	return 0; // throttle
}

// Evaluate rate-limits in order: per-connection, per-PID, per-cgroup, global. If any says drop, drop.
static __always_inline int enforce_limits(__u64 cookie, __u32 pid, __u64 cgid, __u64 bytes)
{
	__u64 now = bpf_ktime_get_ns();

	// Connection
	if (cookie)
	{
		struct rl_conf* cc = bpf_map_lookup_elem(&rl_conf_conn, &cookie);
		if (cc)
		{
			struct rl_bucket* cb = bpf_map_lookup_elem(&rl_bucket_conn, &cookie);
			if (!cb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_conn, &cookie, &init, BPF_NOEXIST);
				cb = bpf_map_lookup_elem(&rl_bucket_conn, &cookie);
			}
			if (cb && !consume_tokens(cb, cc, now, bytes))
				return 0;
		}
	}

	// PID
	if (pid)
	{
		struct rl_conf* pc = bpf_map_lookup_elem(&rl_conf_pid, &pid);
		if (pc)
		{
			struct rl_bucket* pb = bpf_map_lookup_elem(&rl_bucket_pid, &pid);
			if (!pb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_pid, &pid, &init, BPF_NOEXIST);
				pb = bpf_map_lookup_elem(&rl_bucket_pid, &pid);
			}
			if (pb && !consume_tokens(pb, pc, now, bytes))
				return 0;
		}
	}

	// Cgroup
	if (cgid)
	{
		struct rl_conf* gc = bpf_map_lookup_elem(&rl_conf_cgroup, &cgid);
		if (gc)
		{
			struct rl_bucket* gb = bpf_map_lookup_elem(&rl_bucket_cgroup, &cgid);
			if (!gb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_cgroup, &cgid, &init, BPF_NOEXIST);
				gb = bpf_map_lookup_elem(&rl_bucket_cgroup, &cgid);
			}
			if (gb && !consume_tokens(gb, gc, now, bytes))
				return 0;
		}
	}

	// Global
	{
		__u32           gk = 0;
		struct rl_conf* gc = bpf_map_lookup_elem(&rl_conf_global, &gk);
		if (gc)
		{
			struct rl_bucket* gb = bpf_map_lookup_elem(&rl_bucket_global, &gk);
			if (!gb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_global, &gk, &init, BPF_NOEXIST);
				gb = bpf_map_lookup_elem(&rl_bucket_global, &gk);
			}
			if (gb && !consume_tokens(gb, gc, now, bytes))
				return 0;
		}
	}
	return 1;
}

SEC("xdp")
int xdp_waechter(struct xdp_md* ctx)
{
	void* data = (void*)(long)ctx->data;
	void* data_end = (void*)(long)ctx->data_end;
	__u64 bytes = (unsigned long)data_end - (unsigned long)data;

	attribute_and_count(ctx, bytes);

	if (!enforce_limits(0 /*cookie*/, 0 /*pid*/, 0 /*cgid*/, bytes))
		return XDP_DROP;

	return XDP_PASS;
}

// cgroup_skb egress: capture PID and cgroup for socket cookie
SEC("cgroup_skb/egress")
int cgskb_egress(struct __sk_buff* skb)
{
	__u64 cookie = bpf_get_socket_cookie(skb);

	struct proc_meta pm = {};
	pm.pid_tgid = bpf_get_current_pid_tgid();
	pm.cgroup_id = bpf_get_current_cgroup_id();
	bpf_get_current_comm(&pm.comm, sizeof(pm.comm));
	if (cookie)
		bpf_map_update_elem(&cookie_proc_map, &cookie, &pm, BPF_ANY);

	// Account bytes and packets at egress too
	__u64              bytes = (__u64)skb->len;
	__u32              gk = 0;
	struct flow_stats* gs = bpf_map_lookup_elem(&global_stats, &gk);
	add_stats(gs, bytes);
	__u32              pid = pm.pid_tgid >> 32;
	struct flow_stats* ps = bpf_map_lookup_elem(&pid_stats, &pid);
	if (!ps)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&pid_stats, &pid, &zero, BPF_NOEXIST);
		ps = bpf_map_lookup_elem(&pid_stats, &pid);
	}
	if (ps)
		add_stats(ps, bytes);
	struct flow_stats* cgst = bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	if (!cgst)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&cgroup_stats, &pm.cgroup_id, &zero, BPF_NOEXIST);
		cgst = bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	}
	if (cgst)
		add_stats(cgst, bytes);
	if (cookie)
	{
		struct flow_stats* cs = bpf_map_lookup_elem(&conn_stats, &cookie);
		if (!cs)
		{
			struct flow_stats zero = {};
			bpf_map_update_elem(&conn_stats, &cookie, &zero, BPF_NOEXIST);
			cs = bpf_map_lookup_elem(&conn_stats, &cookie);
		}
		if (cs)
			add_stats(cs, bytes);
	}

	// Enforce egress throttles (drop if any limit exceeded)
	if (!enforce_limits(cookie, pid, pm.cgroup_id, bytes))
		return 0; // drop

	return 1; // continue
}

// cgroup_skb ingress: account and optionally enforce for traffic delivered to the process
SEC("cgroup_skb/ingress")
int cgskb_ingress(struct __sk_buff* skb)
{
	__u64            cookie = bpf_get_socket_cookie(skb);
	struct proc_meta pm = {};
	pm.pid_tgid = bpf_get_current_pid_tgid();
	pm.cgroup_id = bpf_get_current_cgroup_id();
	bpf_get_current_comm(&pm.comm, sizeof(pm.comm));

	if (cookie)
		bpf_map_update_elem(&cookie_proc_map, &cookie, &pm, BPF_ANY);

	__u64              bytes = (__u64)skb->len;
	__u32              gk = 0;
	struct flow_stats* gs = bpf_map_lookup_elem(&global_stats, &gk);
	add_stats(gs, bytes);

	__u32              pid = pm.pid_tgid >> 32;
	struct flow_stats* ps = bpf_map_lookup_elem(&pid_stats, &pid);
	if (!ps)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&pid_stats, &pid, &zero, BPF_NOEXIST);
		ps = bpf_map_lookup_elem(&pid_stats, &pid);
	}
	if (ps)
		add_stats(ps, bytes);

	struct flow_stats* cgst = bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	if (!cgst)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&cgroup_stats, &pm.cgroup_id, &zero, BPF_NOEXIST);
		cgst = bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	}
	if (cgst)
		add_stats(cgst, bytes);

	if (cookie)
	{
		struct flow_stats* cs = bpf_map_lookup_elem(&conn_stats, &cookie);
		if (!cs)
		{
			struct flow_stats zero = {};
			bpf_map_update_elem(&conn_stats, &cookie, &zero, BPF_NOEXIST);
			cs = bpf_map_lookup_elem(&conn_stats, &cookie);
		}
		if (cs)
			add_stats(cs, bytes);
	}

	if (!enforce_limits(cookie, pid, pm.cgroup_id, bytes))
		return 0; // drop

	return 1; // continue
}

char LICENSE[] SEC("license") = "GPL";
