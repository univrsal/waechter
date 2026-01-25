/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

int XTrayInit(unsigned char const* TrayIconData, unsigned int TrayIconDataSize, unsigned char TrayIconSize);

void XTrayEventLoop();

void XTrayCleanup();

void XTraySetBackgroundColor(unsigned char R, unsigned char G, unsigned char B);

#if defined(__cplusplus)
}
#endif
