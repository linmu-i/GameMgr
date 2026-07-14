#include <memory>
#include <Log.h>

namespace mgrLog
{
	struct LogContext
	{
		std::shared_ptr<std::ostream> osData;
		std::ostream* os = nullptr;
	};

	static LogContext InitLogContext()
	{
		LogContext result;
		result.osData = std::static_pointer_cast<std::ostream>(std::make_shared<std::ofstream>("log/log.log"));
		result.os = result.osData.get();
		return result;
	}

	static LogContext& GetLogContext()
	{
		static LogContext context = InitLogContext();
		return context;
	}

	static LogLevel& GetLogLevelData()
	{
		static LogLevel level = LogLevel::Info;
		return level;
	}

	LogLevel GetLogLevel()
	{
		return GetLogLevelData();
	}

	void SetLogStream(std::shared_ptr<std::ostream> os)
	{
		auto& context = GetLogContext();
		context.osData = std::move(os);
		context.os = context.osData.get();
	}

	void SetLogLevel(LogLevel level)
	{
		GetLogLevelData() = level;
	}

	//不保存os，外部确保生命周期
	void SetLogStream(std::ostream* os)
	{
		auto& context = GetLogContext();
		context.os = os;
	}

	std::ostream* GetLogStream()
	{
		return GetLogContext().os;
	}
}