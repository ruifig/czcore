#pragma once

#include "crazygaze/core/Common.h"
#include "crazygaze/core/Logging.h"
#include "crazygaze/core/ThreadingUtils.h"
#include "crazygaze/core/Algorithm.h"

/*
This controls if memory should be cleared when the last strong reference is gone and the object destroyed.
Since the way SharedPtr is implemented is that it allocates memory for Control Block + Object in one go, it means the object's memory
will only be de-allocated once the Control Block is also gone.

This means that any code holding a raw pointer for an object that was already destroyed but still has WeakPtrs will likely still
point to the partially correct data (depending on the object's destructor).

For example, this code is a bug, but will still work correctly:

```
struct MyFoo
{
	explicit MyFoo(const char* str) : str(str) {}
	const char* str = nullptr;
};

struct Logger
{
	Logger(MyFoo* ptr) : ptr(ptr) { }
	void log()
	{
		printf("%s\n", ptr->str);
	}

	MyFoo* ptr;
};

auto foo = makeShared<MyFoo>("Hello");
// Keeping a WeakPtr means once the MyFoo object is destroyed, the memory is not actually de-allocated
WeakPtr<MyFoo> wfoo = foo;
// Passing a raw pointer to Logger
Logger logger(foo.get());
// This destroys MyFoo, but doesn't de-allocate the memory, because of the WeakPtr.
foo.reset();

// BUG: "logger.ptr->str" still points "Hello", because MyFoo's destructor doesn't actually clear anything.
logger.log();
```

By setting this to 1, the memory will be cleared to `0xDD` (like MS's CRT Debug Heap) when the object is destroyed, making it
easier to spot these kind of bugs.
*/
#if CZ_DEBUG || CZ_DEVELOPMENT
	#define CZ_SHAREDPTR_CLEAR_MEM 1
#else
	#define CZ_SHAREDPTR_CLEAR_MEM 0
#endif

/**
 * Enable support for capturing stack traces.
 * This enables support, but it's still opt-in on a per-type basis, to minimize performance impact.
 */
#ifndef CZ_SHAREDPTR_STACKTRACES
	#if CZ_DEBUG || CZ_DEVELOPMENT 
		#define CZ_SHAREDPTR_STACKTRACES 1
	#else
		#define CZ_SHAREDPTR_STACKTRACES 0
	#endif
#endif

/**
 * Set this to `std::true_type` in your build system to enable stack traces for all types by default.
 */
#ifndef CZ_SHAREDPTR_STACKTRACES_DEFAULT
	// By default, stack traces are disabled for all types, to minimize performance impact.
	#define CZ_SHAREDPTR_STACKTRACES_DEFAULT std::false_type
#endif

#if CZ_SHAREDPTR_STACKTRACES
	#include "crazygaze/core/LinkedList.h"
#endif

namespace cz
{


#if CZ_SHAREDPTR_STACKTRACES
namespace details
{
	/**
	 * Doubly linked list to keep track of stacktraces
	 */
	class TraceList
	{
	  public:

		~TraceList()
		{
			// If this triggered, we probably have a bug somewhere where we forgot to remove entries.
			assert(m_list.empty());
		}

		void add(struct SharedPtrTrace* trace)
		{
			m_lock.lock();
			m_list.push_back(trace);
			m_lock.unlock();
		}

		void remove(struct SharedPtrTrace* trace)
		{
			m_lock.lock();
			cz::removeFirst(m_list, trace);
			m_lock.unlock();
		}

		bool isEmpty()
		{
			m_lock.lock();
			bool res = m_list.empty();
			m_lock.unlock();
			return res;
		}

		template<typename F>
		void visitAll(F&& f)
		{
			m_lock.lock();
			for(const struct SharedPtrTrace* ele : m_list)
				f(ele);
			m_lock.unlock();
		}

		/**
		 * Returns the first item (or nullptr if empty), and the total number of items.
		 * This is used for debugging things at shutdown.
		 */
		std::pair<const SharedPtrTrace*, uint32_t> getFirstAndCount()
		{
			m_lock.lock();
			SharedPtrTrace* res = m_list.empty() ? nullptr : m_list.front();
			uint32_t size = static_cast<uint32_t>(m_list.size());
			m_lock.unlock();
			return {res, size};
		}

	  private:
		std::vector<struct SharedPtrTrace*> m_list;
		SpinLock m_lock;
	};

	struct SharedPtrTrace
	{
		enum class Type
		{
			Creation,
			StrongRef,
			WeakRef
		};

