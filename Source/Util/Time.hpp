/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <chrono>
#include <functional>
#include <atomic>

#include "spdlog/fmt/fmt.h"

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

	static int64_t GetUnixNow()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
			.count();
	}

	static std::string FormatTime(long Time)
	{
		long Hours = Time / 3600;
		long Minutes = (Time % 3600) / 60;
		long Seconds = Time % 60;
		return fmt::format("{:02}:{:02}:{:02}", Hours, Minutes, Seconds);
	}
} // namespace WTime

class WTimer
{
	double                Interval{};
	double                ElapsedTime{ 0.0 };
	int                   Id{};
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
	WTimer(double IntervalSeconds, std::function<void()> Callback_, int Id_);
};

class WTimerManager : public TSingleton<WTimerManager>
{
	std::vector<WTimer> Timers{};
	double              LastUpdateTime{};
	std::atomic<int>    TimerIdCounter{ 0 };

public:
	int AddTimer(double IntervalSeconds, std::function<void()> Callback)
	{
		auto   NewId = TimerIdCounter++;
		WTimer Timer(IntervalSeconds, std::move(Callback), NewId);
		Timers.push_back(Timer);
		return NewId;
	}

	void RemoveTimer(int TimerId)
	{
		std::erase_if(Timers, [TimerId](WTimer const& Timer) { return Timer.Id == TimerId; });
	}

	void Start(double CurrentTime) { LastUpdateTime = CurrentTime; }
	void UpdateTimers(double CurrentTime);
};