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
	if (State == CS_PendingRemoval)
	{
		DownloadSpeed = 0;
		UploadSpeed = 0;
		return;
	}

	if (auto const TimeStampMS = WTime::GetEpochMs(); TimeStampMS - TimeWindowStart >= RecentTrafficTimeWindow)
	{
		DownloadSpeed = static_cast<double>(RecentDownload) / (static_cast<double>(RecentTrafficTimeWindow) / 1000.0);
		RecentDownload = 0;
		UploadSpeed = static_cast<double>(RecentUpload) / (RecentTrafficTimeWindow / 1000.0);
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