		explicit SharedPtrTrace(Type type, std::shared_ptr<TraceList> inOuter)
			: type(type)
			, outer(std::move(inOuter))
		{
			if (outer)
			{
				timestamp = std::chrono::high_resolution_clock::now();
				frame = gFrameCounter.load();
				trace = std::stacktrace::current();
				outer->add(this);
			}
		}
		
		~SharedPtrTrace()
		{
			if (outer)
				outer->remove(this);
		}
			
		Type type;
		std::chrono::high_resolution_clock::time_point timestamp;
		uint64_t frame;
		std::stacktrace trace;

		// Sounds a bit stupid to use a shared_ptr, but it solves solve problems related to the lifetime of the TraceList and control block
		// I initially had a `std::unique_ptr<TraceList>` in the control block, but that approach had some issues for when the control
		// block was released.
		std::shared_ptr<TraceList> outer;
	};

} // namespace details

#endif

/**
 * Used to extract stack traces from SharedPtr and WeakPtr instances
 */
struct SharedPtrTraces
{
	struct Entry
	{
		std::chrono::high_resolution_clock::time_point ts;
		uint64_t frame;
		std::stacktrace trace;
	};
	
	/**
	 * This is the stack trace of when the control block was created.
	 * This does NOT represent an active reference. It's purpose is to help understand where the object was created,
	 * even if the original strong reference is gone or even if the object was already destroyed (but there are still weak
	 * references keeping the control block alive).
	 */
	Entry creationTrace;
	std::vector<Entry> strong;
	std::vector<Entry> weak;
};


namespace details
{
	/**
	 * Bare minimum needed for the control block that doesn't need to be templated
	 * This is mostly so we have some bare non-templated functionality to query traces
	 */
	class ControlBlockDetails
	{
	  public:
		virtual ~ControlBlockDetails();

		#if CZ_SHAREDPTR_STACKTRACES
		std::unique_ptr<SharedPtrTrace> createStackTrace(SharedPtrTrace::Type type);
		SharedPtrTraces getTraces();

		// The trace for when the control block was create OR tracing was enabled for this control block
		// - If tracing is enabled per class, than this is the trace for when the object was created.
		// - If tracing is disabled for the class, but then was enabled for this particular object instance,
		//	 then this is the trace for when it was enabled.
		std::unique_ptr<SharedPtrTrace> firstTrace;

		// Enabled stack tracing for this block, if not enabled already
		void enableTraces()
		{
			if (!firstTrace)
				firstTrace = createStackTrace(SharedPtrTrace::Type::Creation);
		}
		#endif

	  protected:

		#if CZ_SHAREDPTR_STACKTRACES
		
		// Controls if tracing is currently enabled for this control block.
		// "currently", because this can be enabled/disabled at any point.
		// Once enabled, it will start creating creating traces, and once disable it stops creating
		// traces BUT doesn't delete the previous ones.
		bool tracingEnabled = false;

		// Counts how many traces were creating for the entire lifetime of the control block
		std::atomic<uint64_t> totalLifetimeTraces = 0;

		void* objBasePtr = nullptr; // The application's object pointer
		#endif
	};
}

#if CZ_SHAREDPTR_STACKTRACES

/**
 * When CZ_SHAREDPTR_STACKTRACES is `1`, this class is used to keep a record of
 * all existing control blocks for which we are recording call stacks, regardless of their type.
 * 
 * This allows an application to query what's alive by having a pointer.
 */
class SharedPtrTracingRegistry
{
  public:

	SharedPtrTracingRegistry() = default;
	~SharedPtrTracingRegistry() = default;

	static SharedPtrTracingRegistry& get()
	{
		static SharedPtrTracingRegistry instance;
		return instance;
	}


	// These should only be used internally (as-in, user code should not use this)
	void internal_add(void* objBasePtr, details::ControlBlockDetails* ctrlBlock);
	void internal_remove(void* objBasePtr);

