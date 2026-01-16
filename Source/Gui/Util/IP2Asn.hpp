/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <memory>

#include "Singleton.hpp"
#include "IP2Asn/IP2AsnDB.hpp"

class WIP2Asn : public TSingleton<WIP2Asn>
{
	bool bHaveDatabaseDownloaded{ false };

	std::unique_ptr<WIP2AsnDB> Database{};

public:
	void Init();
};
