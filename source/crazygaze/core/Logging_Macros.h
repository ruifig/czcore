#pragma once

#include "Common_Macros.h"

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
				_cz_internal_msg.frame = gFrameCounter.load();                                                 \
				_cz_internal_msg.level = ::cz::LogLevel::logLevel;                                             \
				_cz_internal_msg.context = getCZLOGContext();                                                  \
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
	export class LogCategory##NAME : public ::cz::LogCategory<::cz::LogLevel::DEFAULT_LEVEL, ::cz::LogLevel::COMPILETIME_LEVEL>  \
	{                                                                                                                                     \
		public:                                                                                                                           \
		LogCategory##NAME() : LogCategory(#NAME) {}                                                                                       \
	}; \
	\
	export extern LogCategory##NAME log##NAME;

#define CZ_DEFINE_LOG_CATEGORY(NAME) LogCategory##NAME log##NAME;


