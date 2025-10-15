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
#include <linux/socket.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <stdbool.h>

// Define AF_* if not provided by uapi headers in BPF target
#ifndef AF_INET
#define AF_INET 2
#endif
#ifndef AF_INET6
#define AF_INET6 10
#endif

// Minimal VLAN header type to avoid incomplete type issues in XDP parsing
struct waechter_vlan_hdr { __u16 tci; __u16 proto; };

// Ring buffers for streaming telemetry to user space
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1 << 20); // 1 MiB for IP events
} ip_events SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1 << 20); // 1 MiB for accounting events
} acct_events SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1 << 18); // 256 KiB for rate-limit drops
} rl_events SEC(".maps");

// Ring buffer for debug events
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 16); // 64 KiB for debug events
} debug_events SEC(".maps");

// Event payloads
struct ip_event {
	__u64 cookie;      // socket cookie if known (0 for XDP path)
	__u64 pid_tgid;    // only for cgroup hooks
	__u64 cgroup_id;   // only for cgroup hooks
	__u8  direction;   // 1=egress, 2=ingress, 3=xdp
	__u8  family;      // AF_INET/AF_INET6
	__u8  l4proto;     // IPPROTO_*
	__u8  pad;
	union {
		struct {
			__u32 saddr;
			__u32 daddr;
			__u16 sport;
			__u16 dport;
		} v4;
		struct {
			__u8  saddr[16];
			__u8  daddr[16];
			__u16 sport;
			__u16 dport;
		} v6;
	};
};

struct acct_event {
	__u64 cookie;
	__u32 pid;
	__u64 cgroup_id;
	__u8  direction; // 1=egress,2=ingress,3=xdp
	__u8  _pad[3];
	__u64 bytes;
	__u64 packets;
};

struct rl_event {
	__u64 cookie;
	__u32 pid;
	__u64 cgroup_id;
	__u64 bytes;
	__u32 scope_mask; // 1=conn,2=pid,4=cgroup,8=global
	__u64 now_ns;
};

struct debug_event {
    __u8 data[32];
    __u32 pkt_len;
    __u64 cookie;
    __u8 direction;
};

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

static __always_inline void emit_acct_event(__u64 cookie, __u32 pid, __u64 cgid, __u8 dir, __u64 bytes, __u64 packets)
{
	struct acct_event* ev = (struct acct_event*)bpf_ringbuf_reserve(&acct_events, sizeof(struct acct_event), 0);
	if (!ev)
		return;
	ev->cookie    = cookie;
	ev->pid       = pid;
	ev->cgroup_id = cgid;
	ev->direction = dir;
	ev->bytes     = bytes;
	ev->packets   = packets;
	bpf_ringbuf_submit(ev, 0);
}

