//
// Created by usr on 27/10/2025.
//

#pragma once

class WAboutDialog
{
	bool bVisible{};

public:
	void Draw();

	void Show()
	{
		bVisible = true;
	}
};
