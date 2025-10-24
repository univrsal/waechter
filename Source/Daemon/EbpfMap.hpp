//
// Created by usr on 12/10/2025.
//

#pragma once

#include <unordered_map>
#include <bpf/bpf.h>
#include <cstdint>
#include <spdlog/spdlog.h>

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

	std::unordered_map<uint32_t, T> const & GetMap() const { return Elements; }

	bool IsValid() const
	{
		return MapFd >= 0;
	}

	bool Lookup(T& OutValue, uint32_t& Key)
	{
		if (bpf_map_lookup_elem(MapFd, &Key, &OutValue) == 0)
		{
			return true;
		}

		return false;
	}

	// Fetch all elements from the bpf map into the local cache
	void Update()
	{
		Elements.clear();
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

	int GetFd() const { return MapFd; }
};
