//
// Created by usr on 10/12/2025.
//

#pragma once
#include "Types.hpp"

#include <cstdint>
#include <string>
#include <memory>

enum class EIPLinkMsgType : char
{
	Exit = 0,
	SetupHtbClass = 1,
	RemoveHtbClass = 2,
	ConfigurePortRouting = 3,
};

struct WRemoveHtbClassMsg
{
	std::string InterfaceName{};
	uint32_t    Mark{};
	uint16_t    MinorId{};
	bool        bIsUpload{};

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Mark, MinorId, InterfaceName, bIsUpload);
	}
};

struct WSetupHtbClassMsg
{
	std::string     InterfaceName{};
	uint32_t        Mark{};
	uint16_t        MinorId{};
	WBytesPerSecond RateLimit{};
	bool            bAddMarkFilter{};

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Mark, MinorId, RateLimit, InterfaceName, bAddMarkFilter);
	}
};

struct WConfigurePortRouting
{
	uint32_t QDiscId{};
	uint16_t Dport{};
	bool     bReplace{ false };
	bool     bRemove{ false };

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(QDiscId, Dport, bReplace, bRemove);
	}
};

struct WIPLinkMsg
{
	EIPLinkMsgType Type{};

	std::shared_ptr<WSetupHtbClassMsg>     SetupHtbClass{};
	std::shared_ptr<WRemoveHtbClassMsg>    RemoveHtbClass{};
	std::shared_ptr<WConfigurePortRouting> SetupPortRouting{};

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Type);
		Ar(SetupHtbClass);
		Ar(RemoveHtbClass);
		Ar(SetupPortRouting);
	}
};