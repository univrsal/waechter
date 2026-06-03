/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Time.hpp"

#include <utility>

WTimer::WTimer(double IntervalSeconds, std::function<void()> Callback_, int Id_, bool AlignToWallClock_)
	: Interval(IntervalSeconds), Id(Id_), AlignToWallClock(AlignToWallClock_), Callback(std::move(Callback_))
{
	if (AlignToWallClock)
	{
		auto const Now = WTime::GetEpochSeconds();
		auto const IntervalSecs = static_cast<int64_t>(IntervalSeconds);
		NextWallClockFireTime = (Now / IntervalSecs + 1) * IntervalSecs;
	}
}

void WTimerManager::UpdateTimers(double CurrentTime)
{
	std::scoped_lock Lock(TimerMutex);
	auto const Delta = CurrentTime - LastUpdateTime;
	LastUpdateTime = CurrentTime;
	for (auto& Timer : Timers)
	{
		if (Timer.Update(Delta))
		{
			Timer.Callback();
		}
	}
}
