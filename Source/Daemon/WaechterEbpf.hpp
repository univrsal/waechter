//
// Created by usr on 12/10/2025.
//

#pragma once

#include <string>
#include <memory>
#include <bpf/libbpf.h>

#include "EbpfObj.hpp"

class WEbpfData;

// Event PODs mirrored from BPF program
struct WIpEvent
{
	unsigned long long Cookie;
	unsigned long long PidTgid;
	unsigned long long CgroupId;
	unsigned char      Direction; // 1=egress,2=ingress,3=xdp
	unsigned char      Family;    // AF_INET/AF_INET6
	unsigned char      L4Proto;   // IPPROTO_*
	unsigned char      _Pad;
	union {
		struct { unsigned int Saddr, Daddr; unsigned short Sport, Dport; } V4;
		struct { unsigned char Saddr[16], Daddr[16]; unsigned short Sport, Dport; } V6;
	};
};

struct WAcctEvent
{
	unsigned long long Cookie;
	unsigned int       Pid;
	unsigned long long CgroupId;
	unsigned char      Direction;
	unsigned char      _pad[3];
	unsigned long long Bytes;
	unsigned long long Packets;
};

struct WRlEvent
{
	unsigned long long Cookie;
	unsigned int       Pid;
	unsigned long long CgroupId;
	unsigned long long Bytes;
	unsigned int       ScopeMask;
	unsigned long long NowNs;
};

// Add back init result enum

enum class EEbpfInitResult
{
	OPEN_FAILED,
	LOAD_FAILED,
	XDP_ATTACH_FAILED,
	CG_INGRESS_ATTACH_FAILED,
	CG_EGRESS_ATTACH_FAILED,
	MAPS_NOT_FOUND,
	SUCCESS = 0,
};

class WWaechterEbpf : public WEbpfObj
{
	int InterfaceIndex{-1};

	std::shared_ptr<WEbpfData> Data{};
public:

	explicit WWaechterEbpf(int InterfaceIndex = -1, std::string const& ProgramObectFilePath = BPF_OBJECT_PATH);
	~WWaechterEbpf();

	EEbpfInitResult Init();

	std::shared_ptr<WEbpfData> GetData() { return Data; }

	void PrintStats();
	int  PollRingBuffers(int TimeoutMs);
};
