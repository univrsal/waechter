//
// Created by usr on 26/10/2025.
//

#pragma once
#include <sys/types.h>
#include <cstdint>
#include <chrono>

#define WKiB *1024
#define WMiB *1024 WKiB
#define WGiB *1024 WMiB

using WProcessId = pid_t;
using WSocketCookie = uint64_t;
using WMsec = int64_t;
using WBytes = uint64_t;
using WBytesPerSecond = double;
using WUsec = std::chrono::microseconds;