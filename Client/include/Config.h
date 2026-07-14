#pragma once

#include <filesystem>
#include <unordered_map>
#include <functional>

#include <EbbGlow/Utils/Types.h>
#include <DataType.h>

namespace cfg
{
	struct GameConfig
	{
		std::filesystem::path gameExe;
		std::filesystem::path saveDir;
	};

	using GameConfigList = std::unordered_map<std::filesystem::path, GameConfig>;

	struct Config
	{
		std::filesystem::path tablePath;
		GameConfigList gameConfigs;
		uint32_t maxPackageSize = 1024 * 128;
		ebbglow::Vec2 winSize{ 800.0f, 600.0f };
		tideecho::NetEndpoint serverEndpoint{ "127.0.0.1", 34184, tideecho::AddressFamily::IPv4 };

		std::function<std::optional<std::filesystem::path>(const std::filesystem::path&)> vDirToPhysicalPath;
	};

	struct DefaultConfig
	{
		static constexpr const std::u8string_view tablePath = u8"table/table.dat";
		static constexpr uint32_t maxPackageSize = 1024 * 128;
		static constexpr ebbglow::Vec2 winSize{ 800.0f, 600.0f };
	};

	Config ReadConfig(const std::filesystem::path& configPath);
	bool WriteConfig(const std::filesystem::path& configPath, const Config& config);

	type::Table RebuildTable(const Config& config);

	struct VDirToPhysicalPath
	{
		const Config& config;

		std::optional<std::filesystem::path> operator()(const std::filesystem::path& vDir) const
		{
			auto it = config.gameConfigs.find(type::GetGameFromVDir(vDir));
			
			if (it != config.gameConfigs.end())
			{
				return it->second.saveDir / type::GetFilePathFromVDir(vDir);
			}
			else
			{
				return std::nullopt;
			}
		}
	};
}