#pragma once

#include "Common.h"

namespace cz
{

class Semaphore
{
public:
	Semaphore (unsigned int count = 0)
		: m_count(count)
	{
	}

	void notify();
	void wait();
	bool trywait();

	template <class Clock, class Duration>
	bool waitUntil(const std::chrono::time_point<Clock, Duration>& point)
	{
		std::unique_lock<std::mutex> lock(m_mtx);
		if (!m_cv.wait_until(lock, point, [this]() { return m_count > 0; }))
		{
			return false;
		}
		m_count--;
		return true;
	}
private:
	TracyLockable(std::mutex, m_mtx);
	std::condition_variable_any m_cv;
	unsigned int m_count;
};


/**
 * A semaphore that blocks until the counter reaches 0
 */
class ZeroSemaphore
{
  public:
	ZeroSemaphore() {}
	void increment();
	void decrement();
	void wait();
	bool trywait();

  private:
	TracyLockable(std::mutex, m_mtx);
	std::condition_variable_any m_cv;
	int m_count = 0;
};

} // namespace cz


