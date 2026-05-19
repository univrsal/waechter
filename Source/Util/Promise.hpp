/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>

template <typename T>
class TPromise
{
	struct WSharedState
	{
		std::function<void(T)>  Callback;
		std::mutex              Mutex;
		std::condition_variable CV;
		bool                    Ready = false;
	};

	std::shared_ptr<WSharedState> SharedState;

public:
	TPromise() { SharedState = std::make_shared<WSharedState>(); }
	virtual ~TPromise() = default;

	void Finish(T const& Result) const
	{
		std::unique_lock Lock(SharedState->Mutex);
		SharedState->CV.wait(Lock, [this] { return SharedState->Ready; });
		SharedState->Callback(Result);
	}

	void Then(std::function<void(T)> Callback)
	{
		{
			std::scoped_lock Lock(SharedState->Mutex);
			SharedState->Callback = Callback;
			SharedState->Ready = true;
		}
		SharedState->CV.notify_one();
	}
};