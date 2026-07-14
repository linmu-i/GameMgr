#include <algorithm>

#include <Diff.h>


namespace svr
{
	void GetDiff(
		std::vector<DiffItem>& requestList,
		std::vector<DiffItem>& sendList,
		std::vector<std::pair<std::filesystem::path, int64_t>>& deleteList,
		std::vector<std::pair<std::filesystem::path, int64_t>>& deleteSendList,
		type::Table& svrTb, type::Table& cltTb)
	{
		auto cltList = cltTb.allFiles();
		std::vector<DiffItem> svrList;
		for (auto& [game, _] : cltTb.directory())
		{
			auto gameFiles = svrTb.gameFiles(game);
			svrList.insert(svrList.begin(), gameFiles.begin(), gameFiles.end());
		}

		while (!cltList.empty())
		{
			auto svrIt = std::find_if(svrList.begin(), svrList.end(),
				[&cltList](DiffItem& s) { return s.first == cltList.front().first; });
			if (svrIt != svrList.end())
			{
				if (svrIt->second.time > cltList.front().second.time)
				{
					if (svrIt->second.status == type::FileStatus::Deleted)
					{
						deleteSendList.push_back(std::make_pair(svrIt->first, svrIt->second.time));
					}
					else
					{
						sendList.push_back(*svrIt);
					}
				}
				else if (svrIt->second.time < cltList.front().second.time)
				{
					if (cltList.front().second.status == type::FileStatus::Deleted)
					{
						deleteList.push_back(std::make_pair(cltList.front().first, cltList.front().second.time));
					}
					else
					{
						if (cltList.front().second.status != type::FileStatus::Deleted) requestList.push_back(cltList.front());
					}
				}
				svrList.erase(svrIt);
				std::swap(cltList.front(), cltList.back());
				cltList.pop_back();
			}
			else
			{
				if (cltList.front().second.status != type::FileStatus::Deleted) requestList.push_back(cltList.front());
				std::swap(cltList.front(), cltList.back());
				cltList.pop_back();
			}
		}
		for (auto& s : svrList)
		{
			sendList.push_back(s);
		}
	}
}