	/**
	 * Using this is optional. It lets the application set some tag for an object.
	 * This can be used for example to add some name to make it easier to search by object name instead of pointer
	 *
	 * @param `tagProvider`
	 *	A callable that returns `void*` that should be used as the tag.
	 *	This is only called if the object is found.
	 *	
	 * @return Returns the previous tag or nullptr if the object was not found
	 *
	 * This would normally be called from object constructor.
	 *
	 * IMPORTANT: If the provided tag points to some of the object's fields, a call with `setTag(<objPtr>, nullptr)` should
	 * be made in the object's destructor. This is because in a multi-threaded environment, there is a time window
	 * between the object destruction and the control block destruction. As-in:
	 * 1. The object gets destroyed (thus causing `tag` to now point to some invalid memory)
	 * 2. The control block gets destroyed.
	 *
	 * Between 1. and 2., some other thread can query the registry and still sees the tag (because the control block still exists),
	 * but that tag now points to invalid memory.
	 *
	 * NOTE:
	 * The reason we have `tagProvider` and not simply a tag is because using simply a tag can be problematic. Imagine the caller
	 * wants to use some custom struct for tags:
	 * ```
	 *	// It sets the tag in the object constructor
	 *	SharedPtrTracingRegistry::get().setTag(ptr, new MyTrackingInfo{...});
	 *
	 *	// It releases the tag in the object destructor
	 *	if (void* tag = SharedPtrTracingRegistry::get().getTag(ptr))
	 *		delete reinterpret_cast<MyTrackingInfo*>(tag);
	 *
	 * This has the problem that if the object is not being tracked, then `setTag` does nothing, thus leaking
	 * the allocated struct.
	 */ 
	template<typename F>
		requires std::is_invocable_r_v<void*, F>
	void* setTag(void* objBasePtr, F&& tagProvider)
	{
		auto lk = std::lock_guard(m_mtx);
		auto it = m_c.find(objBasePtr);
		if (it != m_c.end())
		{
			void* previousTag = it->second.tag;
			it->second.tag = tagProvider();
			return previousTag;
		}
		else
		{
			return nullptr;
		}
	}

	/**
	 * Retrieves the user specified tag.
	 * `first` is true if the object was found
	 * `second` is set to the tag if `first` is true
	 */
	std::pair<bool, void*> getTag(void* objBasePtr);

	/**
	 * Helper to retrieve the tag as a specific type.
	 * All it does is a cast, so use with care
	 */
	template<typename T>
	std::pair<bool, T> getTagAs(void* objBasePtr)
	{
		auto [found, tag] = getTag(objBasePtr);
		if (found)
			return {true, reinterpret_cast<T>(tag)};
		else
			return {false, T{}};
	}

	/**
	 * Visits all tracked objects
	 * The visitor function takes two parameters (the object pointer, and the tag pointer)
	 * If the visitor returns `true`, the iteration stops.
	 * 
	 */
	template<typename F>
		requires std::is_invocable_r_v<bool, F, void*, void*>
	void visitAll(F&& visitor)
	{
		auto lk = std::lock_guard(m_mtx);
		for(auto&& [objBasePtr, info] : m_c)
		{
			if (visitor(objBasePtr, info.tag))
				return;
		}
	}


	/**
	 * Visits all tracked objects, and returns the traces for any objects for
	 * which the visitor function returns true.
	 */
	template<typename F>
		requires std::is_invocable_r_v<bool, F, void*, void*>
	std::vector<SharedPtrTraces> getTracesForMultiple(F&& visitor)
	{
		ZoneScoped;
		std::vector<SharedPtrTraces> res;
		auto lk = std::lock_guard(m_mtx);
		for(auto&& [objBasePtr, info] : m_c)
		{
			if (visitor(objBasePtr, info.tag))
				res.push_back(info.ctrBlk->getTraces());
		}

		return res;
	}

	/**
	 * Gets traces for the specified object (if traces are available)
	 *
	 * IMPORTANT: Note that since pointers can be recycled as objects get created and destroyed,
	 * it is up to the caller to make sure the pointer is still for the original object.
	 * As-in, if the caller simply cached the pointer from some other previous call and doesn't actually
	 * need to use the object, and then it simply queries the registry with that pointer, then depending
	 * on the application logic, that pointer in the registry might now be some other object.
	 */
	SharedPtrTraces getTracesFor(void* objBasePtr)
	{
		ZoneScoped;
		auto lk = std::lock_guard(m_mtx);

		auto it = m_c.find(objBasePtr);
		if (it == m_c.end())
			return {};
		else
			return it->second.ctrBlk->getTraces();
	}

  protected:
	std::mutex m_mtx;

	struct Info
	{
		// Since pointers can be recycled (as objects get deleted and created), we use an incrementing counter to uniquely
		// identify an entry
		uint64_t id = 0;

		details::ControlBlockDetails* ctrBlk = nullptr;
		void* tag = nullptr;
	};

	std::atomic<uint64_t> m_idCounter = 0;
	/**
	 * All the control blocks being tracked
	 * The key is the object pointer (what the application uses)
	 */ 
	std::unordered_map<void*, Info> m_c;
};

#else
/**
 * Dummy implementation for when stack traces are disabled
 */
class SharedPtrTracingRegistry
{
  public:

