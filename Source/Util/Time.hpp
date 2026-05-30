/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>

#include "Singleton.hpp"
#include "Types.hpp"

#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace WTime
{
	static WMsec GetEpochMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();
	}

	static int64_t GetEpochSeconds()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
			.count();
	}

	static std::string FormatTime(long Time)
	{
		long Hours = Time / 3600;
		long Minutes = (Time % 3600) / 60;
		long Seconds = Time % 60;
		return std::format("{:02}:{:02}:{:02}", Hours, Minutes, Seconds);
	}
} // namespace WTime

class WTimer
{
	double                Interval{};
	double                ElapsedTime{ 0.0 };
	int                   Id{};
	bool                  AlignToWallClock{ false };
	int64_t               NextWallClockFireTime{ 0 };
	std::function<void()> Callback{};
	friend class WTimerManager;
	bool Update(double DeltaTime)
	{
		if (AlignToWallClock)
		{
			if (WTime::GetEpochSeconds() >= NextWallClockFireTime)
			{
				NextWallClockFireTime += static_cast<int64_t>(Interval);
				return true;
			}
			return false;
		}
		ElapsedTime += DeltaTime;
		if (ElapsedTime >= Interval)
		{
			ElapsedTime -= Interval;
			return true;
		}
		return false;
	}
	WTimer(double IntervalSeconds, std::function<void()> Callback_, int Id_, bool AlignToWallClock_ = false);
};

class WTimerManager : public TSingleton<WTimerManager>
{
	std::vector<WTimer> Timers{};
	double              LastUpdateTime{};
	std::atomic<int>    TimerIdCounter{ 0 };
	std::mutex          TimerMutex;

public:
	int AddTimer(double IntervalSeconds, std::function<void()> Callback)
	{
		std::scoped_lock Lock(TimerMutex);
		auto   NewId = TimerIdCounter++;
		WTimer Timer(IntervalSeconds, std::move(Callback), NewId);
		Timers.push_back(Timer);
		return NewId;
	}

	int AddAlignedTimer(double IntervalSeconds, std::function<void()> Callback)
	{
		std::scoped_lock Lock(TimerMutex);
		auto   NewId = TimerIdCounter++;
		WTimer Timer(IntervalSeconds, std::move(Callback), NewId, true);
		Timers.push_back(Timer);
		return NewId;
	}

	void RemoveTimer(int TimerId)
	{
		if (Timers.empty())
		{
			return;
		}

		std::scoped_lock Lock(TimerMutex);
		std::erase_if(Timers, [TimerId](WTimer const& Timer) { return Timer.Id == TimerId; });
	}

	void Start(double CurrentTime) { LastUpdateTime = CurrentTime; }
	void UpdateTimers(double CurrentTime);
};