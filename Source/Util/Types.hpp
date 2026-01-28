/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <sys/types.h>
#include <cstdint>
#include <chrono>

#define WKiB *1024
#define WMiB *1024 WKiB
#define WGiB *1024 WMiB

#if _WIN32
using ssize_t = int64_t;
#endif
using WProcessId = int32_t;
using WSocketCookie = uint64_t;
using WMsec = int64_t;
using WSec = int64_t;
using WBytes = uint64_t;
using WBytesPerSecond = double;
using WUsec = std::chrono::microseconds;