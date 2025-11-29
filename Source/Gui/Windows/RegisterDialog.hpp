//
// Created by usr on 29/11/2025.
//

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

	bool IsRegistered() const { return bValid; }
};
