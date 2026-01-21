#pragma once

#include "Logging.h"
#include "Singleton.h"
#include "AsyncCommandQueue.h"
#include "Semaphore.h"

namespace cz
{

class LogOutputs : public Singleton<LogOutputs>
{
  public:
	
	LogOutputs(bool colouredDefaultOutput = true);

	using LogFunc = std::function<void(LogMessage& msg)>;

	void add(void* tag, LogFunc&& logFunc);
	void remove(void* tag);
	void log(bool debuggerOutput, LogMessage& msg);

  protected:

	std::mutex m_mtx;
	std::vector<std::pair<void*, LogFunc>> m_outputs;
};


class FileLogOutput
{
  public: 
	FileLogOutput() = default;
	~FileLogOutput();

	bool open(const std::string& directory, const std::string& filename);

  private:
	void logMsg(LogMessage& msg);

	void finish();

	AsyncCommandQueueAutomatic m_q;
	std::ofstream m_file;
	std::string m_filename;
	Semaphore m_finished;
};

} // namespace cz

