/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

template <typename T>
class TSingleton
{
public:
	static T& GetInstance()
	{
		static T Instance{};
		return Instance;
	}

protected:
	TSingleton() = default;
	virtual ~TSingleton() = default;
};
