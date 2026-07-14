#pragma once

#include <atomic>

#include <EbbGlow/Utils/Resource.h>
#include <Sync.h>

namespace global
{
	ebbglow::rsc::SharedFile& GetFontData();

	std::atomic<bool>& SyncFlag();
	core::CommandResult& SyncCmd();
	std::pair<std::filesystem::path, bool>& StartGameFlag();
}