// Helper to attribute packet to socket via sk_lookup and then to PID/cgroup via cookie map
static __always_inline void attribute_and_count(struct xdp_md* ctx, __u64 bytes)
{
	// XDP cannot safely/portably use cookie helpers; count global only here.
	__u32              gk = 0;
	struct flow_stats* gs = (struct flow_stats*)bpf_map_lookup_elem(&global_stats, &gk);
	add_stats(gs, bytes);
	// Stream telemetry for XDP path
	emit_acct_event(0 /*cookie*/, 0 /*pid*/, 0 /*cgid*/, 3 /*xdp*/, bytes, 1);
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

static __always_inline void emit_rl_drop(__u64 cookie, __u32 pid, __u64 cgid, __u64 bytes, __u32 scope_mask, __u64 now)
{
	struct rl_event* ev = (struct rl_event*)bpf_ringbuf_reserve(&rl_events, sizeof(struct rl_event), 0);
	if (!ev)
		return;
	ev->cookie     = cookie;
	ev->pid        = pid;
	ev->cgroup_id  = cgid;
	ev->bytes      = bytes;
	ev->scope_mask = scope_mask;
	ev->now_ns     = now;
	bpf_ringbuf_submit(ev, 0);
}

// Evaluate rate-limits in order: per-connection, per-PID, per-cgroup, global. If any says drop, drop.
static __always_inline int enforce_limits(__u64 cookie, __u32 pid, __u64 cgid, __u64 bytes, __u32* scope_out)
{
	__u64 now = bpf_ktime_get_ns();
	__u32 scope = 0;

	// Connection
	if (cookie)
	{
		struct rl_conf* cc = (struct rl_conf*)bpf_map_lookup_elem(&rl_conf_conn, &cookie);
		if (cc)
		{
			struct rl_bucket* cb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_conn, &cookie);
			if (!cb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_conn, &cookie, &init, BPF_NOEXIST);
				cb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_conn, &cookie);
			}
			if (cb && !consume_tokens(cb, cc, now, bytes)) {
				scope |= 1; // conn
				if (scope_out) *scope_out = scope;
				emit_rl_drop(cookie, pid, cgid, bytes, scope, now);
				return 0;
			}
		}
	}

	// PID
	if (pid)
	{
		struct rl_conf* pc = (struct rl_conf*)bpf_map_lookup_elem(&rl_conf_pid, &pid);
		if (pc)
		{
			struct rl_bucket* pb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_pid, &pid);
			if (!pb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_pid, &pid, &init, BPF_NOEXIST);
				pb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_pid, &pid);
			}
			if (pb && !consume_tokens(pb, pc, now, bytes)) {
				scope |= 2; // pid
				if (scope_out) *scope_out = scope;
				emit_rl_drop(cookie, pid, cgid, bytes, scope, now);
				return 0;
			}
		}
	}

	// Cgroup
	if (cgid)
	{
		struct rl_conf* gc = (struct rl_conf*)bpf_map_lookup_elem(&rl_conf_cgroup, &cgid);
		if (gc)
		{
			struct rl_bucket* gb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_cgroup, &cgid);
			if (!gb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_cgroup, &cgid, &init, BPF_NOEXIST);
				gb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_cgroup, &cgid);
			}
			if (gb && !consume_tokens(gb, gc, now, bytes)) {
				scope |= 4; // cgroup
				if (scope_out) *scope_out = scope;
				emit_rl_drop(cookie, pid, cgid, bytes, scope, now);
				return 0;
			}
		}
	}

	// Global
	{
		__u32           gk = 0;
		struct rl_conf* gc = (struct rl_conf*)bpf_map_lookup_elem(&rl_conf_global, &gk);
		if (gc)
		{
			struct rl_bucket* gb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_global, &gk);
			if (!gb)
			{
				struct rl_bucket init = {};
				bpf_map_update_elem(&rl_bucket_global, &gk, &init, BPF_NOEXIST);
				gb = (struct rl_bucket*)bpf_map_lookup_elem(&rl_bucket_global, &gk);
			}
			if (gb && !consume_tokens(gb, gc, now, bytes)) {
				scope |= 8; // global
				if (scope_out) *scope_out = scope;
				emit_rl_drop(cookie, pid, cgid, bytes, scope, now);
				return 0;
			}
		}
	}
	if (scope_out) *scope_out = 0;
	return 1;
}

// --- IP event publishing helpers ---
static __always_inline void emit_debug_event_skb(struct __sk_buff* skb, __u64 cookie, __u8 direction) {
	/*
    struct debug_event* ev = (struct debug_event*)bpf_ringbuf_reserve(&debug_events, sizeof(struct debug_event), 0);
    if (!ev) return;
    __builtin_memset(ev->data, 0, sizeof(ev->data));
    int pkt_len = skb->len < 32 ? skb->len : 32;
    bpf_skb_load_bytes(skb, 0, ev->data, pkt_len);
    ev->pkt_len = skb->len;
    ev->cookie = cookie;
    ev->direction = direction;
    bpf_ringbuf_submit(ev, 0);
    */
}

static __always_inline void emit_debug_event_xdp(struct xdp_md* ctx) {
	/*
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;
    struct debug_event* ev = (struct debug_event*)bpf_ringbuf_reserve(&debug_events, sizeof(struct debug_event), 0);
    if (!ev) return;
    __builtin_memset(ev->data, 0, sizeof(ev->data));
    int pkt_len = (data_end - data) < 32 ? (data_end - data) : 32;
    if (pkt_len > 0) {
        __builtin_memcpy(ev->data, data, pkt_len);
    }
    ev->pkt_len = (data_end - data);
    ev->cookie = 0;
    ev->direction = 3; // xdp
    bpf_ringbuf_submit(ev, 0);
    */
}

