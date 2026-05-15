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

		virtual ~BaseSharedPtrControlBlock() = default;

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

		template<typename T, typename Deleter>
		friend void* allocSharedPtrBlock();

		#if CZ_SHAREDPTR_STACKTRACES
		struct StackTraceData
		{
			~StackTraceData()
			{
				firstTrace = nullptr;

				// If there are still any traces in the list, then we have a bug somewhere in this code.
				CZ_CHECK(traceList.back() == nullptr);
			}
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

	template<typename T>
	class SharedPtrControlBlock : public BaseSharedPtrControlBlock
	{
	  public:

		SharedPtrControlBlock([[maybe_unused]] size_t size)
			: BaseSharedPtrControlBlock(size)
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
				deleteObj();
			}

			--strong;
			if (strong == 0 && weak == 0)
			{
				// We need to explicitly call the virtual destructor
				this->~SharedPtrControlBlock();
				free(this);
			}
		}

	};

	template <typename T, typename Deleter = SharedPtrDefaultDeleter>
	class SharedPtrControlBlockWithDeleter : public SharedPtrControlBlock<T>
	{
	  public:
		SharedPtrControlBlockWithDeleter([[maybe_unused]] size_t size)
			: SharedPtrControlBlock<T>(size)
		{
			// The only difference between the base object and a specialized one is that the specialized one adds some more methods.
			static_assert(sizeof(*this) == sizeof(SharedPtrControlBlock<T>));
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
	template<typename T, typename Deleter = SharedPtrDeleterFor<T>>
	static void* allocSharedPtrBlock()
	{
		size_t allocSize = sizeof(SharedPtrControlBlockWithDeleter<T, Deleter>) + sizeof(T);
		void* basePtr = malloc(allocSize);
		BaseSharedPtrControlBlock* control = new (basePtr) SharedPtrControlBlockWithDeleter<T, Deleter>(sizeof(T));

		#if CZ_SHAREDPTR_STACKTRACES
		if constexpr(details::enable_sharedptr_stacktraces_v<T>)
		{
			control->traceData = std::make_unique<BaseSharedPtrControlBlock::StackTraceData>();
			control->traceData->firstTrace = control->createStackTrace(SharedPtrTrace::Type::Creation);
		}
		#endif

		return control + 1;
	}

} // namespace details

template<typename T>
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
 * 
 * 
 */
template<typename T>
class SharedPtr
{
  public:

	template<typename U, bool IsObserver>
	friend class WeakPtrImpl;

	template<typename U>
	friend class SharedPtr;

	using ControlBlock = details::SharedPtrControlBlock<T>;	

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
			const void* rawPtr = (reinterpret_cast<const uint8_t*>(static_cast<T*>(ptr)) - sizeof(details::BaseSharedPtrControlBlock));
			acquireBlock(reinterpret_cast<ControlBlock*>(const_cast<void*>(rawPtr)));
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
	SharedPtr(const SharedPtr<U>& other) noexcept
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
	SharedPtr& operator=(const SharedPtr<U>& other) noexcept
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
	SharedPtr& operator=(SharedPtr<U>&& other) noexcept
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
	SharedRef<T> toSharedRef() const noexcept
	{
		CZ_CHECK(*this);
		return SharedRef<T>(*this);
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
	void acquireBlock(details::SharedPtrControlBlock<U>* control) noexcept
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

template<typename T, bool IsObserver>
class WeakPtrImpl
{
  public:

	using ControlBlock = details::SharedPtrControlBlock<T>;

	using pointer = T*;
	using element_type = T;

	template<typename U, bool IsObserver>
	friend class WeakPtrImpl;

	constexpr WeakPtrImpl() = default;

	WeakPtrImpl(const WeakPtrImpl& other) noexcept
	{
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U, bool IsObserver>
	WeakPtrImpl(const WeakPtrImpl<U, IsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U>
	WeakPtrImpl(const SharedPtr<U>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	WeakPtrImpl(WeakPtrImpl&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U, bool IsObserver>
	WeakPtrImpl(WeakPtrImpl<U, IsObserver>&& other) noexcept
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

	template<typename U, bool IsObserver>
	WeakPtrImpl& operator=(const WeakPtrImpl<U, IsObserver>& other) noexcept
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

	template<typename U, bool IsObserver>
	WeakPtrImpl& operator=(WeakPtrImpl<U, IsObserver>&& other) noexcept
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
	SharedPtr<T> lock() const noexcept
		requires(IsObserver == false)
	{
		if (use_count())
		{
			return SharedPtr<T>(m_control.ctrl->obj());
		}
		else
		{
			return SharedPtr<T>();
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
	void acquireBlock(details::SharedPtrControlBlock<U>* control) noexcept
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

template <typename T>
using WeakPtr = WeakPtrImpl<T, false>;

template <typename T>
using ObserverPtr = WeakPtrImpl<T, true>;

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
template<typename T>
class SharedRef
{
  public:

	using SharedPtrT = SharedPtr<T>;

	template<typename U>
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
	SharedRef(const SharedRef<U>& other)
		: m_ptr(other.m_ptr)
	{
		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again, incase there was a bug
		// somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	explicit SharedRef(const SharedPtr<U>& other) noexcept
		: m_ptr(other)
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	explicit SharedRef(SharedPtr<U>&& other) noexcept
		: m_ptr(std::move(other))
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	#if 0
	/*! Assignment from convertible SharedPtr<U> */
	template<typename U>
	SharedRef& operator=(const SharedPtr<U>& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = other;
		return *this;
	}

	/*! Assignment from convertible SharedPtr<U> rvalue */
	template<typename U>
	SharedRef& operator=(SharedPtr<U>&& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = std::move(other);
		return *this;
	}
	#else
	// Don't allow assignment from SharedPtr, so the user needs to be explicit when converting.
	template<typename U>
	SharedRef& operator=(const SharedPtr<U>& other) noexcept = delete;
	template<typename U>
	SharedRef& operator=(SharedPtr<U>&& other) noexcept = delete;
	#endif

	SharedRef& operator=(const SharedRef& other) noexcept = default;

	template<typename U>
	SharedRef& operator=(const SharedRef<U>& other) noexcept
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
template <typename T>
class EnableSharedFromThis
{
  protected:
	constexpr EnableSharedFromThis() noexcept = default;
	EnableSharedFromThis(const EnableSharedFromThis&) noexcept = default;
	EnableSharedFromThis& operator=(const EnableSharedFromThis&) noexcept = default;
	~EnableSharedFromThis() = default;

  public:
	/**
	 * Returns a SharedPtr<T> to this object.
	 *
	 * NOTE: Assumes the object was allocated via makeShared or proper allocation.
	 * Using this on a stack-allocated or improperly allocated object will crash.
	 */
	SharedPtr<T> sharedFromThis()
	{
		T* derivedThis = static_cast<T*>(this);
		return SharedPtr<T>(derivedThis);
	}

	/**
	 * Returns a SharedPtr<T> to this object.
	 *
	 * NOTE: Assumes the object was allocated via makeShared or proper allocation.
	 * Using this on a stack-allocated or improperly allocated object will crash.
	 */
	SharedPtr<const T> sharedFromThis() const
	{
		const T* derivedThis = static_cast<const T*>(this);
		// Need to cast away const because SharedPtr constructor expects non-const
		return SharedPtr<const T>(const_cast<T*>(derivedThis));
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	WeakPtr<T> weakFromThis()
	{
		return WeakPtr<T>(sharedFromThis());
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	WeakPtr<const T> weakFromThis() const
	{
		return WeakPtr<const T>(sharedFromThis());
	}
};

//
// SharedPtr utilities
//

/**
 * Constructs a SharedPtr<T> with the specified parameters.
 * If uses `T::SharedPtrDeleter` as the deleter if it exists, otherwise uses the default deleter.
 */
template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&& ... args)
{
	static_assert(!std::is_abstract_v<T>, "Type is abstract.");
	void* ptr = details::allocSharedPtrBlock<T>();
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

template <class T, class U>
bool operator==(const SharedPtr<T>& left, const SharedPtr<U>& right) noexcept
{
	return left.get() == right.get();
}

template <class T>
bool operator==(const SharedPtr<T>& left, std::nullptr_t) noexcept
{
	return left.get() == nullptr;
}

template <class T>
bool operator==(std::nullptr_t, const SharedPtr<T>& right) noexcept
{
	return nullptr == right.get();
}

//The <, <=, >, >=, and != operators are synthesized from operator<=> and operator== respectively.
template<typename T1, typename T2>
std::strong_ordering operator<=>(const SharedPtr<T1>& left, const SharedPtr<T2>& right) noexcept
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
template<class T, class U>
SharedRef<T> static_pointer_cast(const SharedRef<U>& other) noexcept
{
	// other is guaranteed non-null, so resulting SharedPtr<T> is also non-null
	SharedPtr<T> casted = static_pointer_cast<T>(other.toSharedPtr());
	CZ_CHECK(casted.get() != nullptr);
	return SharedRef<T>(casted);
}

template<class T, class U>
SharedRef<T> static_pointer_cast(SharedRef<U>&& other) noexcept
{
	SharedPtr<U> tmp = other.toSharedPtr(); // copy, keep it simple. We can't actually move, because SharedRef doesn't have move semantics.
	SharedPtr<T> casted = static_pointer_cast<T>(tmp);
	CZ_CHECK(casted.get() != nullptr);
	return SharedRef<T>(casted);
}

// Comparisons

template <class T, class U>
bool operator==(const SharedRef<T>& left, const SharedRef<U>& right) noexcept
{
	return left.get() == right.get();
}

template<typename T1, typename T2>
std::strong_ordering operator<=>(const SharedRef<T1>& left, const SharedRef<T2>& right) noexcept
{
	return left.get() <=> right.get();
}


//
// Utilities to make it easier to use EnableSharedFromThis
//

template <typename T>
SharedPtr<T> toStrong(T* obj)
{
	return static_pointer_cast<T>(obj->sharedFromThis());
}

template <typename T>
SharedPtr<const T> toStrong(const T* obj)
{
	return static_pointer_cast<const T>(obj->sharedFromThis());
}

template <typename T>
SharedPtr<T> toStrong(T& obj)
{
	return static_pointer_cast<T>(obj.sharedFromThis());
}

template <typename T>
SharedPtr<const T> toStrong(const T& obj)
{
	return static_pointer_cast<const T>(obj.sharedFromThis());
}


} // namespace cz