	static SharedPtrTracingRegistry& get()
	{
		static SharedPtrTracingRegistry instance;
		return instance;
	}

	void setTag(void*, void*); { }
	std::pair<bool, void*> getTag(void*) { return {false, nullptr}; }

	template<typename T>
	std::pair<bool, T> getTagAs(void* objBasePtr) { return {false, T{}}; }
};

#endif

namespace details
{


	template<class T>
	bool shouldCaptureStackTraces()
	{
		if constexpr (requires(T* p) { T::captureSharedPtrStackTraces(); })
			return T::captureSharedPtrStackTraces();
		else
			return false;
	}


	/**
	 * RefCounter implements the following interface:
	 * 
	 * void inc()
	 *		Increments the value
	 * uint32_t dec()
	 *		Decrements and returns the new value
	 * uint32_t count()
	 *		Returns the current value
	 * bool inc_nz()
	 *		Increments the value if the current value is not zero and returns true.
	 *		It returns false if the current value is zero.
	 * bool dec_if_one()
	 *		Decrements the value if the current value is one and returns true.
	 *		If the current value is not one, it does nothing and returns false.
	 */
	template<bool ThreadSafe>
	class RefCounter
	{
	};

	template<>
	class RefCounter<false>
	{
	  private:
		
		//
		// Convoluted way to store the value, to match what msvc stl does, so share some stuff in the natvis file
		struct Value
		{
			struct
			{
				uint32_t _Value;
			} _Storage;
		};

		Value m_value;
	  public:

		RefCounter(uint32_t value) noexcept
			: m_value{Value{{value}}}
		{
		}

		[[nodiscard]] uint32_t count() const noexcept
		{
			return m_value._Storage._Value;
		}
	
		void inc() noexcept
		{
			++m_value._Storage._Value;
		}

		[[nodiscard]] uint32_t dec() noexcept
		{
			assert(m_value._Storage._Value > 0);
			return --m_value._Storage._Value;
		}

		bool inc_nz() noexcept
		{
			if (m_value._Storage._Value == 0)
				return false;
			else
			{
				++m_value._Storage._Value;
				return true;
			}
		}

		bool dec_if_one() noexcept
		{
			if (m_value._Storage._Value == 1)
			{
				--m_value._Storage._Value;
				return true;
			}
			else
			{
				return false;
			}
		}
	};

	/**
	 * Some useful links to understand this
	 * - https://www.boost.org/doc/libs/1_57_0/doc/html/atomic/usage_examples.html#boost_atomic.usage_examples.example_reference_counters
	 */
	template<>
	class RefCounter<true>
	{
	  private:
		mutable std::atomic<uint32_t> m_value;

	  public:

		RefCounter(uint32_t value) noexcept
			: m_value(value)
		{
		}

		uint32_t count() const
		{
			return m_value.load(std::memory_order_relaxed);
		}

		void inc() noexcept
		{
			m_value.fetch_add(1, std::memory_order_relaxed);
		}

		uint32_t dec()
		{
			// fetch_sub returns the previous value, so we do -1, so that when we call this and value is 1, we return 0.
			// It's less confusing that way. As-in. if "if dec()==0 then do something"
			return m_value.fetch_sub(1, std::memory_order_acq_rel) - 1;
		}

		bool inc_nz()
		{
			uint32_t n = count();

			while(n != 0)
			{
				if (m_value.compare_exchange_weak(n, n + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
				{
					// We managed to increment the counter from a non-zero value, which means from this point on no other threads will cause it to go to zero.
					return true; 
				}
			}

			return false;
		}

		bool dec_if_one()
		{
			uint32_t n = count();

			while (n != 0)
			{
				if (n == 1)
				{
					if (m_value.compare_exchange_weak(n, 0, std::memory_order_acq_rel, std::memory_order_relaxed))
					{
						// We managed to decrement the counter from 1 to 0, which means from this point on no other threads will
						// cause it to go to 1 again.
						return true;
					}
				}
				else
				{
					// The value is not 1, so we don't want to decrement it.
					return false;
				}
			}
			return false;
		}
	};


} // namespace details

namespace details
{
	
	template<bool MT>
	class BaseSharedPtrControlBlock : public ControlBlockDetails
	{
	  public:

		BaseSharedPtrControlBlock([[maybe_unused]] size_t size)
	#if CZ_SHAREDPTR_CLEAR_MEM
			: size(size)
	#endif
		{
		}

		~BaseSharedPtrControlBlock() = default;

		template<typename T>
		T* toObject() noexcept
		{
			return reinterpret_cast<T*>(this+1);
		}