static __always_inline void publish_ip_event_skb(struct __sk_buff* skb, __u64 cookie, __u8 dir, __u64 pid_tgid, __u64 cgid)
{
    emit_debug_event_skb(skb, cookie, dir);
	// Ethernet header
	struct ethhdr eth = {};
	if (bpf_skb_load_bytes(skb, 0, &eth, sizeof(eth)) < 0)
		return;
	__u16 h_proto = eth.h_proto;
	__u32 off = sizeof(eth);

	// One VLAN tag (bounded)
	if (h_proto == bpf_htons(ETH_P_8021Q) || h_proto == bpf_htons(ETH_P_8021AD)) {
		struct { __u16 tci; __u16 proto; } vh = {};
		if (bpf_skb_load_bytes(skb, off, &vh, sizeof(vh)) < 0)
			return;
		h_proto = vh.proto;
		off += sizeof(vh);
	}

	if (h_proto == bpf_htons(ETH_P_IP)) {
		__u8 ver_ihl = 0;
		if (bpf_skb_load_bytes(skb, off, &ver_ihl, 1) < 0)
			return;
		__u32 ihl = (ver_ihl & 0x0Fu) * 4u;

		struct iphdr iph = {};
		if (bpf_skb_load_bytes(skb, off, &iph, sizeof(iph)) < 0)
			return;
		__u32 l4off = off + ihl;
		__u16 sport = 0, dport = 0;
		if (iph.protocol == IPPROTO_TCP || iph.protocol == IPPROTO_UDP) {
			struct { __u16 sport, dport; } ports = {};
			if (bpf_skb_load_bytes(skb, l4off, &ports, sizeof(ports)) == 0) {
				sport = bpf_ntohs(ports.sport);
				dport = bpf_ntohs(ports.dport);
			}
		}
		struct ip_event* ev = (struct ip_event*)bpf_ringbuf_reserve(&ip_events, sizeof(struct ip_event), 0);
		if (!ev) return;
		ev->cookie    = cookie;
		ev->pid_tgid  = pid_tgid;
		ev->cgroup_id = cgid;
		ev->direction = dir;
		ev->family    = AF_INET;
		ev->l4proto   = iph.protocol;
		ev->v4.saddr  = iph.saddr;
		ev->v4.daddr  = iph.daddr;
		ev->v4.sport  = sport;
		ev->v4.dport  = dport;
		bpf_ringbuf_submit(ev, 0);
	} else if (h_proto == bpf_htons(ETH_P_IPV6)) {
		struct ipv6hdr ip6h = {};
		if (bpf_skb_load_bytes(skb, off, &ip6h, sizeof(ip6h)) < 0)
			return;
		__u8  nexthdr = ip6h.nexthdr;
		__u32 l4off   = off + sizeof(struct ipv6hdr);
		__u16 sport = 0, dport = 0;
		if (nexthdr == IPPROTO_TCP || nexthdr == IPPROTO_UDP) {
			struct { __u16 sport, dport; } ports = {};
			if (bpf_skb_load_bytes(skb, l4off, &ports, sizeof(ports)) == 0) {
				sport = bpf_ntohs(ports.sport);
				dport = bpf_ntohs(ports.dport);
			}
		}
		struct ip_event* ev = (struct ip_event*)bpf_ringbuf_reserve(&ip_events, sizeof(struct ip_event), 0);
		if (!ev) return;
		ev->cookie    = h_proto;
		ev->pid_tgid  = pid_tgid;
		ev->cgroup_id = cgid;
		ev->direction = dir;
		ev->family    = AF_INET6;
		ev->l4proto   = nexthdr;
		__builtin_memcpy(ev->v6.saddr, &ip6h.saddr, 16);
		__builtin_memcpy(ev->v6.daddr, &ip6h.daddr, 16);
		ev->v6.sport  = sport;
		ev->v6.dport  = dport;
		bpf_ringbuf_submit(ev, 0);
	}
}

