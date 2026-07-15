#include <fstream>

#include <Log.h>
#include <Sync.h>
#include <Global.h>

namespace core
{
	std::optional<std::pair<Command, CommandResult>> CommandManager::getCmd()
	{
		std::lock_guard lock{ *mutex };
		if (commandQueue.empty()) return std::nullopt;
		auto result = std::move(commandQueue.front());
		commandQueue.pop_front();
		return result;
	}

	CommandResult CommandManager::pushCmd(Command cmd)
	{
		if (!isRunning)
		{
			return { std::make_shared<std::atomic<CommandStatus>>(CommandStatus::Error) };
		}
		CommandResult result = { std::make_shared<std::atomic<CommandStatus>>(CommandStatus::Waiting) };
		{
			std::lock_guard lock{ *mutex };
			commandQueue.emplace_back(cmd, result);
		}
		return result;
	}

	void CommandManager::update()
	{
		if (!isRunning)
		{
			for (auto& status : commandQueue)
			{
				if (!status.second.success()) status.second.status->store(CommandStatus::Error);
			}
		}
	}

	enum class SyncState
	{
		Idle,
		Waiting,
		Error
	};

	struct RecvContext
	{
		CommandResult cmdResult;
		uint32_t receivedCount = 0;
		data::FileInfo info;
		std::ofstream fileOfs;

		RecvContext() : cmdResult{}, receivedCount{ 0 }, info{}, fileOfs{} {}

		void clear()
		{
			cmdResult = {};
			receivedCount = 0;
			info = {};
			fileOfs.close();
			fileOfs.clear();
		}
	};

	void SyncMain(cfg::Config& config, SyncContext& context, CommandManager& cmdMgr)
	{
		tideecho::TCPClient client{ context.serverEndpoint };

		if (!client.valid() || client.status() == tideecho::TCPStreamStatus::Idle)
		{
			mgrLog::PrintLog(mgrLog::LogLevel::Error, "Client connection error. Server: {}", context.serverEndpoint.toString());
			context.errorMessage = u8"Client connection error.";
		}

		global::ServerInfoRebuildFlag() = true;

		SyncState state = SyncState::Idle;

		RecvContext recvContext;

		std::optional<CommandResult> cmdResult;

		while (context.isRunning)
		{
			

			if (!client.valid() || client.status() == tideecho::TCPStreamStatus::Idle)
			{
				mgrLog::PrintLog(mgrLog::LogLevel::Error, "Client connection error. Server: {}", context.serverEndpoint.toString());
				context.errorMessage = u8"Client connection error.";
				state = SyncState::Error;
			}

			client.update();

			if (state == SyncState::Idle)
			{
				auto cmdOpt = cmdMgr.getCmd();
				if (cmdOpt)
				{
					auto [cmd, result] = std::move(*cmdOpt);
					if (cmd == Command::Sync)
					{
						result.status->store(CommandStatus::Processing);
						cmdResult = result;
						auto tablePkg = data::MakeTableData(context.table);
						client.asyncSend(tablePkg);
						state = SyncState::Waiting;
					}
					else if (cmd == Command::Exit)
					{
						result.status->store(CommandStatus::Success);
						context.isRunning = false;
					}
				}
			}
			else if (state == SyncState::Waiting)
			{
				auto pkgOpt = client.getPackage();
				if (!pkgOpt)
				{
					continue;
				}
				auto dataType = data::GetDataType(*pkgOpt);
				if (dataType == data::DataType::TableData)
				{
					auto tableOpt = data::TableDataGetTable(*pkgOpt);
					if (tableOpt)
					{
						context.table = std::move(*tableOpt);
						auto tmpTablePath = config.tablePath.parent_path() / (config.tablePath.filename().string() + ".tmp");
						std::filesystem::create_directories(tmpTablePath.parent_path());
						std::ofstream ofs{ tmpTablePath, std::ios::binary };
						if (!type::Serialize(ofs, context.table))
						{
							mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to write table to local file. Server: {}, Path: {}", context.serverEndpoint.toString(), tmpTablePath.string());
							state = SyncState::Error;
							continue;
						}
						ofs.close();
						std::filesystem::rename(tmpTablePath, config.tablePath);
						auto allFiles = context.table.allFiles();

						for (const auto& [vDir, item] : allFiles)
						{
							auto physicalPathOpt = config.vDirToPhysicalPath(vDir);
							if (!physicalPathOpt)
							{
								mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Failed to map virtual directory to physical path. Server: {}, VDir: {}", context.serverEndpoint.toString(), vDir.string());
								continue;
							}
							auto physicalPath = physicalPathOpt.value();

							if (std::filesystem::is_regular_file(physicalPath))
							{
								std::error_code ec;

								// 1. 构造 system_clock 时间点
								std::chrono::sys_time<std::chrono::milliseconds> sys_ts{
									std::chrono::milliseconds(item.time)
								};

								// 2. 使用 clock_cast 通用转换（自动处理 from_sys / from_utc 差异）
								std::filesystem::file_time_type ftime =
									std::chrono::clock_cast<std::filesystem::file_time_type::clock>(sys_ts);

								// 3. 设置文件写入时间
								std::filesystem::last_write_time(physicalPath, ftime, ec);

								if (ec)
								{
									mgrLog::PrintLog(mgrLog::LogLevel::Warning,
										"Failed to set write time for {}. Server: {}, Error: {}",
										physicalPath.string(), context.serverEndpoint.toString(), ec.message());
								}
							}
							else
							{
								mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Physical file does not exist. Server: {}, Path: {}", context.serverEndpoint.toString(), physicalPath.string());
							}
						}

						client.asyncSend(data::MakeACK());
						mgrLog::PrintLog(mgrLog::LogLevel::Info, "Table received and saved. Server: {}, Path: {}", context.serverEndpoint.toString(), config.tablePath.string());
					}
					else
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to parse table data from server. Server: {}", context.serverEndpoint.toString());
						state = SyncState::Error;
					}
				}
				else if (dataType == data::DataType::Error)
				{
					auto reason = data::ErrorGetReason(*pkgOpt);
					mgrLog::PrintLog(mgrLog::LogLevel::Error, "Server returned error. Server: {}, Message: {}", context.serverEndpoint.toString(), reinterpret_cast<const char*>(reason.c_str()));
					context.errorMessage = reason;
					recvContext.clear();
					if (cmdResult)
					{
						cmdResult->status->store(CommandStatus::Error);
					}
					state = SyncState::Idle;
				}
				else if (dataType == data::DataType::ACK)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Info, "Sync succeeded. Server: {}", context.serverEndpoint.toString());
					context.errorMessage.clear();
					if (cmdResult)
					{
						cmdResult->status->store(CommandStatus::Success);
					}
					recvContext.clear();
					state = SyncState::Idle;
				}
				else if (dataType == data::DataType::FileData)
				{
					auto fileInfo = data::FileDataGetInfo(*pkgOpt);
					mgrLog::PrintLog(mgrLog::LogLevel::Debug, "File piece received. Server: {}, File: {}, Piece: {}/{}", context.serverEndpoint.toString(), fileInfo.vDir.string(), fileInfo.index + 1, fileInfo.packageCount);
					if (fileInfo.vDir.empty())
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to parse file info from server. Server: {}", context.serverEndpoint.toString());
						state = SyncState::Error;
						continue;
					}

