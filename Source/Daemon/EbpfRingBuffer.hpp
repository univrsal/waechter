//
// Created by usr on 24/10/2025.
//

#pragma once
#include <bpf/libbpf.h>
#include <spdlog/spdlog.h>
#include <mutex>
#include <deque>
#include <functional>

template <typename T>
class WEbpfRingBuffer
{
	struct ring_buffer* RingBufferPtr{ nullptr };

	std::deque<T> Data{};
	std::mutex    DataMutex;

	static int RingBufferCallback(void* Context, void* Data, std::size_t DataSize)
	{
		if (Context && Data && DataSize == sizeof(T))
		{
			auto RB = static_cast<WEbpfRingBuffer*>(Context);
			T*   OutData = static_cast<T*>(Data);
			RB->PushData(*OutData);
		}
		return 0;
	}

public:
	explicit WEbpfRingBuffer(int MapFd)
	{
		RingBufferPtr = ring_buffer__new(MapFd, RingBufferCallback, this, nullptr);
	}

	~WEbpfRingBuffer()
	{
		if (RingBufferPtr)
		{
			ring_buffer__free(RingBufferPtr);
			RingBufferPtr = nullptr;
		}
	}

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

	[[nodiscard]] bool IsValid() const
	{
		return RingBufferPtr != nullptr;
	}
};
