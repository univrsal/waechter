/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Types.hpp"
#include "Time.hpp"

enum ECounterState
{
	CS_Inactive,       // No traffic activity during the last time window
	CS_Active,         // Traffic activity during the last time window
	CS_PendingRemoval, // Process/Socket has quit/closed, in grace period before removal
};

template <typename T>
struct TTrafficCounter
{
protected:
	WBytes RecentUpload{};
	WBytes RecentDownload{};

	WMsec TimeWindowStart{};
	WMsec RemovalTimeStamp{};

	ECounterState State{ CS_Inactive };

	// The number of ticks this counter has not received any traffic in, currently one
	// tick = one second, clamped at 255
	uint8_t InactiveCounter{ 0 };

public:
	uint8_t GetInactiveCounter() const { return InactiveCounter; }

	std::shared_ptr<T> TrafficItem;

	explicit TTrafficCounter(std::shared_ptr<T> const& TrafficItem_) : TrafficItem(TrafficItem_)
	{
		TimeWindowStart = WTime::GetEpochMs();
	}

	static constexpr WMsec RecentTrafficTimeWindow{ 1000 };
	static constexpr WMsec RemovalTimeWindow{ 5000 }; // Time between pending removal and actual removal

	virtual ~TTrafficCounter() = default;

	void PushOutgoingTraffic(WBytes Bytes)
	{
		RecentUpload += Bytes;
		State = CS_Active;
		InactiveCounter = 0;
	}

	void PushIncomingTraffic(WBytes Bytes)
	{
		RecentDownload += Bytes;
		State = CS_Active;
		InactiveCounter = 0;
	}

	[[nodiscard]] bool IsActive() const { return State == CS_Active; }

	virtual void Refresh()
	{
		if (State == CS_PendingRemoval)
		{
			TrafficItem->DownloadSpeed = 0;
			TrafficItem->UploadSpeed = 0;
			return;
		}

		if (auto const TimeStampMS = WTime::GetEpochMs(); TimeStampMS - TimeWindowStart >= RecentTrafficTimeWindow)
		{
			auto NewDownloadSpeed =
				static_cast<double>(RecentDownload) / (static_cast<double>(RecentTrafficTimeWindow) / 1000.0);
			auto NewUploadSpeed = static_cast<double>(RecentUpload) / (RecentTrafficTimeWindow / 1000.0);

			if (std::abs(NewDownloadSpeed - TrafficItem->DownloadSpeed) > 0.01)
			{
				TrafficItem->DownloadSpeed = NewDownloadSpeed;
				State = CS_Active;
			}

			if (std::abs(NewUploadSpeed - TrafficItem->UploadSpeed) > 0.01)
			{
				TrafficItem->UploadSpeed = NewUploadSpeed;
				State = CS_Active;
			}

			if (TrafficItem->DownloadSpeed < 1)
			{
				TrafficItem->DownloadSpeed = 0;
			}

			if (TrafficItem->UploadSpeed < 1)
			{
				TrafficItem->UploadSpeed = 0;
			}

			if (TrafficItem->DownloadSpeed == 0 && TrafficItem->UploadSpeed == 0)
			{
				InactiveCounter = std::min<uint8_t>(static_cast<uint8_t>(InactiveCounter + 1), 255);
			}

			// We don't set it to inactive immediately
			// because the inactive state determines whether clients receive updates about this item,
			// and we also need to send an update when the download speed reaches 0
			if (InactiveCounter >= 3)
			{
				State = CS_Inactive;
			}

			TrafficItem->TotalDownloadBytes += RecentDownload;
			TrafficItem->TotalUploadBytes += RecentUpload;
			RecentDownload = 0;
			RecentUpload = 0;
			TimeWindowStart = TimeStampMS;
		}
	}

	[[nodiscard]] ECounterState GetState() const { return State; }

	[[nodiscard]] bool IsMarkedForRemoval() const { return State == CS_PendingRemoval; }

	void MarkForRemoval()
	{
		if (State == CS_PendingRemoval)
		{
			return;
		}
		State = CS_PendingRemoval;
		TrafficItem->UploadSpeed = 0;
		TrafficItem->DownloadSpeed = 0;
		RemovalTimeStamp = WTime::GetEpochMs() + RemovalTimeWindow;
	}

	[[nodiscard]] bool DueForRemoval() const
	{
		return State == CS_PendingRemoval && WTime::GetEpochMs() >= RemovalTimeStamp;
	}
};