					if (recvContext.receivedCount > 0 && recvContext.info.vDir != fileInfo.vDir)
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Received file piece for unexpected file. Server: {}, Expected: {}, Received: {}", context.serverEndpoint.toString(), recvContext.info.vDir.string(), fileInfo.vDir.string());
						state = SyncState::Error;
						continue;
					}

					recvContext.info = fileInfo;
					auto physicalPathOpt = config.vDirToPhysicalPath(fileInfo.vDir);

					if (!physicalPathOpt)
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to map virtual directory to physical path. Server: {}, VDir: {}", context.serverEndpoint.toString(), fileInfo.vDir.string());
						state = SyncState::Error;
						continue;
					}

					auto tmpFilePath = physicalPathOpt.value().parent_path() / (physicalPathOpt.value().filename().string() + ".tmp");

					std::filesystem::create_directories(tmpFilePath.parent_path());
					
					if (recvContext.receivedCount == 0)
					{
						recvContext.fileOfs.open(tmpFilePath, std::ios::binary);
					}
					else
					{
						//recvContext.fileOfs.open(tmpFilePath, std::ios::binary | std::ios::app);
					}

					if (!recvContext.fileOfs.is_open())
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to open temporary file for writing. Server: {}, Path: {}", context.serverEndpoint.toString(), tmpFilePath.string());
						state = SyncState::Error;
						continue;
					}

					if (data::WriteFileData(*pkgOpt, recvContext.fileOfs))
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Debug, "File piece received. Server: {}, File: {}, Piece: {}/{}", context.serverEndpoint.toString(), recvContext.info.vDir.string(), recvContext.receivedCount + 1, recvContext.info.packageCount);
					}
					else
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to write file piece to temporary file. Server: {}, File: {} Piece: {}/{}", context.serverEndpoint.toString(), recvContext.info.vDir.string(), recvContext.receivedCount + 1, recvContext.info.packageCount);
						state = SyncState::Error;
						continue;
					}
					recvContext.receivedCount++;
					if (recvContext.receivedCount >= recvContext.info.packageCount)
					{
						recvContext.fileOfs.close();
						auto physicalPath = config.vDirToPhysicalPath(recvContext.info.vDir).value();
						std::filesystem::rename(physicalPath.parent_path() / (physicalPath.filename().string() + ".tmp"), physicalPath);
						mgrLog::PrintLog(mgrLog::LogLevel::Info, "File received. Server: {}, File: {}", context.serverEndpoint.toString(), recvContext.info.vDir.string());
						client.asyncSend(data::MakeACK());
						recvContext.clear();
					}
				}
				else if (dataType == data::DataType::DeleteRequest)
				{
					auto fileVDir = data::DeleteRequestGetVDir(*pkgOpt);
					auto physicalPathOpt = config.vDirToPhysicalPath(fileVDir);
					if (!physicalPathOpt)
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to map virtual directory to physical path for delete request. Server: {}, FileVDir: {}", context.serverEndpoint.toString(), fileVDir.string());
						state = SyncState::Error;
						continue;
					}
					try
					{
						std::filesystem::remove(physicalPathOpt.value());
						mgrLog::PrintLog(mgrLog::LogLevel::Info, "File deleted. Server: {}, FileVDir: {}", context.serverEndpoint.toString(), fileVDir.string());
						client.asyncSend(data::MakeACK());
					}
					catch (const std::filesystem::filesystem_error& e)
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to delete file. Server: {}, FileVDir: {}, Error: {}", context.serverEndpoint.toString(), fileVDir.string(), e.what());
						state = SyncState::Error;
						continue;
					}
				}
				else if (dataType == data::DataType::FileRequest)
				{
					auto fileVDir = data::FileRequestGetVDir(*pkgOpt);
					auto physicalPathOpt = config.vDirToPhysicalPath(fileVDir);
					if (!physicalPathOpt)
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to map virtual directory to physical path for file request. Server: {}, FileVDir: {}", context.serverEndpoint.toString(), fileVDir.string());
						state = SyncState::Error;
						continue;
					}

					uint64_t fileSize = 0;
					try
					{
						fileSize = std::filesystem::file_size(physicalPathOpt.value());
					}
					catch (const std::filesystem::filesystem_error& e)
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to get file size for file request. Server: {}, FileVDir: {}, Error: {}", context.serverEndpoint.toString(), fileVDir.string(), e.what());
						state = SyncState::Error;
						continue;
					}



					data::FileInfo fileInfo = { fileVDir, 0, static_cast<uint16_t>((fileSize + config.maxPackageSize - 1) / config.maxPackageSize), config.maxPackageSize };
					std::ifstream fileIfs{ physicalPathOpt.value(), std::ios::binary };
					if (!fileIfs.is_open())
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to open file for reading in response to file request. Server: {}, FileVDir: {}", context.serverEndpoint.toString(), fileVDir.string());
						state = SyncState::Error;
						continue;
					}
					for (uint16_t i = 0; i < fileInfo.packageCount; ++i)
					{
						fileInfo.index = i;
						auto filePiecePkg = data::MakeFileData(fileInfo, fileIfs);
						client.asyncSend(filePiecePkg);
					}
					mgrLog::PrintLog(mgrLog::LogLevel::Debug, "Server sent file request. Server: {}, FileVDir: {}", context.serverEndpoint.toString(), fileVDir.string());
				}
				else
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Received unexpected data type from server. Server: {}, DataType: {}", context.serverEndpoint.toString(), static_cast<int>(data::GetDataType(*pkgOpt)));
				}


			}
			else if (state == SyncState::Error)
			{
				if (cmdResult)
				{
					cmdResult->status->store(CommandStatus::Error);
				}
				mgrLog::PrintLog(mgrLog::LogLevel::Error, "Sync failed. Server: {}", context.serverEndpoint.toString());
				context.isRunning = false;
				break;
			}

		}

		mgrLog::PrintLog(mgrLog::LogLevel::Info, "Sync thread exiting. Server: {}", context.serverEndpoint.toString());

		global::ServerInfoRebuildFlag() = true;

		context.isRunning = false;
	}
}