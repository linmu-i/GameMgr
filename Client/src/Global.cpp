#include <Global.h>

namespace global
{
	ebbglow::rsc::SharedFile& GetFontData()
	{
		static ebbglow::rsc::SharedFile fontData;
		return fontData;
	}

	std::atomic<bool>& SyncFlag()
	{
		static std::atomic<bool> syncFlag{ false };
		return syncFlag;
	}
	core::CommandResult& SyncCmd()
	{
		static core::CommandResult syncCmd{ std::make_shared<std::atomic<core::CommandStatus>>(core::CommandStatus::Success) };
		return syncCmd;
	}

	std::pair<std::filesystem::path, bool>& StartGameFlag()
	{
		static std::pair<std::filesystem::path, bool> startGameFlag{ {}, false };
		return startGameFlag;
	}

	std::string& WaitingGameName()
	{
		static std::string waitingGameName;
		return waitingGameName;
	}

	std::atomic<bool>& ServerInfoRebuildFlag()
	{
		static std::atomic<bool> rebuildFlag{ false };
		return rebuildFlag;
	}
}