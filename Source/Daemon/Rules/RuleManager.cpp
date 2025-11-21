//
// Created by usr on 20/11/2025.
//

#include "RuleManager.hpp"

#include "Data/RuleUpdate.hpp"
#include "cereal/archives/binary.hpp"
#include "spdlog/spdlog.h"

void WRuleManager::HandleRuleChange(WBuffer const& Buf)
{
	std::lock_guard   Lock(Mutex);
	WRuleUpdate       Update{};
	std::stringstream ss;
	ss.write(Buf.GetData(), static_cast<long int>(Buf.GetWritePos()));
	{
		ss.seekg(1); // Skip message type
		cereal::BinaryInputArchive iar(ss);
		iar(Update);
	}
	spdlog::info("Update for {}, bDownloadBlocked={}, bUploadBlocked={}", Update.TrafficItemId,
		Update.Rules.bDownloadBlocked, Update.Rules.bUploadBlocked);
}