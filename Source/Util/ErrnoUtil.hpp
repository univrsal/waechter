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
	static std::string StrError() { return std::string(strerror(errno)); }
};