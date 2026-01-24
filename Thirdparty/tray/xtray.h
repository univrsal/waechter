/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

int XTrayInit(unsigned char const* TrayIconData, unsigned int TrayIconSize);

void XTrayEventLoop();

void XTrayCleanup();

#if defined(__cplusplus)
}
#endif
