#include "TrafficCounter.hpp"

#include <Time.hpp>

WTrafficCounter::WTrafficCounter()
{
	TimeWindowStart = WTime::GetEpochMs();
}

void WTrafficCounter::PushIncomingTraffic(WBytes Bytes)
{
	RecentDownload += Bytes;
	State = CS_Active;
}

void WTrafficCounter::PushOutgoingTraffic(WBytes Bytes)
{
	RecentUpload += Bytes;
	State = CS_Active;
}

void WTrafficCounter::Refresh()
{
	if (State == CS_Removed)
	{
		return;
	}

	if (State == CS_PendingRemoval && WTime::GetEpochMs() >= RemovalTime)
	{
		State = CS_Removed;
		DownloadSpeed = 0;
		UploadSpeed = 0;
		return;
	}

	if (auto const TimeStampMS = WTime::GetEpochMs(); TimeStampMS - TimeWindowStart >= RecentTrafficTimeWindow)
	{
		DownloadSpeed = RecentDownload / (RecentTrafficTimeWindow / 1000.f);
		RecentDownload = 0;
		UploadSpeed = RecentUpload / (RecentTrafficTimeWindow / 1000.f);
		RecentUpload = 0;
		TimeWindowStart = TimeStampMS;

		if (State != CS_PendingRemoval)
		{
			if (DownloadSpeed == 0 && UploadSpeed == 0)
			{
				State = CS_Inactive;
			}
			else
			{
				State = CS_Active;
			}
		}
	}
}