module;

//////////////////////////////////////////////////////////////////////////
export module czcore:Logging;

import "czcore.h";
import :Common;

export namespace cz
{

/**
 * Used to keep track of the current frame for logging purposes
 * This is NOT incremented automatically by czcore. The application is responsible for incrementing this.
 *
 * If left with it's original value, then frame numbers are not logged.
 */
extern std::atomic<uint64_t> gFrameCounter;

enum class LogLevel
{
	Off,
	Fatal,
	Error,
	Warning,
	Log,
	Verbose,
	VeryVerbose
};

std::string_view toString(LogLevel level);
bool fromString(std::string_view str, LogLevel& dst);

} // namespace cz

export
template<>
struct std::formatter<cz::LogLevel> : public std::formatter<std::string_view>
{
	auto format(cz::LogLevel l, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "{}", toString(l));
	}
};

export namespace cz
{

class LogCategoryBase;

/*!
 * Puts together everything necessary for a log message.
 * This is then passed down to the logging code
 */
struct LogMessage
{
	LogCategoryBase* category;
	LogLevel level;
	uint64_t frame = std::numeric_limits<uint64_t>::max();
	std::string msg;
	std::string timestamp;
	std::string context;
	std::string formattedMsg;
};

namespace details
{

	/**
	 * Breaks into the debugger
	 */
	void doDebugBreak();

	/**
	 * Don't use this directly. Use the LOG macros
	 */
	void logMessage(bool debuggerOutput, LogMessage& msg);

#if CZ_DEBUG
	constexpr LogLevel compileTimeMaxLogLevel = LogLevel::VeryVerbose;
#elif CZ_DEVELOPMENT
	constexpr LogLevel compileTimeMaxLogLevel = LogLevel::VeryVerbose;
#else
	constexpr LogLevel compileTimeMaxLogLevel = LogLevel::Verbose;
#endif

} // namespace details

class LogCategoryBase
{
  public:

	/**
	 * NOTE: Due to the nature how log categories are declared and defined, `name` will be a string literal
	 * and thus we can keep the `const char*` itself without copying the string
	 */
	LogCategoryBase(const char* name, LogLevel initialLevel, LogLevel compileTimeLevel);

	const char* getName() const
	{
		return m_name;
	}

	bool isSuppressed(LogLevel level) const
	{
		return level > m_level;
	}

	void setLevel(LogLevel level);
	LogLevel getInitialLevel() { return m_initialLevel; }
	LogCategoryBase* getNext();
	static LogCategoryBase* getFirst();
	static LogCategoryBase* find(const char* name);

  protected:
	const char* m_name;
	LogLevel m_initialLevel;
	LogLevel m_level;
	LogLevel m_compileTimeLevel;
	LogCategoryBase* m_next = nullptr;
	static LogCategoryBase* ms_first;
};

template<LogLevel defaultLevel, LogLevel compileTimeLevel>
class LogCategory : public LogCategoryBase
{
public:
	LogCategory(const char* name) : LogCategoryBase(name, defaultLevel, compileTimeLevel)
	{
	}

	// Compile time verbosity
	enum
	{
		CompileTimeLevel  = (int)compileTimeLevel
	};
};

/*!
 * Sets all log categories to the specified log level
 */
void setLogLevel(LogLevel level);


/*!
 * Sets log levels based on a string like "Main=Log,Network=Warning"
 *
 * This is especially useful for setting log levels from a config file or command line.
 * Since the level is applied in the order it shows up in the string, you can can for example set all categories to a specific
 * level and then tweak specific categories. E.g:
 *
 * "All=Warning,Network=Verbose,UI=Log"
 */
void setLogSettings(std::string_view logSettings);

} // namespace cz

/**
 * This is used by CZ_LOG to add some context to the log message.
 * Any classes using CZ_LOG can implement this function in their scope so logging shows whatever prefix is desired. E.g, the name
 * of the class or class instance.
 *
 * Note that any code implementing it's own `getCZLOGPrefix` can use a different signature. All that is necessary is that it returns a string-like object.
 */
export inline std::string_view getCZLOGContext()
{
	return "";
}

CZ_DECLARE_MODULE_LOG_CATEGORY(Main, Log, VeryVerbose)

