//
// Created by usr on 12/10/2025.
//

#pragma once
#include <unordered_map>
#include <bpf/bpf.h>
#include <linux/bpf.h>
#include <cstdint>

template <typename T>
class TEbpfMap
{
	int MapFd{ -1 };

	std::unordered_map<uint32_t, T> Elements{};

public:
	explicit TEbpfMap(bpf_map const* Map)
	{
		if (Map)
		{
			MapFd = bpf_map__fd(Map);
		}
	}

	std::unordered_map<uint32_t, T> const& GetMap() const { return Elements; }

	[[nodiscard]] bool IsValid() const { return MapFd >= 0; }

	bool Lookup(T& OutValue, uint32_t& Key)
	{
		if (bpf_map_lookup_elem(MapFd, &Key, &OutValue) == 0)
		{
			return true;
		}

		return false;
	}

	bool Update(uint32_t const& Key, T const& Value, uint64_t Flags = BPF_ANY) const
	{
		return bpf_map_update_elem(MapFd, &Key, &Value, Flags) == 0;
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
