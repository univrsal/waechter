//
// Created by usr on 22/11/2025.
//

#pragma once
#include <sigslot/signal.hpp>

#include "Singleton.hpp"

struct ITrafficItem;

class WNetworkEvents final : public TSingleton<WNetworkEvents>
{
public:
	sigslot::signal<std::shared_ptr<ITrafficItem>> OnSocketCreated;
};