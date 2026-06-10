/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <cerrno>
#include <cstring>
#include <string>

class WErrnoUtil
{
public:
	static std::string StrError(int const Err = 0)
	{
		if (Err == 0)
		{
			return std::string(strerror(errno));
		}
		else
		{
			return std::string(strerror(Err));
		}
	}
};