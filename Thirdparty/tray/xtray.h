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

enum XTrayLogLevel
{
  XTrayLogLevelError = 0,
  XTrayLogLevelWarning = 1, // Unused for now
  XTrayLogLevelInfo = 2, // Unused for now
  XTrayLogLevelDebug = 3
};

typedef void (*XTrayCallback)(int, int, enum XTrayButton, void*);
typedef void (*XTrayLogCallback)(enum XTrayLogLevel, const char*, void*);

struct XTrayInitConfig
{
  unsigned char const* IconData;
  unsigned int IconDataSize;
  unsigned char IconSize;
  const char* IconName;
  XTrayCallback Callback;
  void* CallbackData;
  XTrayLogCallback LogCallback;
  void* LogCallbackData;
};

int XTrayInit(struct XTrayInitConfig Config);

void XTrayEventLoop();

void XTrayCleanup();

void XTraySetBackgroundColor(unsigned char R, unsigned char G, unsigned char B);

void XTraySetVisible(int bVisible);

#if defined(__cplusplus)
}
#endif