static __always_inline void publish_ip_event_xdp(struct xdp_md* ctx)
{
	emit_debug_event_xdp(ctx);
	void* data = (void*)(long)ctx->data;
	void* data_end = (void*)(long)ctx->data_end;
	struct ethhdr* eth = (struct ethhdr*)data;
	if ((void*)(eth + 1) > data_end)
		return;
	__u16 h_proto = eth->h_proto;
	unsigned char* cursor = (unsigned char*)(eth + 1);

	// One VLAN tag (bounded)
	if (h_proto == bpf_htons(ETH_P_8021Q) || h_proto == bpf_htons(ETH_P_8021AD)) {
		struct waechter_vlan_hdr* vh = (struct waechter_vlan_hdr*)(void*)cursor;
		if ((void*)(cursor + sizeof(*vh)) > data_end)
			return;
		h_proto = vh->proto;
		cursor += sizeof(*vh);
	}

	if (h_proto == bpf_htons(ETH_P_IP)) {
		struct iphdr* iph = (struct iphdr*)cursor;
		if ((void*)(iph + 1) > data_end)
			return;
		__u32 ihl = (iph->ihl & 0x0Fu) * 4u;
		unsigned char* l4 = (unsigned char*)iph + ihl;
		if ((void*)l4 > data_end)
			return;
		__u16 sport = 0, dport = 0;
		if (iph->protocol == IPPROTO_TCP || iph->protocol == IPPROTO_UDP) {
			if ((void*)(l4 + sizeof(__u32)) <= data_end) {
				__u16* p = (__u16*)(void*)l4;
				sport = bpf_ntohs(p[0]);
				dport = bpf_ntohs(p[1]);
			}
		}
		struct ip_event* ev = (struct ip_event*)bpf_ringbuf_reserve(&ip_events, sizeof(struct ip_event), 0);
		if (!ev) return;
		ev->cookie    = 0;
		ev->pid_tgid  = 0;
		ev->cgroup_id = 0;
		ev->direction = 3; // xdp
		ev->family    = AF_INET;
		ev->l4proto   = iph->protocol;
		ev->v4.saddr  = iph->saddr;
		ev->v4.daddr  = iph->daddr;
		ev->v4.sport  = sport;
		ev->v4.dport  = dport;
		bpf_ringbuf_submit(ev, 0);
	} else if (h_proto == bpf_htons(ETH_P_IPV6)) {
		struct ipv6hdr* ip6h = (struct ipv6hdr*)cursor;
		if ((void*)(ip6h + 1) > data_end)
			return;
		__u8  nexthdr = ip6h->nexthdr;
		unsigned char* l4 = (unsigned char*)(ip6h + 1);
		if ((void*)l4 > data_end)
			return;
		__u16 sport = 0, dport = 0;
		if (nexthdr == IPPROTO_TCP || nexthdr == IPPROTO_UDP) {
			if ((void*)(l4 + sizeof(__u32)) <= data_end) {
				__u16* p = (__u16*)(void*)l4;
				sport = bpf_ntohs(p[0]);
				dport = bpf_ntohs(p[1]);
			}
		}
		struct ip_event* ev = (struct ip_event*)bpf_ringbuf_reserve(&ip_events, sizeof(struct ip_event), 0);
		if (!ev) return;
		ev->cookie    = 0;
		ev->pid_tgid  = 0;
		ev->cgroup_id = 0;
		ev->direction = 3; // xdp
		ev->family    = AF_INET6;
		ev->l4proto   = nexthdr;
		__builtin_memcpy(ev->v6.saddr, &ip6h->saddr, 16);
		__builtin_memcpy(ev->v6.daddr, &ip6h->daddr, 16);
		ev->v6.sport  = sport;
		ev->v6.dport  = dport;
		bpf_ringbuf_submit(ev, 0);
	}
}

