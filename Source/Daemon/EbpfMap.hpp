//
// Created by usr on 12/10/2025.
//

#pragma once

#include <unordered_map>
#include <bpf/bpf.h>
#include <cstdint>

template<typename T>
class WEbpfMap
{
	int MapFd{-1};

	std::unordered_map<uint32_t, T> Elements{};
public:

	explicit WEbpfMap(int MapFd)
		: MapFd(MapFd)
	{
	}

	bool IsValid() const
	{
		return MapFd >= 0;
	}

	// Get the first element, fetch from the bpf map if not in the local cache
	T const* First()
	{
		if (!Elements.empty())
		{
			return &Elements.begin()->second;
		}

		T Value{};
		uint32_t Key;

		if (bpf_map_lookup_elem(MapFd, &Key, &Value) == 0)
		{
			Elements[Key] = Value;
			return &Elements[Key];
		}

		return nullptr;

	}

	// Lookup an element by key, fetch from the bpf map if not in the local cache
	T const* Lookup(uint32_t Key = 0)
	{
		auto It = Elements.find(Key);
		if (It != Elements.end())
		{
			return &It->second;
		}

		T Value{};
		if (bpf_map_lookup_elem(MapFd, &Key, &Value) == 0)
		{
			Elements[Key] = Value;
			return &Elements[Key];
		}

		return nullptr;
	}

	// Fetch all elements from the bpf map into the local cache
	void Update()
	{
		uint32_t Key = 0;
		int      Ret = bpf_map_get_next_key(MapFd, nullptr, &Key);
		while (Ret == 0)
		{
			T Data{};
			if (bpf_map_lookup_elem(MapFd, &Key, &Data) == 0)
			{
				Elements[Key] = Data;
			}
			uint32_t Prev = Key;
			Ret = bpf_map_get_next_key(MapFd, &Prev, &Key);
		}
	}
};
