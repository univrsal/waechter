/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

int xtray_init(unsigned char const* image_data, unsigned int image_data_size);

void xtray_poll();

void xtray_cleanup();

#if defined(__cplusplus)
}
#endif
