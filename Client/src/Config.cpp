#include <Config.h>
#include <mINI/ini.h>
#include <Log.h>
#include <Type.h>

namespace cfg
{
	Config ReadConfig(const std::filesystem::path& configPath)
	{
		Config result;
		mINI::INIFile iniFile{ configPath };
		mINI::INIStructure cfg;
		iniFile.read(cfg);

		result.tablePath = DefaultConfig::tablePath;
		result.maxPackageSize = DefaultConfig::maxPackageSize;
		result.winSize = DefaultConfig::winSize;
		
		if (cfg.has("Core"))
		{
			if (cfg["Core"].has("TablePath"))
			{
				auto tbPathStr = cfg["Core"].get("TablePath");
				result.tablePath = reinterpret_cast<const char8_t*>(tbPathStr.c_str());
			}
			if (cfg["Core"].has("MaxPackageSize"))
			{
				auto maxPkgSizeStr = cfg["Core"].get("MaxPackageSize");
				try
				{
					result.maxPackageSize = std::stoul(maxPkgSizeStr);
				}
				catch (...)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Invalid MaxPackageSize value: {}", maxPkgSizeStr);
				}
			}
			if (cfg["Core"].has("WinWidth") && cfg["Core"].has("WinHeight"))
			{
				auto winWidthStr = cfg["Core"].get("WinWidth");
				auto winHeightStr = cfg["Core"].get("WinHeight");
				try
				{
					float width = std::stof(winWidthStr);
					float height = std::stof(winHeightStr);
					result.winSize = ebbglow::Vec2{ width, height };
				}
				catch (...)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Invalid WinWidth or WinHeight value: {}x{}", winWidthStr, winHeightStr);
				}
			}
		}

		for (auto& item : cfg)
		{
			if (item.second.has("GameExe") || item.second.has("SaveDir"))
			{	
				GameConfig gameCfg;
				auto gameExeStr = item.second.get("GameExe");
				gameCfg.gameExe = reinterpret_cast<const char8_t*>(gameExeStr.c_str());
				auto saveDirStr = item.second.get("SaveDir");
				gameCfg.saveDir = reinterpret_cast<const char8_t*>(saveDirStr.c_str());
				result.gameConfigs.emplace(reinterpret_cast<const char8_t*>(item.first.c_str()), std::move(gameCfg));
			}
		}
		return result;
	}

	bool WriteConfig(const std::filesystem::path& configPath, const Config& config)
	{
		mINI::INIFile iniFile{ configPath };
		mINI::INIStructure cfg;
		cfg["Core"]["TablePath"] = reinterpret_cast<const char*>(config.tablePath.u8string().c_str());
		cfg["Core"]["MaxPackageSize"] = std::to_string(config.maxPackageSize);
		cfg["Core"]["WinWidth"] = std::to_string(config.winSize.x);
		cfg["Core"]["WinHeight"] = std::to_string(config.winSize.y);
		for (const auto& [gameName, gameCfg] : config.gameConfigs)
		{
			std::string sectionName = reinterpret_cast<const char*>(gameName.u8string().c_str());
			cfg[sectionName]["GameExe"] = reinterpret_cast<const char*>(gameCfg.gameExe.u8string().c_str());
			cfg[sectionName]["SaveDir"] = reinterpret_cast<const char*>(gameCfg.saveDir.u8string().c_str());
		}
		return iniFile.generate(cfg, true);
	}

	type::Table RebuildTable(const Config& config)
	{
		auto table = type::Table{};
		if (std::filesystem::exists(config.tablePath))
		{
			std::ifstream ifs{ config.tablePath, std::ios::binary };

			if (!type::Deserialize(ifs, table))
			{
				mgrLog::PrintLog(mgrLog::LogLevel::Error, "Failed to read table from file. Path: {}", config.tablePath.string());
				return {};
			}
			
		}

		std::vector<std::filesystem::path> removeSaveDirs;

		for (const auto& [gameName, _] : table.directory())
		{
			removeSaveDirs.push_back(gameName);
		}

		for (auto& game : config.gameConfigs)
		{
			auto saveDir = game.second.saveDir;
			removeSaveDirs.erase(std::remove(removeSaveDirs.begin(), removeSaveDirs.end(), game.first), removeSaveDirs.end());
			auto gameFiles = table.gameFiles(game.first);

			for (const auto& entry : std::filesystem::recursive_directory_iterator(saveDir))
			{
				if (entry.is_regular_file())
				{
					auto filePath = std::filesystem::relative(entry.path(), saveDir);
					auto vDir = game.first / filePath;

					auto it = std::find_if(gameFiles.begin(), gameFiles.end(), [&](const auto& file) {
						return file.first == vDir;
					});
					if (it != gameFiles.end())
					{
						if (entry.last_write_time() > std::filesystem::file_time_type::clock::time_point(std::chrono::milliseconds(it->second.time)))
						{
							table.directory()[game.first][filePath] = type::TableItem{ std::chrono::duration_cast<std::chrono::milliseconds>(entry.last_write_time().time_since_epoch()).count(), type::FileStatus::Changed };
						}
						gameFiles.erase(it);
					}
					else
					{
						table.directory()[game.first][filePath] = type::TableItem{ std::chrono::duration_cast<std::chrono::milliseconds>(entry.last_write_time().time_since_epoch()).count(), type::FileStatus::Created };
					}
					
				}
			}

			for (auto& file : gameFiles)
			{
				table.directory()[game.first][type::GetFilePathFromVDir(file.first)] =
				{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
					type::FileStatus::Deleted };
			}
		}

		for (const auto& saveDir : removeSaveDirs)
		{
			table.directory().erase(saveDir);
		}

		
		
		return table;
	}
}