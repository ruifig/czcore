#include "Semaphore.h"

namespace cz
{

void Semaphore::notify()
{
	std::unique_lock<LockableBase(std::mutex)> lock(m_mtx);
	m_count++;
	m_cv.notify_one();
}

void Semaphore::wait()
{
	std::unique_lock<LockableBase(std::mutex)> lock(m_mtx);
	m_cv.wait(lock, [this]() {return m_count > 0; });
	m_count--;
}

bool Semaphore::trywait()
{
	std::unique_lock<LockableBase(std::mutex)> lock(m_mtx);
	if (m_count)
	{
		m_count--;
		return true;
	}
	else
	{
		return false;
	}
}

void ZeroSemaphore::increment()
{
	std::unique_lock<LockableBase(std::mutex)> lock(m_mtx);
	m_count++;
}

void ZeroSemaphore::decrement()
{
	std::unique_lock<LockableBase(std::mutex)> lock(m_mtx);
	m_count--;
	m_cv.notify_all();
}

void ZeroSemaphore::wait()
{
	std::unique_lock<LockableBase(std::mutex)> lock(m_mtx);
	m_cv.wait(lock, [this]()
	{
		return m_count == 0;
	});
}

bool ZeroSemaphore::trywait()
{
	std::unique_lock<LockableBase(std::mutex)> lock(m_mtx);
	if (m_count == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

} // namespace cz

