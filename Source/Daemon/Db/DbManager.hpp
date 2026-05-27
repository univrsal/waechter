/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "Singleton.hpp"

class WDbManager final : TSingleton<WDbManager>
{
public:
	void Initialize();
};
