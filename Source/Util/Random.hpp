/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <random>

class WRandom
{
public:
	static std::string GenerateRandomHexString(size_t Length)
	{
		static char const                                  HexChars[] = "0123456789abcdef";
		thread_local std::mt19937_64                       Generator(std::random_device{}());
		thread_local std::uniform_int_distribution<size_t> Distribution(0, sizeof(HexChars) - 2);

		std::string Result;
		Result.reserve(Length);

		for (size_t i = 0; i < Length; ++i)
		{
			Result += HexChars[Distribution(Generator)];
		}

		return Result;
	}

	static uint32_t RandomInteger(uint32_t Min = 0, uint32_t Max = std::numeric_limits<uint32_t>::max())
	{
		thread_local std::mt19937_64  Generator(std::random_device{}());
		std::uniform_int_distribution Distribution(Min, Max);
		return Distribution(Generator);
	}
};