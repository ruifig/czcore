#pragma once

#include "Common.h"
#include <string_view>

namespace cz
{

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

template<>
struct std::formatter<cz::LogLevel> : public std::formatter<std::string_view>
{
	auto format(cz::LogLevel l, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "{}", toString(l));
	}
};

namespace cz
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
	std::string msg;
	std::string timestamp;
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
	inline static LogCategoryBase* ms_first = nullptr;
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

#define CZ_LOG_CHECK_COMPILETIME_LEVEL(name, verbosity) \
	(((int)::cz::LogLevel::verbosity <= (int)LogCategory##name::CompileTimeLevel) && \
	 ((int)::cz::LogLevel::verbosity <= (int)::cz::details::compileTimeMaxLogLevel))

#define CZ_LOG_IMPL(debuggerOutput, name, logLevel, fmtStr, ...)                                               \
	{                                                                                                          \
		if constexpr(CZ_LOG_CHECK_COMPILETIME_LEVEL(name, logLevel))                                           \
		{                                                                                                      \
			if (!log##name.isSuppressed(::cz::LogLevel::logLevel))                                             \
			{                                                                                                  \
				::cz::LogMessage _cz_internal_msg;                                                             \
				_cz_internal_msg.category = &log##name;                                                        \
				_cz_internal_msg.level = ::cz::LogLevel::logLevel;                                             \
				_cz_internal_msg.msg = std::format(fmtStr, ##__VA_ARGS__);                                     \
				::cz::details::logMessage(debuggerOutput, _cz_internal_msg);                                   \
			}                                                                                                  \
		}                                                                                                      \
		if constexpr (::cz::LogLevel::logLevel == ::cz::LogLevel::Fatal)                                       \
		{                                                                                                      \
			::cz::details::doDebugBreak();                                                                     \
		}                                                                                                      \
	}

#define CZ_LOG(name, logLevel, format, ...) CZ_LOG_IMPL(true, name, logLevel, format,  ##__VA_ARGS__)
#define CZ_LOG_EX(debuggerOutput, name, logLevel, format, ...) CZ_LOG_IMPL(debuggerOutput, name, logLevel, format,  ##__VA_ARGS__)

#define CZ_CHECK_IMPL(expr)                    \
	if (!(expr))                               \
	{                                          \
		CZ_LOG(Main, Fatal, "Assert :" #expr); \
		exit(1);                               \
	}

#define CZ_CHECK_F_IMPL(expr, format, ...)                                 \
	if (!(expr))                                                           \
	{                                                                      \
		CZ_LOG(Main, Fatal, "Assert '" #expr "': " format, ##__VA_ARGS__); \
		exit(1);                                                           \
	}

#if CZ_DEBUG || CZ_DEVELOPMENT
	#define CZ_CHECK(expr) CZ_CHECK_IMPL(expr)
	#define CZ_CHECK_F(expr, format, ...) CZ_CHECK_F_IMPL(expr, format, ##__VA_ARGS__)

	#define CZ_VERIFY(expr) CZ_CHECK_IMPL(expr)
	#define CZ_VERIFY_F(expr, format, ...) CZ_CHECK_F_IMPL(expr, format, ##__VA_ARGS__)
#else
	#define CZ_CHECK(expr) ((void)0)
	#define CZ_CHECK_F(expr, format, ...) ((void)0)

	#define CZ_VERIFY(expr) {if(expr) {}}
	#define CZ_VERIFY_F(expr, format, ...) {if(expr) {}}
#endif


#define CZ_DECLARE_LOG_CATEGORY(NAME, DEFAULT_LEVEL, COMPILETIME_LEVEL) \
	extern class LogCategory##NAME : public ::cz::LogCategory<::cz::LogLevel::DEFAULT_LEVEL, ::cz::LogLevel::COMPILETIME_LEVEL>  \
	{                                                                                                                                     \
		public:                                                                                                                           \
		LogCategory##NAME() : LogCategory(#NAME) {}                                                                                       \
	} log##NAME;

#define CZ_DEFINE_LOG_CATEGORY(NAME) LogCategory##NAME log##NAME;

CZ_DECLARE_LOG_CATEGORY(Main, Log, VeryVerbose)

