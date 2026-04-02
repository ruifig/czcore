module;

//////////////////////////////////////////////////////////////////////////
export module czcore:Threading;

import "czcore.h";
import :Common;

export namespace cz
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

/*!
 * Based on Herb Sutters's Monitor<T> class.
 * It protects all access to an object
 */
template <class T>
class Monitor
{
private:
	mutable T m_t;
	mutable std::mutex m_mtx;

public:
	using Type = T;
	Monitor() {}
	Monitor(T t_) : m_t(std::move(t_)) {}
	template <typename F>
	auto operator()(F f) const -> decltype(f(m_t))
	{
		std::lock_guard<std::mutex> hold{ m_mtx };
		return f(m_t);
	}
};

} // namespace cz

