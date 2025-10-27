#pragma once

#include <unistd.h>
#include <memory>
#include <utility>

#include "IPAddress.hpp"
#include "Buffer.hpp"
#include "Types.hpp"
#include "EBPFCommon.h"

#define WMESSAGE(clazz, type) \
	clazz() : WMessage(type) {}

enum EMessageType : uint8_t
{
	MT_Traffic,
	MT_TrafficTree,
	MT_SetTcpLimit,
};

class WMessage
{
	EMessageType Type{};

public:
	explicit WMessage(EMessageType T)
		: Type(T)
	{
	}

	WMessage(WMessage const&) = default;
	virtual ~WMessage() = default;

	virtual void Serialize(WBuffer& Buf) const
	{
		Buf.Write(Type);
	}

	virtual void Deserialize(WBuffer& Buf) = 0;

	[[nodiscard]] EMessageType GetType() const
	{
		return Type;
	}
};

class WMessageTraffic : public WMessage
{
public:
	size_t        Length{};
	WSocketCookie SocketCookie{};

	EPacketDirection Direction{};

	WMESSAGE(WMessageTraffic, MT_Traffic)

	WMessageTraffic(WSocketCookie Cookie, size_t Len, EPacketDirection Dir)
		: WMessage(MT_Traffic)
		, Length(Len)
		, SocketCookie(Cookie)
		, Direction(Dir)
	{
	}

	void Serialize(WBuffer& Buf) const override
	{
		WMessage::Serialize(Buf);
		Buf.Write(SocketCookie);
		Buf.Write(Length);
		Buf.Write(Direction);
	}

	void Deserialize(WBuffer& Buf) override
	{
		Buf.Read(SocketCookie);
		Buf.Read(Length);
		Buf.Read(Direction);
	}
};

class WMessageSetTCPLimit : public WMessage
{
public:
	WSocketCookie    SocketCookie{};
	WBytesPerSecond  Limit{};
	EPacketDirection Direction{};

	WMESSAGE(WMessageSetTCPLimit, MT_SetTcpLimit)

	WMessageSetTCPLimit(WSocketCookie SocketCookie, WBytesPerSecond Limit, EPacketDirection Direction)
		: WMessage(MT_SetTcpLimit)
		, SocketCookie(SocketCookie)
		, Limit(Limit)
		, Direction(Direction)
	{
	}

	void Serialize(WBuffer& buf) const override
	{
		WMessage::Serialize(buf);
		buf.Write(SocketCookie);
		buf.Write(Limit);
		buf.Write(Direction);
	}

	void Deserialize(WBuffer& Buf) override
	{
		Buf.Read(SocketCookie);
		Buf.Read(Limit);
		Buf.Read(Direction);
	}
};

class WMessageTrafficTree : public WMessage
{
public:
	std::string TrafficTreeJson{};

	WMESSAGE(WMessageTrafficTree, MT_TrafficTree)

	explicit WMessageTrafficTree(std::string TreeJson)
		: WMessage(MT_TrafficTree)
		, TrafficTreeJson(std::move(TreeJson))
	{
	}

	~WMessageTrafficTree() = default;

	void Serialize(WBuffer& Buf) const override
	{
		WMessage::Serialize(Buf);
		Buf.Write<std::size_t>(TrafficTreeJson.length());
		if (TrafficTreeJson.empty())
			return;
		Buf.Write(TrafficTreeJson.c_str(), TrafficTreeJson.size());
	}

	void Deserialize(WBuffer& Buf) override
	{
		auto Length = std::size_t{};
		Buf.Read(Length);
		if (Length == 0)
		{
			TrafficTreeJson.clear();
			return;
		}
		TrafficTreeJson.resize(Length);
		Buf.Read(&TrafficTreeJson[0], Length);
	}
};

static std::shared_ptr<WMessage> ReadFromBuffer(WBuffer& Buf)
{
	EMessageType Type;
	if (!Buf.Read(Type))
	{
		return nullptr;
	}

	std::shared_ptr<WMessage> Msg{};
	switch (Type)
	{
		case MT_Traffic:
			Msg = std::make_shared<WMessageTraffic>();
			Msg->Deserialize(Buf);
			break;
		case MT_SetTcpLimit:
			Msg = std::make_shared<WMessageSetTCPLimit>();
			Msg->Deserialize(Buf);
			break;
		case MT_TrafficTree:
			Msg = std::make_shared<WMessageTrafficTree>();
			Msg->Deserialize(Buf);
			break;
	}

	return Msg;
}
