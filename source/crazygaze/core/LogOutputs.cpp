#include "LogOutputs.h"
#include "Algorithm.h"
#include "StringUtils.h"

namespace cz
{

static void printfLogMessage(LogMessage& msg)
{
	// Reference: https://ss64.com/nt/syntax-ansi.html
	const char* resetCode = "\x1B[0m";

	const char* code = resetCode;
	switch(msg.level)
	{
		case LogLevel::Off:
			code = resetCode;
			break;
		case LogLevel::Fatal:
			code = "\x1B[31m";
			break;
		case LogLevel::Error:
			code = "\x1B[1m\x1B[31m";
			break;
		case LogLevel::Warning:
			code = "\x1B[33m";
			break;
		case LogLevel::Log:
			code = "\x1B[32m";
			break;
		case LogLevel::Verbose:
			code = "\x1B[96m";
			break;
		case LogLevel::VeryVerbose:
			code = "\x1B[36m";
			break;
	}

	printf("%s%s%s", code, msg.formattedMsg.c_str(), resetCode);
}

static void printfLogMessageNoColour(LogMessage& msg)
{
	printf("%s", msg.formattedMsg.c_str());
}

LogOutputs::LogOutputs(bool colouredDefaultOutput)
{
	#if CZ_WINDOWS
		SetConsoleOutputCP(CP_UTF8);
	#endif
	if (colouredDefaultOutput)
		add(this, printfLogMessage);
	else
		add(this, printfLogMessageNoColour);
}

void LogOutputs::add(void* tag, LogFunc&& logFunc)
{
	auto lk = std::lock_guard<std::mutex>(m_mtx);
	m_outputs.push_back(std::make_pair(tag, std::move(logFunc)));
}

void LogOutputs::remove(void* tag)
{
	auto lk = std::lock_guard<std::mutex>(m_mtx);
	remove_if(m_outputs, [tag](std::pair<void*, LogFunc>& p)
	{
		return p.first == tag;
	});
}

void LogOutputs::log(bool debuggerOutput, LogMessage& msg)
{
	if (debuggerOutput)
	{
		#if CZ_WINDOWS
			OutputDebugStringW(reinterpret_cast<LPCWSTR>(widen(msg.formattedMsg).c_str()));
		#endif
	}

	{
		auto lk = std::lock_guard<std::mutex>(m_mtx);
		for(auto&& f : m_outputs)
		{
			f.second(msg);
		}
	}
}

bool FileLogOutput::open(const std::string& inDirectory, const std::string& inFilename)
{
	std::string dir;
	if (inDirectory.size())
	{
		dir = inDirectory;
	}
	else
	{
		dir = std::filesystem::current_path().string();
	}

	std::string filename = dir + std::string("/") + inFilename + ".log";

	// If the file already exists, we need to rename it by adding the last write time to its name.
	// If any of that fails, we return Unknown, since it's probably an OS issue (e.g: Permissions)
	std::error_code ec;
	if (std::filesystem::exists(filename, ec))
	{
#if defined _WIN32
		std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(dir, ec);
		if (ec)
		{
			return false;
		}

		// Cast to seconds, then convert to utc time.
		// We need to first convert to seconds, otherwise fmt::format will display seconds as a floating point.
		auto secs = std::chrono::time_point_cast<std::chrono::seconds>(lastWriteTime);
		auto utcTime = std::filesystem::file_time_type::clock::to_utc(secs);

#elif defined __linux__
		auto lastWriteTime = std::chrono::system_clock::now();
		auto utcTime = std::chrono::time_point_cast<std::chrono::seconds>(lastWriteTime);
#else
	#error "Uknown or unsupported platform"
#endif

		std::string newFilename = dir + std::format("/{}-{:%Y.%m.%d-%H.%M.%S}.log", inFilename, utcTime);
		std::filesystem::rename(filename, newFilename, ec);
		if (ec)
		{
			return false;
		}
	}
	else
	{
		// Nothing to do. If the log file creation fails, we'll just try to create the log file
	}

	m_filename = filename;

#if defined _WIN32
	// On Windows, we need to use a std::wstring
	m_file.open(widen(m_filename), std::ios::out | std::ios::trunc);
#elif defined __linux
	// On linux, open expects an UTF8 string (I think)
	m_file.open(m_filename, std::ios::out | std::ios::trunc);
#elif
	#error "Unknown or unsupported platform"
#endif

	if (!m_file.is_open())
	{
		return false;
	}

	if (LogOutputs* outputs = LogOutputs::tryGet())
	{
		outputs->add(this, [this](LogMessage& msg)
		{
			logMsg(msg);
		});

		return true;
	}
	else
	{
		return false;
	}

}

void FileLogOutput::logMsg(LogMessage& msg)
{
	m_q.send([this, str = msg.formattedMsg]()
	{
		m_file << str;
	});
}

void FileLogOutput::flush()
{
	m_q.send([this]()
	{
		m_file.flush();
	});
}

FileLogOutput::~FileLogOutput()
{
	flush();
	if (!m_file.is_open())
	{
		return;
	}

	if (LogOutputs* outputs = LogOutputs::tryGet())
	{
		outputs->remove(this);
	}
}

} // namespace cz

