//
// Created by usr on 12/10/2025.
//

#pragma once

#include <string>
#include <memory>

#include "EbpfObj.hpp"
#include "Types.hpp"

class WEbpfData;

enum class EEbpfInitResult
{
	Open_Failed,
	Load_Failed,
	Cg_Ingress_Attach_Failed,
	CG_Egress_Attach_Failed,
	Inet_Socket_Create_Failed,
	Inet4_Socket_Connect_Failed,
	Inet6_Socket_Connect_Failed,
	Inet4_Socket_SendMsg_Failed,
	Inet6_Socket_SendMsg_Failed,
	On_Tcp_Set_State_Failed,
	On_Inet_Sock_Destruct_Failed,
	Ring_Buffers_Not_Found,
	Success = 0,
};

class WWaechterEbpf : public WEbpfObj
{
	int                        InterfaceIndex{ -1 };
	std::shared_ptr<WEbpfData> Data{};
	WMsec                      QueuePileupStartTime{};

public:
	explicit WWaechterEbpf(std::string const& ProgramObjectFilePath);
	~WWaechterEbpf();

	EEbpfInitResult Init();

	std::shared_ptr<WEbpfData> GetData() { return Data; }

	void PrintStats();
	void UpdateData();
};
