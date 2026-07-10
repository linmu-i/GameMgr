#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <format>
#include <chrono>
#include <string>
#include <string_view>

namespace svr
{
	enum class LogLevel : uint8_t
	{
		Info,
		Warning,
		Error,
		Debug
	};

	void SetLogStream(std::shared_ptr<std::ostream> os);

	//不保存os，外部确保生命周期
	void SetLogStream(std::ostream* os);

	std::ostream* GetLogStream();

	template<typename ...Args>
	void PrintLog(LogLevel level, const std::string_view logTextFormat, Args ...args)
	{
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
		timeStr = std::format("[{:%Y-%m-%dT%H:%M:%S%z}]",
			std::chrono::zoned_time{ "Asia/Shanghai", ms_time });

		auto* os = GetLogStream();
		if (os)
		{
			std::string logText = std::vformat(logTextFormat, std::make_format_args(args...));
			(*os) << lvStr << ' ' << timeStr << ' ' << logText << std::endl;
		}
	}
}