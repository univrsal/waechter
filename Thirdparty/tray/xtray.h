/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

enum XTrayButton
{
    XTrayButtonLeft = 1,
    XTrayButtonMiddle = 2,
    XTrayButtonRight = 3,
    XTrayButton4 = 4,
    XTrayButton5 = 5
};

typedef void (*XTrayCallback)(int, int, enum XTrayButton, void*);

struct XTrayInitConfig
{
  unsigned char const* IconData;
  unsigned int IconDataSize;
  unsigned char IconSize;
  const char* IconName;
  XTrayCallback Callback;
  void* CallbackData;
};

int XTrayInit(struct XTrayInitConfig Config);

void XTrayEventLoop();

void XTrayCleanup();

void XTraySetBackgroundColor(unsigned char R, unsigned char G, unsigned char B);

#if defined(__cplusplus)
}
#endif
