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


/**
 * Callstack marker, based on my own article at:
 * https://www.crazygaze.com/blog/2016/03/11/callstack-markers-boostasiodetailcall_stack/
 *
 * It allows you to mark a callstack with some key and value, and then check if a certain key is present in the callstack, and get
 * its value.
 *
 * This is useful for strands, to mark that we are currently running a strand in the current thread.
 *
 */
template <typename Key, typename Value = unsigned char>
class Callstack
{
  public:
	class Iterator;

	class Context
	{
	  public:
		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;
		explicit Context(Key* k)
			: m_key(k)
			, m_next(Callstack<Key, Value>::ms_top)
		{
			m_val = reinterpret_cast<unsigned char*>(this);
			Callstack<Key, Value>::ms_top = this;
		}

		Context(Key* k, Value& v)
			: m_key(k)
			, m_val(&v)
			, m_next(Callstack<Key, Value>::ms_top)
		{
			Callstack<Key, Value>::ms_top = this;
		}

		~Context()
		{
			Callstack<Key, Value>::ms_top = m_next;
		}

		Key* getKey()
		{
			return m_key;
		}

		Value* getValue()
		{
			return m_val;
		}

	  private:
		friend class Callstack<Key, Value>;
		friend class Callstack<Key, Value>::Iterator;
		Key* m_key;
		Value* m_val;
		Context* m_next;
	};

	class Iterator
	{
	  public:
		Iterator(Context* ctx)
			: m_ctx(ctx)
		{
		}
		Iterator& operator++()
		{
			if (m_ctx)
				m_ctx = m_ctx->m_next;
			return *this;
		}

		bool operator!=(const Iterator& other)
		{
			return m_ctx != other.m_ctx;
		}

		Context* operator*()
		{
			return m_ctx;
		}

	  private:
		Context* m_ctx;
	};

	/**
	 *  Determine if the specified owner is on the stack
	 *  @return The address of the value if present, nullptr if not present
	 */
	static Value* contains(const Key* k)
	{
		Context* elem = ms_top;
		while (elem)
		{
			if (elem->m_key == k)
				return elem->m_val;
			elem = elem->m_next;
		}
		return nullptr;
	}

	static Iterator begin()
	{
		return Iterator(ms_top);
	}

	static Iterator end()
	{
		return Iterator(nullptr);
	}

  private:
	static thread_local Context* ms_top;
};
 
template <typename Key, typename Value>
typename thread_local Callstack<Key, Value>::Context*
    Callstack<Key, Value>::ms_top = nullptr;

} // namespace cz

