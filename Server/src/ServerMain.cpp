#include <bitset>

#include <TideEcho.h>
#include <DataType.h>
#include <Context.h>

#include <Diff.h>

enum class ServerState
{
	Idle,
	SendFile,
	SentFileWaitACK,
	RequestFile,
	ReceiveFile,
	SendTable,
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
	svr::SetLogStream(&std::cout);
	svr::PrintLog(svr::LogLevel::Info, "Program started.");
	svr::SetLogLevel(svr::LogLevel::Debug);
	svr::Context context = svr::ReadContext("config/config.ini");
	tideecho::TCPServer svr{ context.port };
	if (svr.valid())
	{
		svr::PrintLog(svr::LogLevel::Info, "Server started. Port: {}", context.port);
	}
	else
	{
		svr::PrintLog(svr::LogLevel::Error, "Server start failed. Port: {}", context.port);
		return -1;
	}

	std::deque<tideecho::NetPackage> activeQueue;
	tideecho::NetEndpoint activeRemote;
	type::Table clientTable;

	SendContext sendContext;
	RecvContext recvContext;

	std::vector<svr::DiffItem> requestList;
	std::vector<svr::DiffItem> sendList;

	ServerState state = ServerState::Idle;

	while (svr.valid())
	{
		svr.update();

		do
		{
			auto pkg = svr.getPackage();
			if (!pkg) break;

			if (data::GetDataType(pkg->data) == data::DataType::TableRequest)
			{
				if (state == ServerState::Idle)
				{
					auto tableTmp = data::TableDataGetTable(pkg->data);
					if (tableTmp)
					{
						clientTable = std::move(*tableTmp);
						activeRemote = pkg->remote;

						svr::GetDiff(requestList, sendList, context.table, clientTable);
						state = ServerState::SendFile;
						svr::PrintLog(svr::LogLevel::Info, "Sync started. Remote: {}", pkg->remote.toString());
					}
					else
					{
						svr::PrintLog(svr::LogLevel::Warning, "Invalid table data. Remote: {}", pkg->remote.toString());
						svr.asyncSend(data::MakeError(u8"Invalid table data."), pkg->remote);
					}
				}
				else
				{
					svr::PrintLog(svr::LogLevel::Warning, "Sync request rejected. Another sync in progress. Remote: {}", pkg->remote.toString());
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
					svr::PrintLog(svr::LogLevel::Warning, "Unknown packet. Remote: {}", pkg->remote.toString());
					svr.asyncSend(data::MakeError(u8"Packet should not be sent."), pkg->remote);
				}
			}
		} while (0);//数据包路由，独占处理，任何冲突请求返回Error


		if (activeRemote.valid())
		{
			auto remoteStatus = svr.remoteStatus(activeRemote);
			if (!remoteStatus || *remoteStatus == tideecho::TCPStreamStatus::Error)
			{
				svr::PrintLog(svr::LogLevel::Warning, "Remote disconnected. Remote: {}", activeRemote.toString());
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
					state = ServerState::RequestFile;
					svr::PrintLog(svr::LogLevel::Info, "Sync finished. Remote: {}", activeRemote.toString());
				}
			}
			else if (!sendContext.sending)
			{
				sendContext.update(sendList.back(), context);
				sendContext.sending = true;
			}
			else if (sendContext.info.index > sendContext.info.packageCount)
			{
				if (sendContext.allSent())
				{
					sendList.pop_back();
					sendContext.fileIfs.close();
					sendContext.sendResults.clear();
					sendContext.sending = false;
					state = ServerState::SentFileWaitACK;
					svr::PrintLog(svr::LogLevel::Debug, "File sent. Remote: {}, File: {}", activeRemote.toString(), sendContext.info.vDir.string());
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
					svr::PrintLog(svr::LogLevel::Error, "Failed to read file piece. Remote: {}, File: {}", activeRemote.toString(), sendContext.info.vDir.string());
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
					svr::PrintLog(svr::LogLevel::Debug, "Get ACK. Remote: {}", activeRemote.toString());
					state = ServerState::SendFile;
				}
			}
		}
		else if (state == ServerState::RequestFile)
		{
			if (!requestList.empty())
			{
				auto& item = requestList.back();
				svr.asyncSend(data::MakeFileRequest(item.first), activeRemote);
				recvContext.update({ item.first, 0, 0, context.maxPackageSize }, context);
				state = ServerState::ReceiveFile;
				svr::PrintLog(svr::LogLevel::Debug, "Request file. Remote: {}, File: {}", activeRemote.toString(), item.first.string());
			}
			else
			{
				state = ServerState::SendTable;
				svr::PrintLog(svr::LogLevel::Debug, "All files recved. Remote: {}", activeRemote.toString());
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
						svr::PrintLog(svr::LogLevel::Debug, "File piece received. Remote: {}, File: {}, Piece: {}/{}", activeRemote.toString(), recvContext.info.vDir.string(), recvContext.receivedCount, recvContext.info.packageCount);
						if (recvContext.receivedCount >= data::FilePieceRequestGetInfo(pkg.data).packageCount)
						{
							recvContext.fileOfs.close();
							std::filesystem::rename(context.tmpDir / recvContext.info.vDir, context.dataDir / recvContext.info.vDir);

							auto game = type::GetGameFromVDir(recvContext.info.vDir);
							auto filePath = type::GetFilePathFromVDir(recvContext.info.vDir);
							context.table.directory()[game][filePath] = { requestList.back().second };
							auto tmpTablePath = context.tablePath.parent_path() / (context.tablePath.filename().string() + ".tmp");
							std::ofstream ofs{ tmpTablePath, std::ios::binary};
							if (type::Serialize(ofs, context.table))
							{
								std::filesystem::rename(tmpTablePath, context.tablePath);
							}
							else
							{
								svr::PrintLog(svr::LogLevel::Error, "Failed to write table. Remote: {}, File: {}", activeRemote.toString(), recvContext.info.vDir.string());
								svr.asyncSend(data::MakeError(u8"Failed to write table."), activeRemote);
								state = ServerState::Error;
							}

							requestList.pop_back();
							state = ServerState::RequestFile;
							svr::PrintLog(svr::LogLevel::Info, "File received. Remote: {}, File: {}", activeRemote.toString(), recvContext.info.vDir.string());
						}
					}
					else
					{
						svr::PrintLog(svr::LogLevel::Error, "Failed to write file piece. Remote: {}, File: {}", activeRemote.toString(), recvContext.info.vDir.string());
						svr.asyncSend(data::MakeError(u8"Failed to write file piece."), activeRemote);
						state = ServerState::Error;
					}
				}
			}
		}
		else if (state == ServerState::SendTable)
		{
			svr.asyncSend(data::MakeTableData(context.table), activeRemote);
			state = ServerState::Idle;
			svr::PrintLog(svr::LogLevel::Info, "Table sent. Remote: {}", activeRemote.toString());
		}
		else if (state == ServerState::Error)
		{
			activeRemote = {};
			activeQueue.clear();
			requestList.clear();
			sendList.clear();
			sendContext = SendContext{};
			state = ServerState::Idle;
			svr::PrintLog(svr::LogLevel::Warning, "Sync aborted due to error.");
		}


	}
}