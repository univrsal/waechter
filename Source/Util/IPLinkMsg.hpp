/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
};

struct WRemoveHtbClassMsg
{
	std::string InterfaceName{};
	uint32_t    Mark{};
	uint16_t    MinorId{};

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Mark, MinorId, InterfaceName);
	}
};

struct WSetupHtbClassMsg
{
	std::string     InterfaceName{};
	uint32_t        Mark{};
	uint16_t        MinorId{};
	WBytesPerSecond RateLimit{};

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Mark, MinorId, RateLimit, InterfaceName);
	}
};

struct WIPLinkMsg
{
	EIPLinkMsgType Type{};

	std::shared_ptr<WSetupHtbClassMsg>     SetupHtbClass{};
	std::shared_ptr<WRemoveHtbClassMsg>    RemoveHtbClass{};

	template <class Archive>
	void serialize(Archive& Ar)
	{
		Ar(Type);
		Ar(SetupHtbClass);
		Ar(RemoveHtbClass);
	}
};