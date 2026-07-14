#include <bitset>

#include <TideEcho.h>
#include <DataType.h>
#include <Context.h>
#include <Log.h>

#include <Diff.h>

enum class ServerState
{
	Idle,
	SendFile,
	SentFileWaitACK,
	SendDelete,
	SentDeleteWaitACK,
	RequestFile,
	ReceiveFile,
	SendTable,
	SendTableWaitACK,
	Error
};

struct SendContext
{
	data::FileInfo info;
	std::ifstream fileIfs;
	std::vector<tideecho::AsyncSendResult> sendResults;
	bool sending = false;

	bool allSent() const
	{
		for (auto& item : sendResults)
		{
			if (!item.sent())
			{ 
				return false;
			}
		}
		return true;
	}

	SendContext() : info{}, fileIfs{}, sendResults{} {}

	void update(const svr::DiffItem& item, const svr::Context& context)
	{
		info.vDir = item.first;
		info.index = 0;
		info.packageCount = static_cast<uint16_t>((item.second.time + context.maxPackageSize - 1) / context.maxPackageSize);
		info.maxPackageSize = context.maxPackageSize;
		fileIfs.close();
		fileIfs.clear();
		fileIfs.open(context.dataDir / info.vDir, std::ios::binary);
	}

	void clear()
	{
		info = {};
		fileIfs.close();
		fileIfs.clear();
		sendResults.clear();
		sending = false;
	}
};

struct RecvContext
{
	data::FileInfo info;
	std::ofstream fileOfs;
	uint16_t receivedCount = 0;
	RecvContext() : info{}, fileOfs{}, receivedCount{ 0 } {}
	void update(const data::FileInfo& newInfo, const svr::Context& context)
	{
		info = newInfo;
		receivedCount = 0;
		fileOfs.close();
		fileOfs.clear();
		fileOfs.open(context.tmpDir / info.vDir, std::ios::binary);
	}

	void clear()
	{
		info = {};
		fileOfs.close();
		fileOfs.clear();
		receivedCount = 0;
	}
};

