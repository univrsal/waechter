//
// Created by usr on 23/10/2025.

#pragma once
#include <cstdint>
#include <array>

enum class EIPFamily : uint8_t { Unknown=0, IPv4=4, IPv6=6 };

struct PacketKey {
    EIPFamily family{EIPFamily::Unknown};
    uint8_t  l4_proto{0};            // 6=TCP, 17=UDP, 1=ICMP, 58=ICMPv6, etc.
    // IPv4: first 4 bytes used; IPv6: all 16 bytes used.
    std::array<uint8_t,16> src{};
    std::array<uint8_t,16> dst{};
    uint16_t src_port{0};            // host byte order
    uint16_t dst_port{0};            // host byte order

	std::string src_to_string()
	{
		if (family == EIPFamily::IPv4)
		{
			return std::to_string(src[0]) + "." +
			       std::to_string(src[1]) + "." +
			       std::to_string(src[2]) + "." +
			       std::to_string(src[3]);
		}
		else if (family == EIPFamily::IPv6)
		{
			char buffer[40];
			snprintf(buffer, sizeof(buffer),
			         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
			         "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
			         src[0], src[1], src[2], src[3],
			         src[4], src[5], src[6], src[7],
			         src[8], src[9], src[10], src[11],
			         src[12], src[13], src[14], src[15]);
			return std::string(buffer);
		}
		return "Unknown";
	}

	std::string dst_to_string()
	{
		if (family == EIPFamily::IPv4)
		{
			return std::to_string(dst[0]) + "." +
			       std::to_string(dst[1]) + "." +
			       std::to_string(dst[2]) + "." +
			       std::to_string(dst[3]);
		}
		else if (family == EIPFamily::IPv6)
		{
			char buffer[40];
			snprintf(buffer, sizeof(buffer),
			         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
			         "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
			         dst[0], dst[1], dst[2], dst[3],
			         dst[4], dst[5], dst[6], dst[7],
			         dst[8], dst[9], dst[10], dst[11],
			         dst[12], dst[13], dst[14], dst[15]);
			return std::string(buffer);
		}
		return "Unknown";
	}
};

static inline uint16_t rd16(const uint8_t* p) { return (uint16_t(p[0])<<8) | uint16_t(p[1]); }
static inline uint32_t rd32(const uint8_t* p) { return (uint32_t(rd16(p))<<16) | rd16(p+2); }

static inline bool parse_ipv4(const uint8_t* b, size_t len, PacketKey& out, size_t& l4_off) {
    if (len < 20) return false;
    uint8_t vihl = b[0];
    if ((vihl >> 4) != 4) return false;
    uint8_t ihl = (vihl & 0x0F) * 4;
    if (ihl < 20 || len < ihl) return false;
    uint16_t tot_len = rd16(b+2);
    if (tot_len < ihl || len < tot_len) tot_len = (uint16_t)len; // be tolerant
    out.family = EIPFamily::IPv4;
    out.l4_proto = b[9];
    // src/dst
    for (int i=0;i<4;++i) { out.src[i] = b[12+i]; out.dst[i] = b[16+i]; }
    l4_off = ihl;
    // TCP header length fix-up when needed happens in caller
    return true;
}

static inline bool is_ipv6_ext(uint8_t nh) {
    switch (nh) {
        case 0:  // Hop-by-Hop
        case 43: // Routing
        case 44: // Fragment
        case 50: // ESP (can’t parse further payload/ports)
        case 51: // AH
        case 60: // Destination Options
        case 135:// Mobility
        case 139:// Host Identity
        case 140:// Shim6
        case 59: // No Next Header
            return true;
        default: return false;
    }
}

static inline bool parse_ipv6(const uint8_t* b, size_t len, PacketKey& out, size_t& l4_off) {
    if (len < 40) return false;
    if ((b[0] >> 4) != 6) return false;
    out.family = EIPFamily::IPv6;
    // src/dst
    for (int i=0;i<16;++i) { out.src[i] = b[8+i]; out.dst[i] = b[24+i]; }
    uint8_t next = b[6];
    size_t off = 40;

    while (true) {
        if (!is_ipv6_ext(next)) break;
        if (next == 59) { // No Next Header
            out.l4_proto = 59;
            return false;
        }
        if (next == 44) { // Fragment header: fixed 8 bytes
            if (len < off + 8) return false;
            next = b[off]; // Next Header is first byte
            off += 8;
            continue;
        }
        if (next == 51) { // AH: length in 32-bit words, not counting first 2 words
            if (len < off + 2) return false;
            uint8_t hdrlen_words = b[off+1];
            size_t hdrlen = (size_t(hdrlen_words) + 2) * 4;
            if (len < off + hdrlen) return false;
            next = b[off]; // Next Header
            off += hdrlen;
            continue;
        }
        if (next == 50) { // ESP: can’t safely find ports beyond this
            out.l4_proto = 50;
            return false;
        }
        // Hop-by-Hop, Dest Options, Routing: hdr ext len in 8-byte units (not including first 8 bytes)
        if (len < off + 2) return false;
        uint8_t hdr_ext_len = b[off+1];
        size_t hdrlen = (size_t(hdr_ext_len) + 1) * 8;
        if (len < off + hdrlen) return false;
        next = b[off]; // Next Header
        off += hdrlen;
    }

    out.l4_proto = next;
    l4_off = off;
    return true;
}

static inline bool parse_l4(const uint8_t* b, size_t len, PacketKey& out, size_t l4_off) {
    if (out.l4_proto == 6) { // TCP
        if (len < l4_off + 20) return false;
        out.src_port = rd16(b + l4_off);
        out.dst_port = rd16(b + l4_off + 2);
        uint8_t doff = (b[l4_off + 12] >> 4) * 4;
        if (doff < 20 || len < l4_off + doff) return false;
        return true;
    } else if (out.l4_proto == 17) { // UDP
        if (len < l4_off + 8) return false;
        out.src_port = rd16(b + l4_off);
        out.dst_port = rd16(b + l4_off + 2);
        return true;
    } else {
        // ICMP/ICMPv6 or other: ports left as 0
        return true;
    }
}

// Entry point: parse buffer starting at L3 (IPv4 or IPv6). Returns true if parsed.
inline bool parse_packet_l3(const uint8_t* data, size_t len, PacketKey& out) {
    out = PacketKey{};
    if (len == 0) return false;

    size_t l4_off = 0;
    if ((data[0] >> 4) == 4) {
        if (!parse_ipv4(data, len, out, l4_off)) return false;
    } else if ((data[0] >> 4) == 6) {
        if (!parse_ipv6(data, len, out, l4_off)) return false;
    } else {
        return false;
    }
    if (!parse_l4(data, len, out, l4_off)) return false;
    return true;
}