SEC("xdp")
int xdp_waechter(struct xdp_md* ctx)
{
	void* data = (void*)(long)ctx->data;
	void* data_end = (void*)(long)ctx->data_end;
	__u64 bytes = (unsigned long)data_end - (unsigned long)data;

	attribute_and_count(ctx, bytes);
	publish_ip_event_xdp(ctx);

	__u32 scope = 0;
	if (!enforce_limits(0 /*cookie*/, 0 /*pid*/, 0 /*cgid*/, bytes, &scope))
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
	struct flow_stats* gs = (struct flow_stats*)bpf_map_lookup_elem(&global_stats, &gk);
	add_stats(gs, bytes);
	__u32              pid = pm.pid_tgid >> 32;
	struct flow_stats* ps = (struct flow_stats*)bpf_map_lookup_elem(&pid_stats, &pid);
	if (!ps)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&pid_stats, &pid, &zero, BPF_NOEXIST);
		ps = (struct flow_stats*)bpf_map_lookup_elem(&pid_stats, &pid);
	}
	if (ps)
		add_stats(ps, bytes);
	struct flow_stats* cgst = (struct flow_stats*)bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	if (!cgst)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&cgroup_stats, &pm.cgroup_id, &zero, BPF_NOEXIST);
		cgst = (struct flow_stats*)bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	}
	if (cgst)
		add_stats(cgst, bytes);
	if (cookie)
	{
		struct flow_stats* cs = (struct flow_stats*)bpf_map_lookup_elem(&conn_stats, &cookie);
		if (!cs)
		{
			struct flow_stats zero = {};
			bpf_map_update_elem(&conn_stats, &cookie, &zero, BPF_NOEXIST);
			cs = (struct flow_stats*)bpf_map_lookup_elem(&conn_stats, &cookie);
		}
		if (cs)
			add_stats(cs, bytes);
	}

	// Emit streaming accounting and IP events
	emit_acct_event(cookie, pid, pm.cgroup_id, 1 /*egress*/, bytes, 1);
	publish_ip_event_skb(skb, cookie, 1 /*egress*/, pm.pid_tgid, pm.cgroup_id);

	// Enforce egress throttles (drop if any limit exceeded)
	__u32 scope = 0;
	if (!enforce_limits(cookie, pid, pm.cgroup_id, bytes, &scope))
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
	struct flow_stats* gs = (struct flow_stats*)bpf_map_lookup_elem(&global_stats, &gk);
	add_stats(gs, bytes);

	__u32              pid = pm.pid_tgid >> 32;
	struct flow_stats* ps = (struct flow_stats*)bpf_map_lookup_elem(&pid_stats, &pid);
	if (!ps)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&pid_stats, &pid, &zero, BPF_NOEXIST);
		ps = (struct flow_stats*)bpf_map_lookup_elem(&pid_stats, &pid);
	}
	if (ps)
		add_stats(ps, bytes);

	struct flow_stats* cgst = (struct flow_stats*)bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	if (!cgst)
	{
		struct flow_stats zero = {};
		bpf_map_update_elem(&cgroup_stats, &pm.cgroup_id, &zero, BPF_NOEXIST);
		cgst = (struct flow_stats*)bpf_map_lookup_elem(&cgroup_stats, &pm.cgroup_id);
	}
	if (cgst)
		add_stats(cgst, bytes);

	if (cookie)
	{
		struct flow_stats* cs = (struct flow_stats*)bpf_map_lookup_elem(&conn_stats, &cookie);
		if (!cs)
		{
			struct flow_stats zero = {};
			bpf_map_update_elem(&conn_stats, &cookie, &zero, BPF_NOEXIST);
			cs = (struct flow_stats*)bpf_map_lookup_elem(&conn_stats, &cookie);
		}
		if (cs)
			add_stats(cs, bytes);
	}

	// Emit streaming accounting and IP events
	emit_acct_event(cookie, pid, pm.cgroup_id, 2 /*ingress*/, bytes, 1);
	publish_ip_event_skb(skb, cookie, 2 /*ingress*/, pm.pid_tgid, pm.cgroup_id);

	__u32 scope = 0;
	if (!enforce_limits(cookie, pid, pm.cgroup_id, bytes, &scope))
		return 0; // drop

	return 1; // continue
}

char LICENSE[] SEC("license") = "Proprietary";
