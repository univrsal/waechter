/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "IPAddress.hpp"

struct WResolveRequest
{
	WIPAddress AddressToResolve;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(AddressToResolve);
	}
};

struct WResolveResponse
{
	WIPAddress  AddressToResolve;
	std::string ResolveResult;

	template <class Archive>
	void serialize(Archive& archive)
	{
		archive(AddressToResolve, ResolveResult);
	}
};