		virtual void deleteObj() = 0;

	  protected:

		template<typename T, bool MT, typename Deleter>
		friend void* allocSharedPtrBlock();

		RefCounter<MT> strong = 0;
		// weak is initialized to 1, because it helps resolve a race condition when both weak and strong reach 0
		RefCounter<MT> weak = 1;

	#if CZ_SHAREDPTR_CLEAR_MEM
		// Size of the object, in bytes.
		size_t size;
	#endif
	};


	struct SharedPtrDefaultDeleter
	{
		template<typename T>
		void operator()(T* obj) const
		{
			obj->~T();
		}
	};

	template <typename T, class = void>
	struct GetSharedPtrDeleterType
	{
		using type = SharedPtrDefaultDeleter;
	};

	// Use SFINAE to check if T has a "SharedPtrDeleter" member
	template<typename T>
	struct GetSharedPtrDeleterType<T, std::void_t<typename T::SharedPtrDeleter>>
	{
		using type = typename T::SharedPtrDeleter;
	};

	template<typename T, bool MT>
	class SharedPtrControlBlock : public BaseSharedPtrControlBlock<MT>
	{
	  public:

		SharedPtrControlBlock([[maybe_unused]] size_t size)
			: BaseSharedPtrControlBlock<MT>(size)
		{
			// The only difference between the base object and a specialized one is that the specialized one adds some more methods.
			static_assert(sizeof(*this) == sizeof(BaseSharedPtrControlBlock<MT>));
		}

		T* obj()
		{
			return this->toObject<T>();
		}

		uint32_t strongRefs() const noexcept
		{
			return this->strong.count();
		}

		uint32_t weakRefs() const noexcept
		{
			return this->weak.count();
		}

		void incStrong() noexcept
		{
			this->strong.inc();
		}

		void incWeak() noexcept
		{
			this->weak.inc();
		}

		void decStrong()
		{
			assert(this->strong.count() > 0);

			if (this->strong.dec() == 0)
			{
				this->deleteObj();
				decWeak();
			}
		}

		bool decStrongIfOne()
		{
			assert(this->strong.count() > 0);

			if (this->strong.dec_if_one())
			{
				this->deleteObj();
				decWeak();
				return true;
			}
			else
			{
				return false;
			}
		}

		void decWeak()
		{
			if (this->weak.dec() == 0)
			{
				// We need to explicitly call the virtual destructor
				this->~SharedPtrControlBlock();
				free(this);
			}
		}

		bool lockStrong()
		{
			return this->strong.inc_nz();
		}
	};

	template <typename T, bool MT, typename Deleter = SharedPtrDefaultDeleter>
	class SharedPtrControlBlockWithDeleter : public SharedPtrControlBlock<T, MT>
	{
	  public:
		SharedPtrControlBlockWithDeleter([[maybe_unused]] size_t size)
			: SharedPtrControlBlock<T, MT>(size)
		{
			// The only difference between the base object and a specialized one is that the specialized one adds some more methods.
			static_assert(sizeof(*this) == sizeof(SharedPtrControlBlock<T, MT>));
		}

		virtual void deleteObj() override
		{
			auto ptr = const_cast<std::remove_const_t<T>*>(this->obj());
			Deleter{}(ptr);
			#if CZ_SHAREDPTR_CLEAR_MEM
				memset(ptr, 0xDD, this->size);
			#endif
		}
	};

	template<typename T>
	using SharedPtrDeleterFor = typename details::GetSharedPtrDeleterType<T>::type;

	/**
	 * Allocates memory for a control block + a T object
	 *
	 * Returns the pointer that can be used to construct a T object with placement new
	 */
	template<typename T, bool MT, typename Deleter = SharedPtrDeleterFor<T>>
	static void* allocSharedPtrBlock()
	{
		size_t allocSize = sizeof(SharedPtrControlBlockWithDeleter<T, MT, Deleter>) + sizeof(T);
		void* basePtr = malloc(allocSize);
		BaseSharedPtrControlBlock<MT>* control = new (basePtr) SharedPtrControlBlockWithDeleter<T, MT, Deleter>(sizeof(T));

		void* objBasePtr = control + 1;

		#if CZ_SHAREDPTR_STACKTRACES
		control->objBasePtr = objBasePtr;
		control->tracingEnabled = details::shouldCaptureStackTraces<T>();
		control->firstTrace = control->createStackTrace(SharedPtrTrace::Type::Creation);
		#endif

		return objBasePtr;
	}

}  // namespace details


} // namespace cz


