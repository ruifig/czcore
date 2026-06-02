#pragma once

namespace cz
{

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

/**
 * Copied from https://rigtorp.se/spinlock/
 *
 * This implements a spinlock mutex.
 * A spinlock mutex doesn't put the thread to sleep. Instead, it continuously checks if the lock is available. This is
 * faster if we know that the locks are held for very short periods.
 *
 * NOTE: Because this implements the same public interface as std::mutex, it can be used with
 * std::lock_guard and std::unique_lock.
 *
 */
class SpinLock
{
  private:

	std::atomic<bool> lock_ = {0};

	// Copied from Tracy
	void emitPause()
	{
	#if defined(_MSC_VER) && !(defined(_M_ARM) || defined(_M_ARM64))
		_mm_pause();
	#elif defined(__x86_64__) || defined(__i386__)
		__asm__ volatile("pause" ::: "memory");
	#elif defined(__aarch64__) || (defined(__arm__) && __ARM_ARCH >= 7)
		__asm__ volatile("yield" ::: "memory");
	#elif defined(__powerpc__) || defined(__powerpc64__)
			// No idea if ever been compiled in such archs but ... as precaution
		__asm__ volatile("or 27,27,27");
	#elif defined(__sparc__)
		__asm__ volatile("rd %ccr, %g0 \n\trd %ccr, %g0 \n\trd %ccr, %g0");
	#else
		std::this_thread::yield();
	#endif
	}

  public:

	void lock() noexcept
	{
		for (;;)
		{
			// Optimistically assume the lock is free on the first try
			if (!lock_.exchange(true, std::memory_order_acquire))
			{
				return;
			}
			// Wait for lock to be released without generating cache misses
			while (lock_.load(std::memory_order_relaxed))
			{
				// Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
				// hyper-threads
				emitPause();
			}
		}
	}

	bool try_lock() noexcept
	{
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses if someone does while(!try_lock())
		return !lock_.load(std::memory_order_relaxed) && !lock_.exchange(true, std::memory_order_acquire);
	}

	void unlock() noexcept
	{
		lock_.store(false, std::memory_order_release);
	}
};

} // namespace cz


