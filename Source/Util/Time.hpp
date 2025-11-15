#pragma once
#include <chrono>
#include <spdlog/fmt/fmt.h>
#include <functional>

#include "Singleton.hpp"
#include "Types.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

namespace WTime
{
	static WMsec GetEpochMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();
	}

	static std::string FormatTime(long Time)
	{
		// format as HH:MM:SS
		long Hours = Time / 3600;
		long Minutes = (Time % 3600) / 60;
		long Seconds = Time % 60;
		return fmt::format("{:02}:{:02}:{:02}", Hours, Minutes, Seconds);
	}
} // namespace WTime

class WTimer
{
	double const          Interval{};
	double                ElapsedTime{ 0.0 };
	std::function<void()> Callback{};
	friend class WTimerManager;
	bool Update(double DeltaTime)
	{
		ElapsedTime += DeltaTime;
		if (ElapsedTime >= Interval)
		{
			ElapsedTime -= Interval;
			return true;
		}
		return false;
	}
	WTimer(double IntervalSeconds, std::function<void()> Callback_);
};

class WTimerManager : public TSingleton<WTimerManager>
{
	std::vector<WTimer> Timers{};
	double              LastUpdateTime{};

public:
	void AddTimer(double IntervalSeconds, std::function<void()> Callback)
	{
		WTimer Timer(IntervalSeconds, std::move(Callback));
		Timers.push_back(Timer);
	}

	void Start(double CurrentTime) { LastUpdateTime = CurrentTime; }
	void UpdateTimers(double CurrentTime);
};