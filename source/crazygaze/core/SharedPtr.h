#pragma once

#include "Common.h"
#include "Logging.h"


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
 * To enable support, set the CZCORE_ENABLE_STACKTRACES CMake option to ON.
 * Setting that option compiles in stack trace support, BUT doesn't enable it by default, to minimize performance impact.
 *
 * To enable stack traces for a specific type T you can:
 * - Define a static member `enable_sharedptr_stacktraces` in your type:
 *		``static constexpr bool enable_sharedptr_stacktraces = true;```
 * - To enable stack traces for all types, you define CZ_SHAREDPTR_STACKTRACES_DEFAULT macro as `std::true_type`
 */

#ifndef CZ_SHAREDPTR_STACKTRACES
	#if CZ_DEBUG || CZ_DEVELOPMENT 
		#define CZ_SHAREDPTR_STACKTRACES 1
	#else
		#define CZ_SHAREDPTR_STACKTRACES 0
	#endif
#endif

#ifndef CZ_SHAREDPTR_STACKTRACES_DEFAULT
	// By default, stack traces are disabled for all types, to minimize performance impact.
	#define CZ_SHAREDPTR_STACKTRACES_DEFAULT std::false_type
#endif

#if CZ_SHAREDPTR_STACKTRACES
	#include "crazygaze/core/LinkedList.h"
#endif


namespace cz
{

template<typename T>
inline void sharedPtrDeleter(T* obj)
{
	obj->~T();
}

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
	struct SharedPtrTrace : public DoublyLinked<SharedPtrTrace>
	{
		enum class Type
		{
			Creation,
			StrongRef,
			WeakRef
		};

		explicit SharedPtrTrace(Type type, DoublyLinkedList<SharedPtrTrace>* outer)
			: type(type)
			, outer(outer)
		{
			if (outer)
			{
				timestamp = std::chrono::high_resolution_clock::now();
				frame = gFrameCounter.load();
				trace = std::stacktrace::current();
				outer->pushBack(this);
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
		DoublyLinkedList<SharedPtrTrace>* outer = nullptr;
	};

#endif

	class BaseSharedPtrControlBlock
	{
	  public:

		BaseSharedPtrControlBlock([[maybe_unused]] size_t size)
#if CZ_SHAREDPTR_CLEAR_MEM
			: size(size)
#endif
		{
		}

		template<typename T>
		T* toObject()
		{
			return reinterpret_cast<T*>(this+1);
		}

		void incStrong()
		{
			++strong;
		}

		void incWeak()
		{
			++weak;
		}

		void decWeak()
		{
			assert(weak > 0);
			--weak;
			if (weak == 0 && strong == 0)
			{
				free(this);
			}
		}

		unsigned int strongRefs() const
		{
			return strong;
		}

		unsigned int weakRefs() const
		{
			return weak;
		}

		/**
		 * Allocates memory for a control block + a T object
		 *
		 * Returns the pointer that can be used to construct a T object with placement new
		 */
		template<typename T>
		static void* allocBlock()
		{
			size_t allocSize = sizeof(BaseSharedPtrControlBlock) + sizeof(T);
			void* basePtr = malloc(allocSize);
			BaseSharedPtrControlBlock* control = new (basePtr) BaseSharedPtrControlBlock(sizeof(T));

			#if CZ_SHAREDPTR_STACKTRACES
			if constexpr(details::enable_sharedptr_stacktraces_v<T>)
			{
				control->traceData = std::make_unique<StackTraceData>();
				control->traceData->firstTrace = control->createStackTrace(SharedPtrTrace::Type::Creation);
			}
			#endif

			return control + 1;
		}

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
				for(const SharedPtrTrace* ele : traceData->traceList)
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
				}
			}
			return res;
		}
		#endif

	  protected:

		#if CZ_SHAREDPTR_STACKTRACES
		struct StackTraceData
		{
			DoublyLinkedList<SharedPtrTrace> traceList;
			std::unique_ptr<SharedPtrTrace> firstTrace;
		};
		// Intentionally putting this in a unique_ptr, so it's out of the same cache line, and it's not loaded every time
		// we need to dereference a SharedPtr/WeakPtr.
		std::unique_ptr<StackTraceData> traceData;
		#endif

		unsigned int weak = 0;
		unsigned int strong = 0;

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

	template<typename T, typename Deleter = SharedPtrDefaultDeleter>
	class SharedPtrControlBlock : public BaseSharedPtrControlBlock
	{
	  public:

		SharedPtrControlBlock()
		{
			// The only difference between the base object and a specialized one is that the specialized one adds some more methods.
			static_assert(sizeof(*this) == sizeof(BaseSharedPtrControlBlock));
		}

		T* obj()
		{
			if (strong)
			{
				return toObject<T>();
			}
			else
			{
				return nullptr;
			}
		}

		void decStrong()
		{
			assert(strong > 0);
			if (strong == 1)
			{
				auto ptr = obj();
				Deleter{}(ptr);
				#if CZ_SHAREDPTR_CLEAR_MEM
					memset(ptr, 0xDD, size);
				#endif
			}

			--strong;
			if (strong == 0 && weak == 0)
			{
				free(this);
			}
		}

	};
}

template<typename T, typename Deleter>
class SharedRef;

/**
 * A rather simplistic std::shared_ptr equivalent, that is faster than std::shared_ptr since it is NOT thread safe.
 * Things to be aware of:
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
template<typename T, typename Deleter = details::SharedPtrDefaultDeleter>
class SharedPtr
{
  public:

	template<typename U, typename UDeleter, bool IsObserver>
	friend class WeakPtrImpl;

	template<typename U, typename UDeleter>
	friend class SharedPtr;

	using ControlBlock = details::SharedPtrControlBlock<T, Deleter>;	

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
			void* rawPtr = (reinterpret_cast<uint8_t*>(static_cast<T*>(ptr)) - sizeof(details::BaseSharedPtrControlBlock));
			acquireBlock(reinterpret_cast<ControlBlock*>(rawPtr));
		}
	}

	~SharedPtr() noexcept
	{
		m_control.release();
	}

	SharedPtr(const SharedPtr& other) noexcept
	{
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U>
	SharedPtr(const SharedPtr<U, Deleter>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	SharedPtr(SharedPtr&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U>
	SharedPtr(SharedPtr<U>&& other) noexcept
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
	SharedPtr& operator=(const SharedPtr<U, Deleter>& other) noexcept
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
	SharedPtr& operator=(SharedPtr<U, Deleter>&& other) noexcept
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

	unsigned int use_count() const noexcept
	{
		return m_control.ctrl ? m_control.ctrl->strongRefs() : 0;
	}

	unsigned int weak_use_count() const noexcept
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
	SharedRef<T, Deleter> toSharedRef() const noexcept
	{
		CZ_CHECK(*this);
		return SharedRef<T,Deleter>(*this);
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
	void acquireBlock(details::SharedPtrControlBlock<U, Deleter>* control) noexcept
	{
		static_assert(std::is_convertible_v<U*,T*>);
		m_control.ctrl = reinterpret_cast<ControlBlock*>(control);
#if CZ_SHAREDPTR_STACKTRACES
		m_control.trace = nullptr;
#endif
		if (m_control.ctrl)
		{
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
				ctrl->decStrong();
				ctrl = nullptr;
#if CZ_SHAREDPTR_STACKTRACES
				trace = nullptr;
#endif
			}
		}
	} m_control;
};

template<typename T, typename Deleter, bool IsObserver>
class WeakPtrImpl
{
  public:

	using ControlBlock = details::SharedPtrControlBlock<T, Deleter>;

	template<typename U, typename UDeleter, bool IsObserver>
	friend class WeakPtrImpl;

	constexpr WeakPtrImpl() = default;

	WeakPtrImpl(const WeakPtrImpl& other) noexcept
	{
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U, bool IsObserver>
	WeakPtrImpl(const WeakPtrImpl<U, Deleter, IsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U>
	WeakPtrImpl(const SharedPtr<U, Deleter>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	WeakPtrImpl(WeakPtrImpl&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	~WeakPtrImpl() noexcept
	{
		m_control.release();
	}

	template<typename U, bool IsObserver>
	WeakPtrImpl(WeakPtrImpl<U, Deleter, IsObserver>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		std::swap(m_control, reinterpret_cast<ControlHolder&>(other.m_control));
	}

	WeakPtrImpl& operator=(const WeakPtrImpl& other) noexcept
	{
		WeakPtrImpl(other).swap(*this);
		return *this;
	}

	template<typename U, bool IsObserver>
	WeakPtrImpl& operator=(const WeakPtrImpl<U, Deleter, IsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		WeakPtrImpl(other).swap(*this);
		return *this;
	}

	template<typename U>
	WeakPtrImpl& operator=(const SharedPtr<U, Deleter>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		WeakPtrImpl(other).swap(*this);
		return *this;
	}

	template<typename U, bool IsObserver>
	WeakPtrImpl& operator=(WeakPtrImpl<U, Deleter, IsObserver>&& other) noexcept
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

	unsigned int use_count() const noexcept
	{
		return m_control.ctrl ? m_control.ctrl->strongRefs() : 0;
	}

	unsigned int weak_use_count() const noexcept
	{
		return m_control.ctrl ? m_control.ctrl->weakRefs() : 0;
	}

	bool expired() const noexcept
	{
		return use_count() == 0 ? true : false;
	}

	/**
	 * Promotes the WeakPtr to a SharedPtr.
	 * This is only available is IsObserver==false
	 */
	SharedPtr<T, Deleter> lock() const noexcept
		requires(IsObserver == false)
	{
		if (use_count())
		{
			return SharedPtr<T, Deleter>(m_control.ctrl->obj());
		}
		else
		{
			return SharedPtr<T, Deleter>();
		}
	}

	/**
	 * Gets the raw pointer. This is only available for ObserverPtr.
	 */
	template<bool B = IsObserver, typename = std::enable_if_t<B>> 
	T* tryGet() noexcept
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
	void acquireBlock(details::SharedPtrControlBlock<U, Deleter>* control) noexcept
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
				ctrl->decWeak();
				ctrl = nullptr;
#if CZ_SHAREDPTR_STACKTRACES
				trace = nullptr;
#endif
			}
		}
	} m_control;
};

template <typename T, typename Deleter = details::SharedPtrDefaultDeleter>
using WeakPtr = WeakPtrImpl<T, Deleter, false>;

template <typename T, typename Deleter = details::SharedPtrDefaultDeleter>
using ObserverPtr = WeakPtrImpl<T, Deleter, true>;

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
 */
template<typename T, typename Deleter = details::SharedPtrDefaultDeleter>
class SharedRef
{
  public:

	using SharedPtrT = SharedPtr<T, Deleter>;

	template<typename U, typename UDeleter>
	friend class SharedRef;

	/*!
	 * No default construction, since SharedRef must always be non-null.
	 */
	SharedRef() = delete;

	template<typename U>
	explicit SharedRef(const SharedPtr<U, Deleter>& other) noexcept
		: m_ptr(other)
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	explicit SharedRef(SharedPtr<U, Deleter>&& other) noexcept
		: m_ptr(std::forward<SharedPtr<U, Deleter>>(other))
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	SharedRef(const SharedRef& other)
		: m_ptr(other.m_ptr)
	{
		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again,
		// incase there was a bug somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	SharedRef(SharedRef&& other)
		: m_ptr(other.m_ptr)
	{
		// Note that we are NOT moving the other.m_ptr, since SharedRef must always be non-null.

		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again, incase there was a bug
		// somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	SharedRef(const SharedRef<U, Deleter>& other)
		: m_ptr(other.m_ptr)
	{
		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again, incase there was a bug
		// somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	SharedRef(SharedRef<U, Deleter>&& other)
		: m_ptr(other.m_ptr)
	{
		// Note that we are NOT moving the other.m_ptr, since SharedRef must always be non-null.

		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again,
		// incase there was a bug somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	#if 0
	/*! Assignment from convertible SharedPtr<U> */
	template<typename U>
	SharedRef& operator=(const SharedPtr<U, Deleter>& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = other;
		return *this;
	}

	/*! Assignment from convertible SharedPtr<U> rvalue */
	template<typename U>
	SharedRef& operator=(SharedPtr<U, Deleter>&& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = std::move(other);
		return *this;
	}
	#else
	// Don't allow assignment from SharedPtr, so the user needs to be explicit when converting.
	template<typename U>
	SharedRef& operator=(const SharedPtr<U, Deleter>& other) noexcept = delete;
	template<typename U>
	SharedRef& operator=(SharedPtr<U, Deleter>&& other) noexcept = delete;
	#endif

	SharedRef& operator=(const SharedRef& other) noexcept = default;

	template<typename U>
	SharedRef& operator=(const SharedRef<U, Deleter>& other) noexcept
	{
		CZ_CHECK(other.m_ptr.get() != nullptr);
		m_ptr = other.m_ptr;
		return *this;
	}

	/*! Move assignment */
	SharedRef& operator=(SharedRef&& other) noexcept
	{
		CZ_CHECK(other.m_ptr.get() != nullptr);
		m_ptr = other.m_ptr;
		return *this;
	}

	template<typename U>
	SharedRef& operator=(SharedRef<U, Deleter>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
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

	unsigned int use_count() const noexcept
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

//
// SharedPtr utilities
//

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&& ... args)
{
	static_assert(!std::is_abstract_v<T>, "Type is abstract.");
	void* ptr = details::BaseSharedPtrControlBlock::allocBlock<T>();
	return SharedPtr<T>(new(ptr) T(std::forward<Args>(args)...));
}

template<class T, class U>
SharedPtr<T> static_pointer_cast(const SharedPtr<U>& other) noexcept
{
	return SharedPtr<T>(static_cast<T*>(other.get()));
}

template<class T, class U>
SharedPtr<T> static_pointer_cast(SharedPtr<U>&& other) noexcept
{
	SharedPtr<U> other_(std::move(other));
	return SharedPtr<T>(static_cast<T*>(other_.get()));
}

template <class T, class U, typename Deleter>
bool operator==(const SharedPtr<T, Deleter>& left, const SharedPtr<U, Deleter>& right) noexcept
{
	return left.get() == right.get();
}

template <class T, typename Deleter>
bool operator==(const SharedPtr<T, Deleter>& left, std::nullptr_t) noexcept
{
	return left.get() == nullptr;
}

template <class T, typename Deleter>
bool operator==(std::nullptr_t, const SharedPtr<T, Deleter>& right) noexcept
{
	return nullptr == right.get();
}

//The <, <=, >, >=, and != operators are synthesized from operator<=> and operator== respectively.
template<typename T1, typename T2, typename Deleter>
std::strong_ordering operator<=>(const SharedPtr<T1, Deleter>& left, const SharedPtr<T2, Deleter>& right) noexcept
{
	return left.get() <=> right.get();
}

//
// SharedRef utilities
//

template<typename T, typename... Args>
SharedRef<T> makeSharedRef(Args&&... args)
{
	return SharedRef<T>(makeShared<T>(std::forward<Args>(args)...));
}

// Casting helpers similar to SharedPtr versions
template<class T, class U, typename Deleter>
SharedRef<T, Deleter> static_pointer_cast(const SharedRef<U, Deleter>& other) noexcept
{
	// other is guaranteed non-null, so resulting SharedPtr<T> is also non-null
	SharedPtr<T, Deleter> casted = static_pointer_cast<T>(other.toSharedPtr());
	CZ_CHECK(casted.get() != nullptr);
	return SharedRef<T, Deleter>(casted);
}

template<class T, class U, typename Deleter>
SharedRef<T, Deleter> static_pointer_cast(SharedRef<U, Deleter>&& other) noexcept
{
	SharedPtr<U, Deleter> tmp = other.toSharedPtr(); // copy, keep simple
	SharedPtr<T, Deleter> casted = static_pointer_cast<T>(tmp);
	CZ_CHECK(casted.get() != nullptr);
	return SharedRef<T, Deleter>(casted);
}

// Comparisons

template <class T, class U, typename Deleter>
bool operator==(const SharedRef<T, Deleter>& left, const SharedRef<U, Deleter>& right) noexcept
{
	return left.get() == right.get();
}

template<typename T1, typename T2, typename Deleter>
std::strong_ordering operator<=>(const SharedRef<T1, Deleter>& left, const SharedRef<T2, Deleter>& right) noexcept
{
	return left.get() <=> right.get();
}



} // namespace cz

