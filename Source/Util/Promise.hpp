/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <functional>
#include <memory>
#include <future>

template <typename T>
class TPromise
{
	struct WSharedState
	{
		std::function<void(T)> Callback;
		std::promise<void>     CallbackReady;
	};

	std::shared_ptr<WSharedState> SharedState;

public:
	TPromise() { SharedState = std::make_shared<WSharedState>(); }
	virtual ~TPromise() {}

	void Finish(T const& Result) const
	{
		SharedState->CallbackReady.get_future().wait();
		SharedState->Callback(Result);
	}

	void Then(std::function<void(T)> Callback)
	{
		SharedState->Callback = Callback;
		SharedState->CallbackReady.set_value();
	}
};