//
// Created by usr on 02/11/2025.
//

#include "Time.hpp"

#include <utility>

WTimer::WTimer(double IntervalSeconds, std::function<void()> Callback_)
	: Interval(IntervalSeconds)
	, Callback(std::move(Callback_))
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
