#pragma once

#include <Type.h>
#include <Log.h>
#include <mINI/ini.h>

namespace svr
{
	struct Context
	{
		type::Table table;
		std::filesystem::path tablePath;
		std::filesystem::path dataDir;
		std::filesystem::path tmpDir;
		uint16_t port = 34184;
		uint32_t maxPackageSize = 1024 * 128;
	};

	struct DefaultContext
	{
		static constexpr const std::u8string_view tablePath = u8"table/table.dat";
		static constexpr const std::u8string_view dataDir = u8"data/";
		static constexpr const std::u8string_view tmpDir = u8"tmp/";
		static constexpr uint16_t port = 34184;
	};

	Context ReadContext(std::filesystem::path configPath)
	{
		mINI::INIFile iniFile{ configPath };
		mINI::INIStructure cfg;
		iniFile.read(cfg);

		Context result;
		result.table = {};
		result.dataDir = DefaultContext::dataDir;
		result.tablePath = DefaultContext::tablePath;
		result.tmpDir = DefaultContext::tmpDir;
		result.port = DefaultContext::port;
		if (!cfg.has("Core"))
		{
			return result;
		}
		else
		{
			if (cfg["Core"].has("TablePath"))
			{
				auto tbPathStr = cfg["Core"].get("TablePath");
				result.tablePath = reinterpret_cast<const char8_t*>(tbPathStr.c_str());
				std::ifstream ifs{ result.tablePath, std::ios::binary };
				type::Table tmpTb;
				if (type::Deserialize(ifs, tmpTb))
				{
					result.table = std::move(tmpTb);
				}
				else
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Read table failed. Path: {}", tbPathStr);
				}
			}

			if (cfg["Core"].has("DataDir"))
			{
				auto dataDirStr = cfg["Core"].get("DataDir");
				result.dataDir = reinterpret_cast<const char8_t*>(dataDirStr.c_str());
			}

			if (cfg["Core"].has("TmpDir"))
			{
				auto dataDirStr = cfg["Core"].get("DataDir");
				result.dataDir = reinterpret_cast<const char8_t*>(dataDirStr.c_str());
			}

			if (cfg["Core"].has("Port"))
			{
				auto portStr = cfg["Core"].get("Port");
				try
				{
					result.port = static_cast<uint16_t>(std::stoul(portStr));
				}
				catch (...)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Read port failed. Port: {}", portStr);
				}
			}

			if (cfg["Core"].has("MaxPackageSize"))
			{
				auto maxPackageSizeStr = cfg["Core"].get("MaxPackageSize");
				try
				{
					result.maxPackageSize = static_cast<uint64_t>(std::stoull(maxPackageSizeStr));
				}
				catch (...)
				{
					mgrLog::PrintLog(mgrLog::LogLevel::Warning, "Read max package size failed. MaxPackageSize: {}", maxPackageSizeStr);
				}
			}
		}
	}
}