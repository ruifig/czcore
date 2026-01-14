#include "Logging.h"
#include "StringUtils.h"
#include "LogOutputs.h"

CZ_DEFINE_LOG_CATEGORY(Main)

namespace cz
{


namespace
{
	struct LogLevelStr
	{
		const char* a;
		const char* b;
	};

	static const LogLevelStr logLevelsStrs[static_cast<int>(LogLevel::VeryVerbose)+1] =
	{
		{"Off", "Off"},
		{"FTL", "Fatal"},
		{"ERR", "Error"},
		{"WRN", "Warning"},
		{"LOG", "Log"},
		{"VER", "Verbose"},
		{"VVE", "VeryVerbose"}
	};
}

bool fromString(std::string_view str, LogLevel& dst)
{
	for(int i = 0; i <= static_cast<int>(LogLevel::VeryVerbose); i++)
	{
		if (asciiStrEqualsCi(str, logLevelsStrs[i].a) || asciiStrEqualsCi(str, logLevelsStrs[i].b))
		{
			dst = static_cast<LogLevel>(i);
			return true;
		}
	}

	return false;
}

namespace details
{

void doDebugBreak()
{
#if defined _WIN32
	__debugbreak();
#elif defined __linux__
	__builtin_trap();
#else
	#error "Uknown or unsupported platform"
#endif
}

/** Don't use this directly. Use the LOG macros */
void logMessage(bool debuggerOutput, LogMessage& msg)
{
	auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::utc_clock::now());
	auto nowSecs = std::chrono::time_point_cast<std::chrono::seconds>(nowMs);
	auto ms = nowMs - nowSecs;
	msg.timestamp = std::format("{:%H:%M:%S}:{:03d}", nowSecs, ms.count());
	msg.formattedMsg = std::format("{}:{}:{}:{}{}\n", msg.timestamp, msg.category->getName(), msg.level, msg.context, msg.msg);

	if (LogOutputs* logs = LogOutputs::tryGet())
	{
		logs->log(debuggerOutput, msg);
	}

	#if TRACY_ENABLE
	if (TracyIsConnected)
	{
		// Picking a colour according to https://ss64.com/nt/syntax-ansi.html
		uint32_t tracyColor = tracy::Color::White;
		switch(msg.level)
		{
			case LogLevel::Fatal:
			case LogLevel::Error:
				tracyColor = 0xc50f1f; // DARK_RED
				break;
			case LogLevel::Warning:
				tracyColor = 0xc19c00; // DARK_YELLOW
				break;
			case LogLevel::Off:
			case LogLevel::Log: // DARK_GREEN
				tracyColor = 0x13a10e;
				break;
			case LogLevel::Verbose: // BRIGHT_CYAN
				tracyColor = 0x61d6d6;
				break;
			case LogLevel::VeryVerbose: // DARK_CYAN
				tracyColor = 0x3a96dd;
				break;
		};

		TracyMessageC(msg.formattedMsg.data(), msg.formattedMsg.size(), tracyColor);
	}
	#endif

}


} // namespace details

//////////////////////////////////////////////////////////////////////////
// LogCategoryBase
//////////////////////////////////////////////////////////////////////////

LogCategoryBase::LogCategoryBase(const char* name, LogLevel initialLevel, LogLevel compileTimeLevel)
	: m_name(name)
	, m_initialLevel(initialLevel)
	, m_level(initialLevel)
	, m_compileTimeLevel(compileTimeLevel)
{
	if (ms_first==nullptr)
	{
		ms_first = this;
	}
	else
	{
		// Find the last category
		LogCategoryBase* ptr = ms_first;
		while(ptr->m_next)
			ptr = ptr->m_next;
		ptr->m_next = this;
	}
}

void LogCategoryBase::setLevel(LogLevel level)
{
	// Take into considering the minimum compiled level
	m_level = LogLevel( (std::min)(static_cast<int>(m_compileTimeLevel), static_cast<int>(level)) );
}

cz::LogCategoryBase* LogCategoryBase::getNext()
{
	return m_next;
}

LogCategoryBase* LogCategoryBase::getFirst()
{
	return ms_first;
}

LogCategoryBase* LogCategoryBase::find(const char* name)
{
	LogCategoryBase* ptr = ms_first;
	while(ptr)
	{
		if (ptr->m_name==name)
			return ptr;
		ptr = ptr->m_next;
	};

	return nullptr;
}

std::string_view toString(LogLevel level)
{
	return logLevelsStrs[static_cast<int>(level)].a;
}

void setLogLevel(LogLevel level)
{
	LogCategoryBase* it = LogCategoryBase::getFirst();
	while (it)
	{
		it->setLevel(level);
		it = it->getNext();
	}
}

void setLogSettings(std::string_view logSettings)
{
	visitKeyValues(logSettings, [](std::string_view key, std::string_view value)
	{
		bool isAll = asciiStrEqualsCi(key, "All");
		LogLevel level;
		if (!fromString(value, level))
			return;

		LogCategoryBase* it = LogCategoryBase::getFirst();
		while (it)
		{
			if (isAll || asciiStrEqualsCi(it->getName(), key))
			{
				it->setLevel(level);
			}
			it = it->getNext();
		}
	});
}

} // namespace cz



