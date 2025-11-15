//
// Created by usr on 09/10/2025.
//

#pragma once
#include <cerrno>
#include <cstring>
#include <string>

class WErrnoUtil
{
public:
	static std::string StrError() { return std::string(strerror(errno)); }
};