int main()
{
	tideecho::NetServiceGuard guard;
	//mgrLog::SetLogStream(&std::cout);
	mgrLog::PrintLog(mgrLog::LogLevel::Info, "Program started.");
	mgrLog::SetLogLevel(mgrLog::LogLevel::Debug);
	svr::Context context = svr::ReadContext("config/config.ini");
	tideecho::TCPServer svr{ context.port };
	if (svr.valid())
	{
		mgrLog::PrintLog(mgrLog::LogLevel::Info, "Server started. Port: {}", context.port);
	}
	else
	{
		mgrLog::PrintLog(mgrLog::LogLevel::Error, "Server start failed. Port: {}", context.port);
		return -1;
	}

	std::deque<tideecho::NetPackage> activeQueue;
	tideecho::NetEndpoint activeRemote;
	type::Table clientTable;

	SendContext sendContext;
	RecvContext recvContext;

	std::vector<svr::DiffItem> requestList;
	std::vector<svr::DiffItem> sendList;
	std::vector<std::pair<std::filesystem::path, int64_t>> deleteList;
	std::vector<std::pair<std::filesystem::path, int64_t>> deleteSendList;

	ServerState state = ServerState::Idle;

	while (svr.valid())
	{
		svr.update();

		do
		{
			auto pkg = svr.getPackage();
			if (!pkg) break;

			if (data::GetDataType(pkg->data) == data::DataType::TableData)
			{
				if (state == ServerState::Idle)
				{
					auto tableTmp = data::TableDataGetTable(pkg->data);
					if (tableTmp)
					{
						clientTable = std::move(*tableTmp);
						activeRemote = pkg->remote;

						svr::GetDiff(requestList, sendList, deleteList, deleteSendList, context.table, clientTable);

						for (auto& item : sendList)
						{
							auto game = type::GetGameFromVDir(item.first);
							auto filePath = type::GetFilePathFromVDir(item.first);
							clientTable.directory()[game][filePath] = item.second;
						}
						for (auto& item : deleteSendList)
						{
							auto game = type::GetGameFromVDir(item.first);
							auto filePath = type::GetFilePathFromVDir(item.first);
							clientTable.directory()[game][filePath] = { item.second, type::FileStatus::Deleted };
						}

						auto deletedTable = context.table;
						for (auto& item : deleteList)
						{
							auto phyPath = context.dataDir / item.first;
							if (std::filesystem::exists(phyPath))
							{
								std::error_code ec;
								std::filesystem::remove(phyPath, ec);
								if (ec)
								{
									mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Failed to delete file. Path: {}, Error: {}", phyPath.string(), ec.message());
								}
							}
							auto game = type::GetGameFromVDir(item.first);
							auto filePath = type::GetFilePathFromVDir(item.first);
							deletedTable.directory()[game].erase(filePath);
						}
						std::ofstream ofs{ context.tablePath.parent_path() / (context.tablePath.filename().string() + ".tmp"), std::ios::binary};
						if (!type::Serialize(ofs, deletedTable))
						{
							mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to write table to local file. Path: {}", context.tablePath.string());
							svr.asyncSend(data::MakeError(u8"Failed to write table to local file."), pkg->remote);
							state = ServerState::Error;
							break;
						}
						ofs.close();
						std::filesystem::rename(context.tablePath.parent_path() / (context.tablePath.filename().string() + ".tmp"), context.tablePath);

						state = ServerState::SendFile;
						mgrLog::PrintLog(mgrLog::LogLevel::Info, "Sync started. Remote: {}", pkg->remote.toString());
					}
					else
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Invalid table data. Remote: {}", pkg->remote.toString());
						svr.asyncSend(data::MakeError(u8"Invalid table data."), pkg->remote);
					}
				}
				else
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Sync request rejected. Another sync in progress. Remote: {}", pkg->remote.toString());
					svr.asyncSend(data::MakeError(u8"Another sync in progress."), pkg->remote);
				}
			}
			else
			{
				if (pkg->remote == activeRemote)
				{
					activeQueue.push_back(std::move(*pkg));
				}
				else
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Unknown packet. Remote: {}", pkg->remote.toString());
					svr.asyncSend(data::MakeError(u8"Packet should not be sent."), pkg->remote);
				}
			}
		} while (0);//数据包路由，独占处理，任何冲突请求返回Error


		if (activeRemote.valid())
		{
			auto remoteStatus = svr.remoteStatus(activeRemote);
			if (!remoteStatus || *remoteStatus == tideecho::TCPStreamStatus::Error)
			{
				mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Remote disconnected. Remote: {}", activeRemote.toString());
				activeRemote = {};
				activeQueue.clear();
				requestList.clear();
				sendList.clear();
				sendContext = SendContext{};
				state = ServerState::Idle;
			}
		}


		if (state == ServerState::SendFile)
		{
			if (sendList.empty())
			{
				bool allSent = true;
				for (auto& item : sendContext.sendResults)
				{
					if (!item.sent())
					{
						allSent = false;
						break;
					}
				}
				if (allSent)
				{
					state = ServerState::SendDelete;
					mgrLog::PrintLog(mgrLog::LogLevel::Info, "Sync finished. Remote: {}", activeRemote.toString());
				}
			}
			else if (!sendContext.sending)
			{
				sendContext.update(sendList.back(), context);
				sendContext.sending = true;
			}
			else if (sendContext.info.index >= sendContext.info.packageCount)
			{
				if (sendContext.allSent())
				{
					sendList.pop_back();
					sendContext.fileIfs.close();
					sendContext.sendResults.clear();
					sendContext.sending = false;
					state = ServerState::SentFileWaitACK;
					mgrLog::PrintLog(mgrLog::LogLevel::Debug, "File sent. Remote: {}, File: {}", activeRemote.toString(), sendContext.info.vDir.string());
				}
			}
			else
			{
				auto fileData = data::GetFilePiece(sendContext.info, sendContext.fileIfs);
				if (fileData)
				{
					auto result = svr.asyncSend(data::MakeFileData(sendContext.info, *fileData), activeRemote);
					sendContext.sendResults.push_back(std::move(result));
					sendContext.info.index++;
				}
				else
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to read file piece. Remote: {}, File: {}", activeRemote.toString(), sendContext.info.vDir.string());
					svr.asyncSend(data::MakeError(u8"Failed to read file piece."), activeRemote);
					state = ServerState::Error;
				}
			}
		}
		else if (state == ServerState::SentFileWaitACK)
		{
			if (!activeQueue.empty())
			{
				auto pkg = std::move(activeQueue.front());
				activeQueue.pop_front();
				if (data::GetDataType(pkg.data) == data::DataType::ACK)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Debug, "Get ACK. Remote: {}", activeRemote.toString());
					state = ServerState::SendFile;
				}
			}
		}
		else if (state == ServerState::SendDelete)
		{
			if (!deleteSendList.empty())
			{
				auto& item = deleteSendList.back();
				svr.asyncSend(data::MakeDeleteRequest(item.first), activeRemote);
				state = ServerState::SentDeleteWaitACK;
				mgrLog::PrintLog(mgrLog::LogLevel::Debug, "Delete request sent. Remote: {}, File: {}", activeRemote.toString(), item.first.string());
			}
			else
			{
				state = ServerState::RequestFile;
				mgrLog::PrintLog(mgrLog::LogLevel::Debug, "All delete requests sent. Remote: {}", activeRemote.toString());
			}
		}
		else if (state == ServerState::SentDeleteWaitACK)
		{
			if (!activeQueue.empty())
			{
				auto pkg = std::move(activeQueue.front());
				activeQueue.pop_front();
				if (data::GetDataType(pkg.data) == data::DataType::ACK)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Debug, "Get ACK for delete request. Remote: {}", activeRemote.toString());
					deleteSendList.pop_back();
					state = ServerState::SendDelete;
				}
			}
		}
		else if (state == ServerState::RequestFile)
		{
			if (!requestList.empty())
			{
				auto& item = requestList.back();
				svr.asyncSend(data::MakeFileRequest(item.first), activeRemote);
				std::filesystem::create_directories(context.tmpDir / item.first.parent_path());
				recvContext.update({ item.first, 0, 0, context.maxPackageSize }, context);
				state = ServerState::ReceiveFile;
				mgrLog::PrintLog(mgrLog::LogLevel::Debug, "Request file. Remote: {}, File: {}", activeRemote.toString(), item.first.string());
			}
			else
			{
				state = ServerState::SendTable;
				mgrLog::PrintLog(mgrLog::LogLevel::Debug, "All files recved. Remote: {}", activeRemote.toString());
			}
		}
		else if (state == ServerState::ReceiveFile)
		{
			if (!activeQueue.empty())
			{
				auto pkg = std::move(activeQueue.front());
				activeQueue.pop_front();
				if (data::GetDataType(pkg.data) == data::DataType::FileData)
				{
					if (data::WriteFileData(pkg.data, recvContext.fileOfs))
					{
						recvContext.receivedCount++;
						mgrLog::PrintLog(mgrLog::LogLevel::Debug, "File piece received. Remote: {}, File: {}, Piece: {}/{}", activeRemote.toString(), recvContext.info.vDir.string(), recvContext.receivedCount, recvContext.info.packageCount);
						if (recvContext.receivedCount >= data::FileDataGetInfo(pkg.data).packageCount)
						{
							recvContext.fileOfs.close();
							std::filesystem::create_directories(context.dataDir / recvContext.info.vDir.parent_path());
							std::filesystem::rename(context.tmpDir / recvContext.info.vDir, context.dataDir / recvContext.info.vDir);

							auto targetPath = context.dataDir / recvContext.info.vDir;

							// 1. 检查文件是否存在（可选，但建议保留）
							if (std::filesystem::is_regular_file(targetPath))
							{
								std::error_code ec;

								// 2. 构造系统时钟时间点（毫秒精度）
								std::chrono::sys_time<std::chrono::milliseconds> sys_ts{
									std::chrono::milliseconds(requestList.back().second.time)
								};

								// 3. 使用 clock_cast 转换为文件系统专用时钟（兼容 MSVC / GCC / Clang）
								std::filesystem::file_time_type ftime =
									std::chrono::clock_cast<std::filesystem::file_time_type::clock>(sys_ts);

								// 4. 设置文件的最后写入时间（非抛出版本）
								std::filesystem::last_write_time(targetPath, ftime, ec);

								if (ec)
								{
									mgrLog::PrintLog(mgrLog::LogLevel::Warning,
										"Failed to set write time for {}, Error: {}",
										targetPath.string(), ec.message());
								}
							}
							else
							{
								mgrLog::PrintLog(mgrLog::LogLevel::Warning,
									"Physical file does not exist. Path: {}",
									targetPath.string());
							}

							auto game = type::GetGameFromVDir(recvContext.info.vDir);
							auto filePath = type::GetFilePathFromVDir(recvContext.info.vDir);
							context.table.directory()[game][filePath] = { requestList.back().second };
							auto tmpTablePath = context.tablePath.parent_path() / (context.tablePath.filename().string() + ".tmp");
							std::ofstream ofs{ tmpTablePath, std::ios::binary};
							if (type::Serialize(ofs, context.table))
							{
								ofs.close();
								std::filesystem::rename(tmpTablePath, context.tablePath);
							}
							else
							{
								mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to write table. Remote: {}, File: {}", activeRemote.toString(), recvContext.info.vDir.string());
								svr.asyncSend(data::MakeError(u8"Failed to write table."), activeRemote);
								state = ServerState::Error;
							}

							requestList.pop_back();
							state = ServerState::RequestFile;
							mgrLog::PrintLog(mgrLog::LogLevel::Info, "File received. Remote: {}, File: {}", activeRemote.toString(), recvContext.info.vDir.string());
						}
					}
					else
					{
						mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to write file piece. Remote: {}, File: {}", activeRemote.toString(), recvContext.info.vDir.string());
						svr.asyncSend(data::MakeError(u8"Failed to write file piece."), activeRemote);
						state = ServerState::Error;
					}
				}
			}
		}
		else if (state == ServerState::SendTable)
		{
			svr.asyncSend(data::MakeTableData(clientTable), activeRemote);
			state = ServerState::SendTableWaitACK;
			mgrLog::PrintLog(mgrLog::LogLevel::Info, "Table sent. Remote: {}", activeRemote.toString());
		}
		else if (state == ServerState::SendTableWaitACK)
		{
			if (!activeQueue.empty())
			{
				auto pkg = std::move(activeQueue.front());
				activeQueue.pop_front();
				if (data::GetDataType(pkg.data) == data::DataType::ACK)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Info, "Sync succeeded. Remote: {}", activeRemote.toString());
					svr.asyncSend(data::MakeACK(), activeRemote);
					activeRemote = {};
					activeQueue.clear();
					requestList.clear();
					sendList.clear();
					sendContext = SendContext{};
					state = ServerState::Idle;
				}
			}
		}
		else if (state == ServerState::Error)
		{
			svr.asyncSend(data::MakeError(u8"Sync aborted due to error."), activeRemote);
			activeRemote = {};
			activeQueue.clear();
			requestList.clear();
			sendList.clear();
			sendContext = SendContext{};
			state = ServerState::Idle;
			mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Sync aborted due to error.");
		}


	}
}