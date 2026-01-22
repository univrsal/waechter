/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <bpf/libbpf.h>
#include <mutex>
#include <deque>

#include "spdlog/spdlog.h"

template <typename T>
class TEbpfRingBuffer
{
	struct ring_buffer* RingBufferPtr{ nullptr };

	std::deque<T> Data{};
	std::mutex    DataMutex;

	static int RingBufferCallback(void* Context, void* Data, std::size_t DataSize)
	{
		if (Context && Data && DataSize == sizeof(T))
		{
			auto RB = static_cast<TEbpfRingBuffer*>(Context);
			T*   OutData = static_cast<T*>(Data);
			RB->PushData(*OutData);
		}
		return 0;
	}

public:
	explicit TEbpfRingBuffer(bpf_map* Map)
	{
		if (Map == nullptr)
		{
			spdlog::error("WEbpfRingBuffer: invalid Map {} provided; map not found or not loaded");
			RingBufferPtr = nullptr;
			return;
		}
		int Fd = bpf_map__fd(Map);
		RingBufferPtr = ring_buffer__new(Fd, RingBufferCallback, this, nullptr);
		if (!RingBufferPtr)
		{
			spdlog::error("WEbpfRingBuffer: ring_buffer__new failed");
		}
	}

	~TEbpfRingBuffer() { ring_buffer__free(RingBufferPtr); }

	void PushData(T const& NewData)
	{
		std::lock_guard Lock(DataMutex);
		Data.push_back(NewData);
	}

	std::mutex& GetDataMutex() { return DataMutex; }

	std::deque<T>& GetData() { return Data; }

	void Poll(int TimeOutMS = 0)
	{
		auto Return = ring_buffer__poll(RingBufferPtr, TimeOutMS);
		if (Return < 0)
		{
			spdlog::error("ring_buffer__poll failed: {}", Return);
		}
	}

	[[nodiscard]] bool IsValid() const { return RingBufferPtr != nullptr; }
};
