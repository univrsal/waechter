#pragma once

#include <Types.hpp>
#include <Time.hpp>

enum ECounterState
{
	CS_Inactive,       // No traffic activity during the last time window
	CS_Active,         // Traffic activity during the last time window
	CS_PendingRemoval, // Process has quit
	CS_Removed         // Process has been removed
};

class WTrafficCounter final
{
protected:
	WBytes RecentUpload{};
	WBytes RecentDownload{};

	WMsec TimeWindowStart{};
	WMsec RemovalTime{};

	WBytesPerSecond DownloadSpeed{};
	WBytesPerSecond UploadSpeed{};

	ECounterState State{ CS_Inactive };

public:
	static constexpr WMsec RecentTrafficTimeWindow{ 1000 };
	static constexpr WMsec RemovalTimeWindow{ 5000 }; // Time between pending removal and actual removal

	WTrafficCounter();
	~WTrafficCounter() = default;

	void PushIncomingTraffic(WBytes Bytes);
	void PushOutgoingTraffic(WBytes Bytes);

	void Refresh();

	[[nodiscard]] WBytesPerSecond GetDownloadSpeed() const
	{
		return DownloadSpeed;
	}

	[[nodiscard]] WBytesPerSecond GetUploadSpeed() const
	{
		return UploadSpeed;
	}

	[[nodiscard]] ECounterState GetState() const
	{
		return State;
	}

	void MarkForRemoval()
	{
		if (State == CS_Inactive)
		{
			State = CS_PendingRemoval;
			RemovalTime = WTime::GetEpochMs() + RemovalTimeWindow;
		}
	}
};
