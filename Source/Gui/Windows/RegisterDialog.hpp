//
// Created by usr on 29/11/2025.
//

#pragma once

class WRegisterDialog
{
	bool bVisible{};

	char Username[15]{};
	char SerialKey[40]{};

	bool bValid{};
	bool bFailedActivation{};

public:
	void Init();
	void Draw();
	void Show() { bVisible = true; }

	void Validate();

	bool IsRegistered() const { return bValid; }
};
