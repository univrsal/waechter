#pragma once

#include <unistd.h>
#include <memory>

#include "IPAddress.hpp"
#include "Buffer.hpp"
#include "Types.hpp"
#include "EBPFCommon.h"

#define WMESSAGE(clazz, type) \
	clazz() : WMessage(type) {}

enum EMessageType : uint8_t
{
	MT_Traffic,
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
	WSocketTuple Connection{};
	size_t       Length{};
	WProcessId   Pid{};

	EPacketDirection Direction{};

	WMESSAGE(WMessageTraffic, MT_Traffic)

	WMessageTraffic(WSocketTuple const& Pair, size_t Len, EPacketDirection Dir)
		: WMessage(MT_Traffic)
		, Connection(Pair)
		, Length(Len)
		, Pid(getpid())
		, Direction(Dir)
	{
	}

	void Serialize(WBuffer& Buf) const override
	{
		WMessage::Serialize(Buf);
		Buf.Write(Connection);
		Buf.Write(Length);
		Buf.Write(Pid);
		Buf.Write(Direction);
	}

	void Deserialize(WBuffer& Buf) override
	{
		Buf.Read(Connection);
		Buf.Read(Length);
		Buf.Read(Pid);
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
	}

	return Msg;
}
