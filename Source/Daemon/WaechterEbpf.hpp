//
// Created by usr on 12/10/2025.
//

#pragma once

#include <string>
#include <memory>

#include "EbpfObj.hpp"

class WEbpfData;

enum class EEbpfInitResult
{
	Open_Failed,
	Load_Failed,
	Xdp_Attach_Failed,
	Cg_Ingress_Attach_Failed,
	CG_Egress_Attach_Failed,
	Inet_Socket_Create_Failed,
	Inet4_Socket_Connect_Failed,
	Inet6_Socket_Connect_Failed,
	Ring_Buffers_Not_Found,
	Success = 0,
};

class WWaechterEbpf : public WEbpfObj
{
	int InterfaceIndex{ -1 };

	std::shared_ptr<WEbpfData> Data{};

public:
	explicit WWaechterEbpf(int InterfaceIndex = -1, std::string const& ProgramObjectFilePath = BPF_OBJECT_PATH);
	~WWaechterEbpf();

	EEbpfInitResult Init();

	std::shared_ptr<WEbpfData> GetData() { return Data; }

	void PrintStats();
	void UpdateData();
};
