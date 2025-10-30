#pragma once

#include "Types.hpp"
#include "Time.hpp"

enum ECounterState
{
	CS_Inactive,       // No traffic activity during the last time window
	CS_Active,         // Traffic activity during the last time window
	CS_PendingRemoval, // Process has quit
};

template <typename T>
class TTrafficCounter
{
protected:
	WBytes RecentUpload{};
	WBytes RecentDownload{};

	WMsec TimeWindowStart{};

	ECounterState State{ CS_Inactive };

public:
	std::shared_ptr<T> TrafficItem;

	explicit TTrafficCounter(std::shared_ptr<T> const& TrafficItem_)
		: TrafficItem(TrafficItem_)
	{
		TimeWindowStart = WTime::GetEpochMs();
	}

	static constexpr WMsec RecentTrafficTimeWindow{ 1000 };
	static constexpr WMsec RemovalTimeWindow{ 5000 }; // Time between pending removal and actual removal

	~TTrafficCounter() = default;

	void PushOutgoingTraffic(WBytes Bytes)
	{
		RecentUpload += Bytes;
		State = CS_Active;
	}

	void PushIncomingTraffic(WBytes Bytes)
	{
		RecentDownload += Bytes;
		State = CS_Active;
	}

	void Refresh()
	{
		if (State == CS_PendingRemoval)
		{
			TrafficItem->DownloadSpeed = 0;
			TrafficItem->UploadSpeed = 0;
			return;
		}

		if (auto const TimeStampMS = WTime::GetEpochMs(); TimeStampMS - TimeWindowStart >= RecentTrafficTimeWindow)
		{
			TrafficItem->DownloadSpeed = static_cast<double>(RecentDownload) / (static_cast<double>(RecentTrafficTimeWindow) / 1000.0);
			RecentDownload = 0;
			TrafficItem->UploadSpeed = static_cast<double>(RecentUpload) / (RecentTrafficTimeWindow / 1000.0);
			RecentUpload = 0;
			TimeWindowStart = TimeStampMS;

			if (State != CS_PendingRemoval)
			{
				if (TrafficItem->DownloadSpeed < 1)
				{
					State = CS_Inactive;
					TrafficItem->DownloadSpeed = 0;
				}

				if (TrafficItem->UploadSpeed < 1)
				{
					State = CS_Inactive;
					TrafficItem->UploadSpeed = 0;
				}
			}
		}
	}

	[[nodiscard]] ECounterState GetState() const
	{
		return State;
	}

	void MarkForRemoval()
	{
		State = CS_PendingRemoval;
	}
};
