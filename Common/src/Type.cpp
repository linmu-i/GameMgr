#include <chrono>

#include <Type.h>

namespace type
{
	std::filesystem::path GetGameFromVDir(const std::filesystem::path& vDir)
	{
		if (vDir.empty()) return {};
		return *vDir.begin();
	}

	std::filesystem::path GetFilePathFromVDir(const std::filesystem::path& vDir)
	{
		if (vDir.empty()) return {};
		auto it = vDir.begin();
		++it;
		std::filesystem::path result;
		for (; it != vDir.end(); ++it)
		{
			result /= *it;
		}
		return result;
	}

	std::optional<TableItem> Table::file(const std::filesystem::path& vDir) { return this->file(GetGameFromVDir(vDir), GetFilePathFromVDir(vDir)); }
	std::optional<TableItem> Table::file(const std::filesystem::path& game, const std::filesystem::path& filePath)
	{
		auto fIt = dat.find(game);
		if (fIt == dat.end()) return std::nullopt;
		auto sIt = fIt->second.find(filePath);
		if (sIt == fIt->second.end()) return std::nullopt;
		return sIt->second;
	}

	std::vector<std::pair<std::filesystem::path, TableItem>> Table::gameFiles(const std::filesystem::path& game)
	{
		auto it = dat.find(game);
		if (it == dat.end()) return {};

		std::vector<std::pair<std::filesystem::path, TableItem>> result;
		result.reserve(it->second.size());
		for (auto& [filePath, item] : it->second)
		{
			result.emplace_back(game / filePath, item);
		}
		return result;
	}
	std::vector<std::pair<std::filesystem::path, TableItem>> Table::allFiles()
	{
		uint64_t size = 0;
		for (auto& m : dat)
		{
			size += m.second.size();
		}
		std::vector<std::pair<std::filesystem::path, TableItem>> result;
		result.reserve(size);

		for (auto& [game, m] : dat)
		{
			for (auto& [filePath, item] : m)
			{
				result.emplace_back(game / filePath, item);
			}
		}
		return result;
	}

	void Table::insert(const std::filesystem::path& vDir, TableItem item)
	{
		this->insert(GetGameFromVDir(vDir), GetFilePathFromVDir(vDir), std::move(item));
	}
	void Table::insert(const std::filesystem::path& game, const std::filesystem::path& filePath, TableItem item)
	{
		dat[game][filePath] = item;
	}

	void Table::remove(const std::filesystem::path& vDir) { this->remove(GetGameFromVDir(vDir), GetFilePathFromVDir(vDir)); }
	void Table::remove(const std::filesystem::path& game, const std::filesystem::path& filePath)
	{
		auto fIt = dat.find(game);
		if (fIt == dat.end()) return;
		auto sIt = fIt->second.find(filePath);
		if (sIt == fIt->second.end()) return;
		fIt->second.erase(sIt);
		if (fIt->second.empty())
		{
			dat.erase(fIt);
		}
	}

	void Table::forcedPush()
	{
		auto timePoint = std::chrono::system_clock::now();
		int64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
		lastForcedPushTime = time;
	}
}