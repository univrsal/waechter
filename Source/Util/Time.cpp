/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Time.hpp"

#include <utility>

WTimer::WTimer(double IntervalSeconds, std::function<void()> Callback_)
	: Interval(IntervalSeconds), Callback(std::move(Callback_))
{
}

void WTimerManager::UpdateTimers(double CurrentTime)
{
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
