/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <sigslot/signal.hpp>

#include "Singleton.hpp"

struct WSocketCounter;
struct WProcessCounter;

class WNetworkEvents final : public TSingleton<WNetworkEvents>
{
public:
	sigslot::signal<std::shared_ptr<WSocketCounter>>         OnSocketCreated;
	sigslot::signal<std::shared_ptr<WSocketCounter> const&>  OnSocketRemoved;
	sigslot::signal<std::shared_ptr<WProcessCounter> const&> OnProcessRemoved;
};