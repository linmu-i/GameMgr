#pragma once

#include <atomic>
#include <mutex>

#include <TideEcho.h>

#include <DataType.h>
#include <Config.h>

namespace core
{
	struct SyncContext
	{
		std::atomic<bool> isRunning = true;
		//std::unique_ptr<std::mutex> mutex = std::make_unique<std::mutex>();
		std::u8string errorMessage;

		type::Table table;

		tideecho::NetEndpoint serverEndpoint;
	};

	enum class Command
	{
		Sync = 0x0,
		Exit = 0xff
	};

	enum class CommandStatus : uint8_t
	{
		Success = 0x0,
		Processing = 0x1,
		Waiting = 0x2,
		Error = 0xff
	};

	struct CommandResult
	{
		std::shared_ptr<std::atomic<CommandStatus>> status = std::make_shared<std::atomic<CommandStatus>>(CommandStatus::Error);
		bool success() const { return *status == CommandStatus::Success; }
		bool processing() const { return *status == CommandStatus::Processing; }
		bool waiting() const { return *status == CommandStatus::Waiting; }
		bool error() const { return *status == CommandStatus::Error; }
	};

	class CommandManager
	{
	private:
		std::deque<std::pair<Command, CommandResult>> commandQueue;
		std::unique_ptr<std::mutex> mutex = std::make_unique<std::mutex>();

	public:
		std::optional<std::pair<Command, CommandResult>> getCmd();
		CommandResult pushCmd(Command cmd);
	};

	void SyncMain(cfg::Config& config, SyncContext& context, CommandManager& cmdMgr);
}