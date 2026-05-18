#pragma once

#include "Common.h"
#include "Logging.h"
#include "ThreadingUtils.h"

/**
 *
 * Custom shared_ptr implementation.
 * It allows tweaking a few things:
 * - Allows specifying if it's thread safe or not.
 * - Allows capturing stack traces for debugging purposes.
 *
 * It doesn't provide exactly the same functionality as std::shared_ptr, and any object to be tracked
 * needs to be allocated with makeShared. This is because the control block is ALWAYS allocated together with the object.
 *
 *
 * Available classes:
 * 
 * - SharedPtr
 *		Equivalent to std::shared_ptr.
 * - WeakPtr
 *		Equivalent to std::weak_ptr.
 * - SharedRef
 *		A non-nullable version of SharedPtr. Similar to std::shared_ptr but doesn't allow null values. This is handy for APIs that
 *		want to enforce that the pointer is not null.
 * - ObserverPtr
 *		A special kind of WeakPtr that doesn't allow promoting to SharedPtr. The purpose is just to check if a pointer is still valid.
 *		It has very little use, and it's unsafe if used with multi-threading.
 * 
 */


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
 * The library allows capturing stack traces, which helps figure out leaks and dangling weak pointers.
 * To enable support, set CZ_SHAREDPTR_STACKTRACES to 1 in your build system (or 0 to disable it).
 * If not set, it defaults to 1 in debug/development builds, and 0 in release builds.
 *
 * When set to 1, support is compiled in, BUT not actually enabled.
 * It can be enabled on a per-type basis, or enabled for all types by changing CZ_SHAREDPTR_STACKTRACES_DEFAULT.
 *
 * To enable for a specific type T (and any derived types) you can add a static member in your type:
 *		``static constexpr bool enable_sharedptr_stacktraces = true;```
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

	template<typename T, class = void>
	struct enable_sharedptr_stacktraces : CZ_SHAREDPTR_STACKTRACES_DEFAULT
	{
	};

	// Use SFINAE to check if T has a "enable_sharedptr_stacktraces" member, and if so, use it as the value for
	// enable_sharedptr_stacktraces<T>::value
	template<typename T>
	struct enable_sharedptr_stacktraces<T, std::void_t<decltype(T::enable_sharedptr_stacktraces)>>
		: std::bool_constant<T::enable_sharedptr_stacktraces>
	{
	};

	template<class T>
	inline constexpr bool enable_sharedptr_stacktraces_v =
#if CZ_SHAREDPTR_STACKTRACES
		 enable_sharedptr_stacktraces<T>::value;
#else
		false;
#endif


#if CZ_SHAREDPTR_STACKTRACES

	/**
	 * Doubly linked list to keep track of stacktraces
	 */
	class TraceList
	{
	  public:

		void add(struct SharedPtrTrace* trace)
		{
			m_lock.lock();
			m_list.pushBack(trace);
			m_lock.unlock();
		}

		void remove(struct SharedPtrTrace* trace)
		{
			m_lock.lock();
			m_list.remove(trace);
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

	  private:
		DoublyLinkedList<struct SharedPtrTrace> m_list;
		SpinLock m_lock;
	};

	struct SharedPtrTrace : public DoublyLinked<SharedPtrTrace>
	{
		enum class Type
		{
			Creation,
			StrongRef,
			WeakRef
		};

		explicit SharedPtrTrace(Type type, TraceList* outer)
			: type(type)
			, outer(outer)
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
		TraceList* outer = nullptr;
	};

#endif


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
	 */
	template<bool ThreadSafe>
	class RefCounter
	{
	};

	template<>
	class RefCounter<false>
	{
	  private:
		uint32_t m_value;
	  public:

		RefCounter(uint32_t value) noexcept
			: m_value(value)
		{
		}

		[[nodiscard]] uint32_t count() const noexcept
		{
			return m_value;
		}
	
		void inc() noexcept
		{
			++m_value;
		}

		[[nodiscard]] uint32_t dec() noexcept
		{
			assert(m_value > 0);
			return --m_value;
		}

		bool inc_nz() noexcept
		{
			if (m_value == 0)
				return false;
			else
			{
				++m_value;
				return true;
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
				if (m_value.compare_exchange_weak(n, n + 1, std::memory_order_acquire, std::memory_order_relaxed))
				{
					// We managed to increment the counter from a non-zero value, which means from this point on no other threads will cause it to go to zero.
					return true; 
				}
			}

			return false;
		}
	};

	template<bool MT>
	class BaseSharedPtrControlBlock
	{
	  public:

		BaseSharedPtrControlBlock([[maybe_unused]] size_t size)
#if CZ_SHAREDPTR_CLEAR_MEM
			: size(size)
#endif
		{
		}

		virtual ~BaseSharedPtrControlBlock() = default;

		template<typename T>
		T* toObject() noexcept
		{
			return reinterpret_cast<T*>(this+1);
		}


		virtual void deleteObj() = 0;


		#if CZ_SHAREDPTR_STACKTRACES
		std::unique_ptr<SharedPtrTrace> createStackTrace(SharedPtrTrace::Type type)
		{
			if (traceData)
			{
				// Using `new` instead of make_unique, so `std::make_unique` doesn't show up in the stacktrace.
				// This makes it easier for tools by allowing them to skip all the frames at the top that start with `cz::`
				return std::unique_ptr<SharedPtrTrace>(new SharedPtrTrace(type, &traceData->traceList));
			}
			else
			{
				return nullptr;
			}
		}

		SharedPtrTraces getTraces()
		{
			SharedPtrTraces res;

			if (traceData)
			{
				traceData->traceList.visitAll([&res](const SharedPtrTrace* ele)
				{
					SharedPtrTraces::Entry entry{ele->timestamp, ele->frame, ele->trace};
					if (ele->type == SharedPtrTrace::Type::Creation)
						res.creationTrace = std::move(entry);
					else if (ele->type == SharedPtrTrace::Type::StrongRef)
						res.strong.emplace_back(std::move(entry));
					else if (ele->type == SharedPtrTrace::Type::WeakRef)
						res.weak.emplace_back(std::move(entry));
					else
					{
						CZ_CHECK(false);
					}
				});
			}

			return res;
		}
		#endif

	  protected:

		template<typename T, bool MT, typename Deleter>
		friend void* allocSharedPtrBlock();

		#if CZ_SHAREDPTR_STACKTRACES
		struct StackTraceData
		{
			~StackTraceData()
			{
				firstTrace = nullptr;

				// If there are still any traces in the list, then we have a bug somewhere in this code.
				CZ_CHECK(traceList.isEmpty());
			}
			TraceList traceList;
			std::unique_ptr<SharedPtrTrace> firstTrace;
		};
		// Intentionally putting this in a unique_ptr, so it's out of the same cache line, and it's not loaded every time
		// we need to dereference a SharedPtr/WeakPtr.
		std::unique_ptr<StackTraceData> traceData;
		#endif

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

	// Use SFINAE to check if T has a "SharedPtrDeleter" member, and if so, use it as the value for
	// enable_sharedptr_stacktraces<T>::value
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

		#if CZ_SHAREDPTR_STACKTRACES
		if constexpr(details::enable_sharedptr_stacktraces_v<T>)
		{
			control->traceData = std::make_unique<typename BaseSharedPtrControlBlock<MT>::StackTraceData>();
			control->traceData->firstTrace = control->createStackTrace(SharedPtrTrace::Type::Creation);
		}
		#endif

		return control + 1;
	}

} // namespace details

template<typename T, bool MT>
class SharedRef;

/**
 * A rather simplistic std::shared_ptr equivalent, that is faster than std::shared_ptr since it is NOT thread safe.
 * Things to be aware of:
 *
 * Custom deleters can be supported by defining a "SharedPtrDeleter" type in your class, and providing an implementation for it.
 * For example:
 * ```
 *		struct MyDeleter
 *		{
 * 			template<typename T>
 * 			void operator()(T* obj) const { obj->~T(); }
 *		};
 *
 *		struct Foo
 *		{
 *			SharedPtrDeleter = MyDeleter; // Specify what deleter to use for this class
 *		};
 * ```
 * 
 * 
 * - NOT thread safe. That's the point of this class.
 * - Might not provide all the functions and/or operators std::shared_ptr provides
 * - IMPORTANT: Assumes memory for the object was allocated with BaseSharedPtrControlBlock::allocBlock. this means that when using
 *   the SharedPtr<T>::SharedPtr(U* ptr) constructor, care must be taken that "ptr" was allocated properly. You can do this by:
 *		- Using makeShared<T>. E.g: SharedPtr<Foo> foo = makeShared<Foo>();
 *		- Using BaseSharedPtrControlBlock::allocBlock and placement new. e.g:
 *			SharedPtr<Foo> foo(new (details::BaseSharedPtrControlBlock::allocBlock<Foo>()) Foo);
 *			The reason this constructor is provided is so that SharedPtr can be used with classes whose constructors are private/protected.
 * 
 */
template<typename T, bool MT>
class SharedPtr
{
  public:

	template<typename U, bool OtherMT, bool IsObserver>
	friend class WeakPtrImpl;

	template<typename U, bool OtherMT>
	friend class SharedPtr;

	using ControlBlock = details::SharedPtrControlBlock<T, MT>;	

	using pointer = T*;
	using element_type = T;

	SharedPtr() noexcept
	{
	}

	SharedPtr(std::nullptr_t) noexcept
	{
	}

	/**
	 * Constructs the SharedPtr from a previously allocated object.
	 * IMPORTANT: The object's memory MUST have been allocated with BaseSharedPtrControlBlock::allocBlock. See the SharedPtr
	 * class documentation for details.
	 */
	template<typename U>
	explicit SharedPtr(U* ptr) noexcept
	{
		if (ptr)
		{
			static_assert(std::is_convertible_v<U*, T*>);
			const void* rawPtr = (reinterpret_cast<const uint8_t*>(static_cast<T*>(ptr)) - sizeof(details::BaseSharedPtrControlBlock<MT>));
			acquireBlock<true>(reinterpret_cast<ControlBlock*>(const_cast<void*>(rawPtr)));
		}
	}

	~SharedPtr() noexcept
	{
		m_control.release();
	}

	SharedPtr(const SharedPtr& other) noexcept
	{
		acquireBlock<true>(other.m_control.ctrl);
	}

	template<typename U>
	SharedPtr(const SharedPtr<U, MT>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock<true>(other.m_control.ctrl);
	}

	SharedPtr(SharedPtr&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U>
	SharedPtr(SharedPtr<U, MT>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		std::swap(m_control, reinterpret_cast<ControlHolder&>(other.m_control));
	}

	SharedPtr& operator=(const SharedPtr& other) noexcept
	{
		SharedPtr(other).swap(*this);
		return *this;
	}

	template<typename U>
	SharedPtr& operator=(const SharedPtr<U, MT>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		SharedPtr(other).swap(*this);
		return *this;
	}

	SharedPtr& operator=(SharedPtr&& other) noexcept
	{
		SharedPtr(std::move(other)).swap(*this);
		return *this;
	}

	template<typename U>
	SharedPtr& operator=(SharedPtr<U, MT>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		SharedPtr(std::move(other)).swap(*this);
		return *this;
	}

	T* operator->() const noexcept
	{
		CZ_CHECK(m_control.ctrl);
		return m_control.ctrl->toObject<T>();
	}

	T* get() const noexcept
	{
		if(m_control.ctrl)
		{
			return m_control.ctrl->template toObject<T>();
		}
		else
		{
			return nullptr;
		}
	}

	T& operator*() const noexcept
	{
		CZ_CHECK(m_control.ctrl);
		return *m_control.ctrl->toObject<T>();
	}

	explicit operator bool() const noexcept
	{
		return m_control.ctrl ? true : false;
	}

	uint32_t use_count() const noexcept
	{
		return m_control.ctrl ? m_control.ctrl->strongRefs() : 0;
	}

	uint32_t weak_use_count() const noexcept
	{
		return m_control.ctrl ? m_control.ctrl->weakRefs() : 0;
	}

	bool unique() const noexcept
	{
		return use_count() == 1;
	}

	void reset() noexcept
	{
		m_control.release();
	}

	void swap(SharedPtr& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	// Explicit access to underlying SharedPtr
	SharedRef<T, MT> toSharedRef() const noexcept
	{
		CZ_CHECK(*this);
		return SharedRef<T, MT>(*this);
	}

	SharedPtrTraces getTraces() const noexcept
	{
#if CZ_SHAREDPTR_STACKTRACES
		return m_control.ctrl ? m_control.ctrl->getTraces() : SharedPtrTraces{};
#else
		return {};
#endif
	}


  private:

	// This is private, since only WeakPtr::lock can use it
	SharedPtr(details::SharedPtrControlBlock<T, MT>* control) noexcept
	{
		acquireBlock<false>(control);
	}

	template<bool doInc, typename U>
	void acquireBlock(details::SharedPtrControlBlock<U, MT>* control) noexcept
	{
		static_assert(std::is_convertible_v<U*,T*>);
		m_control.ctrl = reinterpret_cast<ControlBlock*>(control);
#if CZ_SHAREDPTR_STACKTRACES
		m_control.trace = nullptr;
#endif
		if (m_control.ctrl)
		{
			if constexpr(doInc)
				m_control.ctrl->incStrong();

#if CZ_SHAREDPTR_STACKTRACES
			m_control.trace = m_control.ctrl->createStackTrace(details::SharedPtrTrace::Type::StrongRef);
#endif
		}
	}

	// Using a struct instead of ControlBlock directly, 
	// so it's easier to deal with the stack traces (if enabled)
	struct ControlHolder
	{
		ControlBlock* ctrl = nullptr;
#if CZ_SHAREDPTR_STACKTRACES
		std::unique_ptr<details::SharedPtrTrace> trace;
#endif

		void release()
		{
			if (ctrl)
			{
				// This needs to be before decStrong.
				// If it was after decStrong, it meant if the decStrong caused the control block to be destroyed, then destroying
				// the trace after that would cause a use-after-free.
				#if CZ_SHAREDPTR_STACKTRACES
				trace = nullptr;
				#endif

				ctrl->decStrong();
				ctrl = nullptr;
			}
		}
	} m_control;
};

template<typename T, bool MT, bool IsObserver>
class WeakPtrImpl
{
  public:

	using ControlBlock = details::SharedPtrControlBlock<T, MT>;

	using pointer = T*;
	using element_type = T;

	template<typename U, bool OtherMT, bool IsObserver>
	friend class WeakPtrImpl;

	constexpr WeakPtrImpl() = default;

	WeakPtrImpl(const WeakPtrImpl& other) noexcept
	{
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U, bool OtherIsObserver>
	WeakPtrImpl(const WeakPtrImpl<U, MT, OtherIsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U>
	WeakPtrImpl(const SharedPtr<U, MT>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	WeakPtrImpl(WeakPtrImpl&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U, bool OtherIsObserver>
	WeakPtrImpl(WeakPtrImpl<U, MT, OtherIsObserver>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		std::swap(m_control, reinterpret_cast<ControlHolder&>(other.m_control));
	}

	~WeakPtrImpl() noexcept
	{
		m_control.release();
	}

	WeakPtrImpl& operator=(const WeakPtrImpl& other) noexcept
	{
		WeakPtrImpl(other).swap(*this);
		return *this;
	}

	template<typename U, bool OtherIsObserver>
	WeakPtrImpl& operator=(const WeakPtrImpl<U, MT, OtherIsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		WeakPtrImpl(other).swap(*this);
		return *this;
	}

	WeakPtrImpl& operator=(WeakPtrImpl&& other) noexcept
	{
		WeakPtrImpl(std::move(other)).swap(*this);
		return *this;
	}

	template<typename U, bool OtherIsObserver>
	WeakPtrImpl& operator=(WeakPtrImpl<U, MT, OtherIsObserver>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		WeakPtrImpl(std::move(other)).swap(*this);
		return *this;
	}

	void reset() noexcept
	{
		WeakPtrImpl{}.swap(*this);
	}

	void swap(WeakPtrImpl& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	uint32_t use_count() const noexcept
	{
		return m_control.ctrl ? m_control.ctrl->strongRefs() : 0;
	}

	uint32_t weak_use_count() const noexcept
	{
		return m_control.ctrl ? m_control.ctrl->weakRefs() : 0;
	}

	bool expired() const noexcept
	{
		return use_count() == 0 ? true : false;
	}

	/**
	 * Promotes the WeakPtr to a SharedPtr.
	 * This is only available if IsObserver==false
	 */
	SharedPtr<T, MT> lock() const noexcept
		requires(IsObserver == false)
	{
		if (!m_control.ctrl)
			return {};

		if (m_control.ctrl->lockStrong())
		{
			return SharedPtr<T, MT>(m_control.ctrl);
		}
		else
		{
			return {};
		}
	}

	/**
	 * Gets the raw pointer. This is only available for ObserverPtr, and if MT==false.
	 * The reason it is not available for MT==true, is because in a multi-threaded context, the object could be destroyed right
	 * after this function returns, so the raw pointer would likely be a dangling pointer.
	 */
	T* tryGet() noexcept
		requires(IsObserver == true && MT == false)
	{
		if (use_count())
		{
			return m_control.ctrl->obj();
		}
		else
		{
			// If expired, then release the block.
			// This is so WeakPtr/ObserverPtr release control blocks as soon as possible (to free up memory)
			m_control.release();

			return nullptr;
		}
	}

	SharedPtrTraces getTraces() const noexcept
	{
#if CZ_SHAREDPTR_STACKTRACES
		return m_control.ctrl ? m_control.ctrl->getTraces() : SharedPtrTraces{};
#else
		return {};
#endif
	}
  private:

	template<typename U>
	void acquireBlock(details::SharedPtrControlBlock<U, MT>* control) noexcept
	{
		static_assert(std::is_convertible_v<U*,T*>);
		m_control.ctrl = reinterpret_cast<ControlBlock*>(control);
#if CZ_SHAREDPTR_STACKTRACES
		m_control.trace = nullptr;
#endif
		if (m_control.ctrl)
		{
			m_control.ctrl->incWeak();
#if CZ_SHAREDPTR_STACKTRACES
			m_control.trace = m_control.ctrl->createStackTrace(details::SharedPtrTrace::Type::WeakRef);
#endif
		}
	}

	// Using a struct instead of ControlBlock directly, 
	// so it's easier to deal with the stack traces (if enabled)
	struct ControlHolder
	{
		ControlBlock* ctrl = nullptr;
#if CZ_SHAREDPTR_STACKTRACES
		std::unique_ptr<details::SharedPtrTrace> trace;
#endif

		void release()
		{
			if (ctrl)
			{
				// This needs to be before decStrong.
				// If it was after decStrong, it meant if the decStrong caused the control block to be destroyed, then destroying
				// the trace after that would cause a use-after-free.
				#if CZ_SHAREDPTR_STACKTRACES
				trace = nullptr;
				#endif

				ctrl->decWeak();
				ctrl = nullptr;
			}
		}
	} m_control;
};

template <typename T, bool MT>
using WeakPtr = WeakPtrImpl<T, MT, false>;

template <typename T, bool MT>
using ObserverPtr = WeakPtrImpl<T, MT, true>;

/**
 * SharedRef<T> : non-nullable SharedPtr<T>
 *
 * Invariants:
 *  - m_ptr.get() is never nullptr.
 *  - No default constructor.
 *  - Construction/assignment from SharedPtr/SharedRef checks non-null
 *
 * This is intentionally a thin wrapper over SharedPtr<T>, so it shares the same
 * control block and ref-counting behavior, but expresses intent in the type.
 *
 * IMPORTANT IMPLEMENTATION DETAILS:
 * - No move contructors/assignments are provided, since SharedRef must always be non-null.
 */
template<typename T, bool MT>
class SharedRef
{
  public:

	using SharedPtrT = SharedPtr<T, MT>;

	template<typename U, bool OtherMT>
	friend class SharedRef;

	/*!
	 * No default construction, since SharedRef must always be non-null.
	 */
	SharedRef() = delete;

	SharedRef(const SharedRef& other)
		: m_ptr(other.m_ptr)
	{
		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again,
		// incase there was a bug somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	SharedRef(const SharedRef<U, MT>& other)
		: m_ptr(other.m_ptr)
	{
		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again, incase there was a bug
		// somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	explicit SharedRef(const SharedPtr<U, MT>& other) noexcept
		: m_ptr(other)
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	explicit SharedRef(SharedPtr<U, MT>&& other) noexcept
		: m_ptr(std::move(other))
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	#if 0
	/*! Assignment from convertible SharedPtr<U> */
	template<typename U>
	SharedRef& operator=(const SharedPtr<U, MT>& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = other;
		return *this;
	}

	/*! Assignment from convertible SharedPtr<U> rvalue */
	template<typename U>
	SharedRef& operator=(SharedPtr<U, MT>&& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = std::move(other);
		return *this;
	}
	#else
	// Don't allow assignment from SharedPtr, so the user needs to be explicit when converting.
	template<typename U>
	SharedRef& operator=(const SharedPtr<U, MT>& other) noexcept = delete;
	template<typename U>
	SharedRef& operator=(SharedPtr<U, MT>&& other) noexcept = delete;
	#endif

	SharedRef& operator=(const SharedRef& other) noexcept = default;

	template<typename U>
	SharedRef& operator=(const SharedRef<U, MT>& other) noexcept
	{
		CZ_CHECK(other.m_ptr.get() != nullptr);
		m_ptr = other.m_ptr;
		return *this;
	}

	// NOTE: No operator bool(), since SharedRef is always non-null.

	T* operator->() const noexcept
	{
		return m_ptr.get();
	}

	T& operator*() const noexcept
	{
		return *m_ptr.get();
	}

	T* get() const noexcept
	{
		return m_ptr.get();
	}

	// Expose SharedPtr-like utilities where useful

	uint32_t use_count() const noexcept
	{
		return m_ptr.use_count();
	}

	bool unique() const noexcept
	{
		return m_ptr.unique();
	}

	// Explicit access to underlying SharedPtr
	const SharedPtrT& toSharedPtr() const noexcept
	{
		return m_ptr;
	}

	/*!
	 * Implicit conversion to SharedPtr<T>, so APIs taking a SharedPtr<T> can accept SharedRef<T> without changes.
	 */
	operator SharedPtrT() const noexcept
	{
		return m_ptr;
	}

	void swap(SharedRef& other) noexcept
	{
		m_ptr.swap(other.m_ptr);
	}

  private:
	SharedPtrT m_ptr;
};

/**
 * Equivalent to std::enable_shared_from_this
 * 
 * Usage:
 *   class MyClass : public EnableSharedFromThis<MyClass> {
 *   public:
 *       void someMethod() {
 *           SharedPtr<MyClass> sharedThis = sharedFromThis();
 *           // Use sharedThis...
 *       }
 *   };
 *
 *
 * This should only be used with instances that are allocated via makeShared or properly allocated with
 * BaseSharedPtrControlBlock::allocBlock, so that the control block is set up correctly.
 *
 */
template <typename T, bool MT>
class EnableSharedFromThis
{
  protected:
	constexpr EnableSharedFromThis() noexcept = default;
	EnableSharedFromThis(const EnableSharedFromThis&) noexcept = default;
	EnableSharedFromThis& operator=(const EnableSharedFromThis&) noexcept = default;
	~EnableSharedFromThis() = default;

  public:

	static constexpr bool EnableSharedFromThis_MT = MT;

	/**
	 * Returns a SharedPtr<T> to this object.
	 *
	 * NOTE: Assumes the object was allocated via makeShared or proper allocation.
	 * Using this on a stack-allocated or improperly allocated object will crash.
	 */
	SharedPtr<T, MT> sharedFromThis()
	{
		T* derivedThis = static_cast<T*>(this);
		return SharedPtr<T, MT>(derivedThis);
	}

	/**
	 * Returns a SharedPtr<T> to this object.
	 *
	 * NOTE: Assumes the object was allocated via makeShared or proper allocation.
	 * Using this on a stack-allocated or improperly allocated object will crash.
	 */
	SharedPtr<const T, MT> sharedFromThis() const
	{
		const T* derivedThis = static_cast<const T*>(this);
		// Need to cast away const because SharedPtr constructor expects non-const
		return SharedPtr<const T, MT>(const_cast<T*>(derivedThis));
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	WeakPtr<T, MT> weakFromThis()
	{
		return WeakPtr<T, MT>(sharedFromThis());
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	WeakPtr<const T, MT> weakFromThis() const
	{
		return WeakPtr<const T, MT>(sharedFromThis());
	}
};

//
// SharedPtr utilities
//

/**
 * Constructs a SharedPtr<T> with the specified parameters.
 * If uses `T::SharedPtrDeleter` as the deleter if it exists, otherwise uses the default deleter.
 */
template <typename T, bool MT, typename... Args>
SharedPtr<T, MT> makeShared(Args&& ... args)
{
	static_assert(!std::is_abstract_v<T>, "Type is abstract.");
	void* ptr = details::allocSharedPtrBlock<T, MT>();
	return SharedPtr<T, MT>(new(ptr) T(std::forward<Args>(args)...));
}

template<class T, bool MT, class U>
SharedPtr<T, MT> static_pointer_cast(const SharedPtr<U, MT>& other) noexcept
{
	return SharedPtr<T, MT>(static_cast<T*>(other.get()));
}

template<class T, bool MT, class U>
SharedPtr<T, MT> static_pointer_cast(SharedPtr<U, MT>&& other) noexcept
{
	SharedPtr<U, MT> other_(std::move(other));
	return SharedPtr<T, MT>(static_cast<T*>(other_.get()));
}

template <class T, class U, bool MT>
bool operator==(const SharedPtr<T, MT>& left, const SharedPtr<U, MT>& right) noexcept
{
	return left.get() == right.get();
}

template <class T, bool MT>
bool operator==(const SharedPtr<T, MT>& left, std::nullptr_t) noexcept
{
	return left.get() == nullptr;
}

template <class T, bool MT>
bool operator==(std::nullptr_t, const SharedPtr<T, MT>& right) noexcept
{
	return nullptr == right.get();
}

//The <, <=, >, >=, and != operators are synthesized from operator<=> and operator== respectively.
template<typename T1, typename T2, bool MT>
std::strong_ordering operator<=>(const SharedPtr<T1, MT>& left, const SharedPtr<T2, MT>& right) noexcept
{
	return left.get() <=> right.get();
}

//
// SharedRef utilities
//

template<typename T, bool MT, typename... Args>
SharedRef<T, MT> makeSharedRef(Args&&... args)
{
	return SharedRef<T, MT>(makeShared<T, MT>(std::forward<Args>(args)...));
}

// Casting helpers similar to SharedPtr versions
template<class T, class U, bool MT>
SharedRef<T, MT> static_pointer_cast(const SharedRef<U, MT>& other) noexcept
{
	// other is guaranteed non-null, so resulting SharedPtr<T> is also non-null
	SharedPtr<T, MT> casted = static_pointer_cast<T, U, MT>(other.toSharedPtr());
	CZ_CHECK(casted.get() != nullptr);
	return SharedRef<T, MT>(casted);
}

template<class T, class U, bool MT>
SharedRef<T, MT> static_pointer_cast(SharedRef<U, MT>&& other) noexcept
{
	SharedPtr<U, MT> tmp = other.toSharedPtr(); // copy, keep it simple. We can't actually move, because SharedRef doesn't have move semantics.
	SharedPtr<T, MT> casted = static_pointer_cast<T, U, MT>(tmp);
	CZ_CHECK(casted.get() != nullptr);
	return SharedRef<T, MT>(casted);
}

// Comparisons

template <class T, class U, bool MT>
bool operator==(const SharedRef<T, MT>& left, const SharedRef<U, MT>& right) noexcept
{
	return left.get() == right.get();
}

template<typename T1, typename T2, bool MT>
std::strong_ordering operator<=>(const SharedRef<T1, MT>& left, const SharedRef<T2, MT>& right) noexcept
{
	return left.get() <=> right.get();
}


//
// Utilities to make it easier to use EnableSharedFromThis with class hiearchies by doing the required cast.
//

template <typename T, typename U>
SharedPtr<T, U::EnableSharedFromThis_MT> toStrong(U* obj)
{
	
	return static_pointer_cast<T, T::EnableSharedFromThis_MT>(obj->sharedFromThis());
}

template <typename T>
SharedPtr<const T, T::EnableSharedFromThis_MT> toStrong(const T* obj)
{
	return static_pointer_cast<const T, T::EnableSharedFromThis_MT>(obj->sharedFromThis());
}

template <typename T>
SharedPtr<T, T::EnableSharedFromThis_MT> toStrong(T& obj)
{
	return static_pointer_cast<T, T::EnableSharedFromThis_MT>(obj.sharedFromThis());
}

template <typename T>
SharedPtr<const T, T::EnableSharedFromThis_MT> toStrong(const T& obj)
{
	return static_pointer_cast<const T, T::EnableSharedFromThis_MT>(obj.sharedFromThis());
}


} // namespace cz

