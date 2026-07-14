#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <format>
#include <chrono>
#include <string>
#include <string_view>

namespace mgrLog
{
	enum class LogLevel : uint8_t
	{
		Debug = 0,
		Info = 1,
		Warning = 2,
		Error = 3
	};

	void SetLogStream(std::shared_ptr<std::ostream> os);

	//不保存os，外部确保生命周期
	void SetLogStream(std::ostream* os);

	std::ostream* GetLogStream();

	void SetLogLevel(LogLevel level);
	LogLevel GetLogLevel();

	template<typename ...Args>
	void PrintLog(LogLevel level, const std::string_view logTextFormat, Args ...args)
	{
		if (static_cast<uint8_t>(level) < static_cast<uint8_t>(GetLogLevel())) return;
		std::string lvStr;
		switch (level)
		{
		case LogLevel::Info:
			lvStr = "[Info]";
			break;
		case LogLevel::Warning:
			lvStr = "[Warning]";
			break;
		case LogLevel::Error:
			lvStr = "[Error]";
			break;
		case LogLevel::Debug:
			lvStr = "[Debug]";
			break;
		default:
			lvStr = "[Unknown]";
		}

		auto time = std::chrono::system_clock::now();
		std::string timeStr;
		auto ms_time = std::chrono::time_point_cast<std::chrono::milliseconds>(time);

		// 转换为东八区（Asia/Shanghai），并格式化为所需字符串
		static const auto* tz = std::chrono::locate_zone("Asia/Shanghai");
		auto zt = std::chrono::zoned_time{ tz, ms_time };
		std::string fmt = "[{:%Y-%m-%dT%H:%M:%S%z}]";
		timeStr = std::vformat(fmt, std::make_format_args(zt));

		auto* os = GetLogStream();
		if (os)
		{
			std::string logText;
			try
			{
				logText = std::vformat(logTextFormat, std::make_format_args(args...));
			}
			catch (...)
			{
				logText = std::string("Log formatting error.") + std::string(logTextFormat);
			}
			(*os) << lvStr << ' ' << timeStr << ' ' << logText << std::endl;
		}
	}
}