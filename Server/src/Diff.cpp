#include <Diff.h>

namespace svr
{
	void GetDiff(
		std::vector<DiffItem>& requestList,
		std::vector<DiffItem>& sendList,
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
					sendList.push_back(*svrIt);
				}
				else if (svrIt->second.time < cltList.front().second.time)
				{
					requestList.push_back(cltList.front());
				}
				svrList.erase(svrIt);
				std::swap(cltList.front(), cltList.back());
				cltList.pop_back();
			}
			else
			{
				requestList.push_back(cltList.front());
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