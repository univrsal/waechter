/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Util/ImageUtil.hpp"

class WRegisterDialog
{
	bool bVisible{};

	char Username[15]{};
	char SerialKey[40]{};

	bool bValid{};
	bool bFailedActivation{};

	WTexture Watermark{};

public:
	void Init();
	void Unload() { Watermark.Unload(); }
	void Draw();
	void Show() { bVisible = true; }

	void Validate();

	[[nodiscard]] bool IsRegistered() const { return bValid; }
};
