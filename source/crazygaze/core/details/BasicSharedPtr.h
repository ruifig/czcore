#pragma once

#include "SmartPtrsHelper.h"

/**
 * MT specifies if it's thread safe or not.
 */

namespace cz::details
{

template<typename T, bool MT>
class BasicSharedRef;

/**
 * A rather simplistic std::shared_ptr equivalent.
 *
 * Things to be aware of:
 *
 *	- The control block is ALWAYS allocated together with the object, therefore it doesn't allow using SharedPtr with objects that were allocated in a different way.
 *		- Allocation should be done with the "makeShared" helper functions.
 * - Casts when multiple inheritance are problematic. You might end up with heap corruption.
 * - Doesn't provide all the functions and operators std::shared_ptr provides.
 * - Custom deleters can be supported by defining a "SharedPtrDeleter" type in your class.
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
 * - Both thread safe and non-thread safe version are supported. (typedef to SharedPtr<T>/WeakPtr<T> and LocalSharedPtr<T>/LocalWeakPtr<T>)
 * - Allows capturing stack traces for debugging purposes
 *		- Setting CZ_SHAREDPTR_STACKTRACES to 1 compiles in stack trace support, but enabling it for a specific class is opt-in.
 *		  You can enable it for a specific class by adding a `static bool captureSharedPtrStackTraces() { return true; }` method to the class.
 *		  By doing this, stack traces can be enabled/disabled at run-time:
 *			- Compile with CZ_SHAREDPTR_STACKTRACES set to 1. This adds little overhead by itself, but should still be disabled in release builds.
 *			- Add a `captureSharedPtrStackTraces` method to the classes you want to capture stack traces for, and return true/false based on a runtime condition.
 */
template<typename T, bool MT>
class BasicSharedPtr
{
  public:

	template<typename U, bool OtherMT, bool IsObserver>
	friend class BasicWeakPtr;

	template<typename U, bool OtherMT>
	friend class BasicSharedPtr;

	template <typename T, bool MT>
	friend class BasicEnableSharedFromThis;

	using ControlBlock = SharedPtrControlBlock<T, MT>;	

	using pointer = T*;
	using element_type = T;

	BasicSharedPtr() noexcept
	{
	}

	BasicSharedPtr(std::nullptr_t) noexcept
	{
	}

	/**
	 * Constructs the the shared pointer from a previously allocated object.
	 * IMPORTANT: The object's memory MUST have been allocated with details::allocSharedPtrBlock.
	 */
	template<typename U>
	explicit BasicSharedPtr(U* ptr) noexcept
		requires(std::is_base_of_v<T, U>)
	{
		if (ptr)
		{
			const void* rawPtr = (reinterpret_cast<const uint8_t*>(static_cast<T*>(ptr)) - sizeof(ControlBlock));
			acquireBlock<true>(reinterpret_cast<ControlBlock*>(const_cast<void*>(rawPtr)));
		}
	}

	~BasicSharedPtr() noexcept
	{
		m_control.release();
	}

	BasicSharedPtr(const BasicSharedPtr& other) noexcept
	{
		acquireBlock<true>(other.m_control.ctrl);
	}

	template<typename U>
	BasicSharedPtr(const BasicSharedPtr<U, MT>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock<true>(other.m_control.ctrl);
	}

	BasicSharedPtr(BasicSharedPtr&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U>
	BasicSharedPtr(BasicSharedPtr<U, MT>&& other) noexcept
		requires(std::is_base_of_v<T, U>)
	{
		std::swap(m_control, reinterpret_cast<ControlHolder&>(other.m_control));
	}

	BasicSharedPtr& operator=(const BasicSharedPtr& other) noexcept
	{
		BasicSharedPtr(other).swap(*this);
		return *this;
	}

	template<typename U>
	BasicSharedPtr& operator=(const BasicSharedPtr<U, MT>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		BasicSharedPtr(other).swap(*this);
		return *this;
	}

	BasicSharedPtr& operator=(BasicSharedPtr&& other) noexcept
	{
		BasicSharedPtr(std::move(other)).swap(*this);
		return *this;
	}

	template<typename U>
	BasicSharedPtr& operator=(BasicSharedPtr<U, MT>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		BasicSharedPtr(std::move(other)).swap(*this);
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

	/**
	 * This is not safe when multiple threads are involved, since another thread might be holding weak pointers, which can get promoted
	 * to shared pointers at any time.
	 */
	bool unique() const noexcept
	{
		return use_count() == 1;
	}

	void reset() noexcept
	{
		m_control.release();
	}

	/**
	 * Deletes the managed object if this is the last shared pointer owning it.
	 * If there are other shared pointers owning the same object, then this does nothing.
	 *
	 * This can be useful to manually control when the object gets destroyed, which can be important in some cases, e.g, when the
	 * destructor needs to be called on a specific thread. That can be achieved by keeping the object alive with one SharedPtr,
	 * and then calling resetIfLast on the thread you want the destructor to run on, which will cause the destructor to run on
	 * that thread if it's the last SharedPtr, or do nothing if there are other SharedPtrs still alive.
	 *
	 * Not that this is NOT the same as checking `use_count() == 1`. Even if `use_count() == 1` is true, it can still be the case
	 * that there are other threads holding weak pointers, which can promote to shared pointers at any time, so it's not safe to
	 * call `reset()` in that case. This function handles that case correctly.
	 *
	 * @return true if the object was deleted (or the shared pointer was already empty), false if not.
	 */
	bool resetIfLast() noexcept
	{
		return m_control.releaseIfOne();
	}

	void swap(BasicSharedPtr& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	// Explicit access to underlying SharedPtr
	BasicSharedRef<T, MT> toSharedRef() const noexcept
	{
		CZ_CHECK(*this);
		return BasicSharedRef<T, MT>(*this);
	}

	SharedPtrTraces getTraces() const noexcept
	{
#if CZ_SHAREDPTR_STACKTRACES
		return m_control.ctrl ? m_control.ctrl->getTraces() : SharedPtrTraces{};
#else
		return {};
#endif
	}

	// Don't use this directly. It's for internal use only
	static BasicSharedPtr _internal_createFromAlreadyAcquiredBlock(ControlBlock* control) noexcept
	{
		BasicSharedPtr res;
		res.acquireBlock<false>(control);
		return res;
	}

	// Don't use this directly. It's for internal use only
	template<typename U>
	static BasicSharedPtr _internal_stealBlockAndCreate(BasicSharedPtr<U, MT>& from)
	{
		BasicSharedPtr res;
		res.m_control = reinterpret_cast<ControlHolder&&>(std::move(from.m_control));
		from.m_control = {};
		return res;
	}

  private:

	// This is private, so that only BasicWeakPtr::lock and BasicEnableSharedFromThis::sharedFromThis can use it
	#if 0
	BasicSharedPtr(ControlBlock* control) noexcept
	{
		acquireBlock<false>(control);
	}
	#else
	#endif

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

		bool releaseIfOne()
		{
			if (ctrl)
			{
				// This needs to be different from what we do in release(), because we should only destroy the trace IF the release actually happens.
				#if CZ_SHAREDPTR_STACKTRACES
				auto savedTrace = std::move(trace);
				#endif

				if (ctrl->decStrongIfOne())
				{
					ctrl = nullptr;
					return true;
				}

				#if CZ_SHAREDPTR_STACKTRACES
				// The release didn't happen , so we need to put the trace back, since the control block is still alive.
				trace = std::move(savedTrace);
				#endif
			}

			return false;
		}
	} m_control;
};

template<typename T, bool MT, bool IsObserver>
class BasicWeakPtr
{
  public:

	using ControlBlock = details::SharedPtrControlBlock<T, MT>;

	using pointer = T*;
	using element_type = T;

	template<typename U, bool OtherMT, bool OtherIsObserver>
	friend class BasicWeakPtr;

	constexpr BasicWeakPtr() = default;

	BasicWeakPtr(const BasicWeakPtr& other) noexcept
	{
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtr(const BasicWeakPtr<U, MT, OtherIsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U>
	BasicWeakPtr(const BasicSharedPtr<U, MT>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	BasicWeakPtr(BasicWeakPtr&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtr(BasicWeakPtr<U, MT, OtherIsObserver>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		std::swap(m_control, reinterpret_cast<ControlHolder&>(other.m_control));
	}

	~BasicWeakPtr() noexcept
	{
		m_control.release();
	}

	BasicWeakPtr& operator=(const BasicWeakPtr& other) noexcept
	{
		BasicWeakPtr(other).swap(*this);
		return *this;
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtr& operator=(const BasicWeakPtr<U, MT, OtherIsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		BasicWeakPtr(other).swap(*this);
		return *this;
	}

	BasicWeakPtr& operator=(BasicWeakPtr&& other) noexcept
	{
		BasicWeakPtr(std::move(other)).swap(*this);
		return *this;
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtr& operator=(BasicWeakPtr<U, MT, OtherIsObserver>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		BasicWeakPtr(std::move(other)).swap(*this);
		return *this;
	}

	void reset() noexcept
	{
		BasicWeakPtr{}.swap(*this);
	}

	void swap(BasicWeakPtr& other) noexcept
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
	BasicSharedPtr<T, MT> lock() const noexcept
		requires(IsObserver == false)
	{
		if (!m_control.ctrl)
			return {};

		if (m_control.ctrl->lockStrong())
		{
			return BasicSharedPtr<T, MT>::_internal_createFromAlreadyAcquiredBlock(m_control.ctrl);
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
class BasicSharedRef
{
  public:

	using SharedPtrT = BasicSharedPtr<T, MT>;

	template<typename U, bool OtherMT>
	friend class BasicSharedRef;

	/*!
	 * No default construction, since SharedRef must always be non-null.
	 */
	BasicSharedRef() = delete;

	BasicSharedRef(const BasicSharedRef& other)
		: m_ptr(other.m_ptr)
	{
		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again,
		// incase there was a bug somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	BasicSharedRef(const BasicSharedRef<U, MT>& other)
		: m_ptr(other.m_ptr)
	{
		// This is not strictly necessary, since other is a SharedRef and thus non-null, but just to be safe, we check again, incase there was a bug
		// somewhere else
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	explicit BasicSharedRef(const BasicSharedPtr<U, MT>& other) noexcept
		: m_ptr(other)
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	template<typename U>
	explicit BasicSharedRef(BasicSharedPtr<U, MT>&& other) noexcept
		: m_ptr(std::move(other))
	{
		CZ_CHECK(m_ptr.get() != nullptr);
	}

	#if 0
	/*! Assignment from convertible SharedPtr<U> */
	template<typename U>
	BasicSharedRef& operator=(const BasicSharedPtr<U, MT>& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = other;
		return *this;
	}

	/*! Assignment from convertible SharedPtr<U> rvalue */
	template<typename U>
	BasicSharedRef& operator=(BasicSharedPtr<U, MT>&& other) noexcept
	{
		CZ_CHECK(other.get() != nullptr);
		m_ptr = std::move(other);
		return *this;
	}
	#else
	// Don't allow assignment from SharedPtr, so the user needs to be explicit when converting.
	template<typename U>
	BasicSharedRef& operator=(const BasicSharedPtr<U, MT>& other) noexcept = delete;
	template<typename U>
	BasicSharedRef& operator=(BasicSharedPtr<U, MT>&& other) noexcept = delete;
	#endif

	BasicSharedRef& operator=(const BasicSharedRef& other) noexcept = default;

	template<typename U>
	BasicSharedRef& operator=(const BasicSharedRef<U, MT>& other) noexcept
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

	void swap(BasicSharedRef& other) noexcept
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
 * details::allocSharedPtrBlock, so that the control block is set up correctly.
 *
 */
template <typename T, bool MT>
class BasicEnableSharedFromThis
{
  protected:
	constexpr BasicEnableSharedFromThis() noexcept = default;
	BasicEnableSharedFromThis(const BasicEnableSharedFromThis&) noexcept = default;
	BasicEnableSharedFromThis& operator=(const BasicEnableSharedFromThis&) noexcept = default;
	~BasicEnableSharedFromThis() = default;

  public:

	static constexpr bool BasicEnableSharedFromThis_MT = MT;

	/**
	 * Returns a shared pointer to this object
	 *
	 * Since this always returns a shared pointer to the base class, you might prefer to use the `sharedFrom` global function, 
	 * which give a shared pointer of T, it does the necessary cast so it returns a shared pointer of T.
	 * 
	 * NOTE: Assumes the object was allocated via makeShared or any of the other helper functions.
	 */
	BasicSharedPtr<T, MT> sharedFromThis()
	{
		T* derivedThis = static_cast<T*>(this);
		void* rawPtr = reinterpret_cast<uint8_t*>(derivedThis) - sizeof(typename BasicSharedPtr<T, MT>::ControlBlock);
		auto ctrl = reinterpret_cast<typename BasicSharedPtr<T, MT>::ControlBlock*>(rawPtr);

		if (ctrl->lockStrong())
		{
			return BasicSharedPtr<T, MT>::_internal_createFromAlreadyAcquiredBlock(ctrl);
		}
		else
		{
			// I believe we should never get here, since `this` needs to be valid, and if so, then `lockStrong` should always succeed.
			// If this triggers, then two things I can think of:
			//	- Most likely the problem is that this was called from the object's constructor, before the control block was fully setup.
			//	- There is some race condition where the object is being destroyed in another thread right when sharedFromThis is
			//	  called.
			CZ_CHECK(false);
			return {};
		}
	}

	/**
	 * Const version of sharedFromThis.
	 */
	BasicSharedPtr<const T, MT> sharedFromThis() const
	{
		const T* derivedThis = static_cast<const T*>(this);
		void* rawPtr = reinterpret_cast<uint8_t*>(const_cast<T*>(derivedThis)) - sizeof(typename BasicSharedPtr<T, MT>::ControlBlock);
		auto ctrl = reinterpret_cast<typename BasicSharedPtr<T, MT>::ControlBlock*>(rawPtr);

		if (ctrl->lockStrong())
		{
			return BasicSharedPtr<const T, MT>::createFromAlreadyAcquiredBlock(ctrl);
		}
		else
		{
			CZ_CHECK(false);
			return {};
		}
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	BasicWeakPtr<T, MT, false> weakFromThis()
	{
		return BasicWeakPtr<T, MT, false>(sharedFromThis());
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	BasicWeakPtr<const T, MT, false> weakFromThis() const
	{
		return BasicWeakPtr<const T, MT, false>(sharedFromThis());
	}
};

} // namespace cz::details



//
// Utility functions
//

namespace cz
{

	namespace details
	{

		/**
		 * Constructs a shared pointer with the specified parameters.
		 */
		template <typename T, bool MT, typename... Args>
		BasicSharedPtr<T, MT> basicMakeShared(Args&& ... args)
		{
			static_assert(!std::is_abstract_v<T>, "Type is abstract.");
			void* ptr = details::allocSharedPtrBlock<T, MT>();
			return BasicSharedPtr<T, MT>(new(ptr) T(std::forward<Args>(args)...));
		}

		template<typename T, bool MT, typename... Args>
		BasicSharedRef<T, MT> basicMakeSharedRef(Args&&... args)
		{
			return BasicSharedRef<T, MT>(basicMakeShared<T, MT>(std::forward<Args>(args)...));
		}

	}

	template<class T, bool MT, class U>
	details::BasicSharedPtr<T, MT> static_pointer_cast(const details::BasicSharedPtr<U, MT>& other) noexcept
		requires(std::is_base_of_v<U, T>)
	{
		return details::BasicSharedPtr<T, MT>(static_cast<T*>(other.get()));
	}

	template<class T, bool MT, class U>
	details::BasicSharedPtr<T, MT> static_pointer_cast(details::BasicSharedPtr<U, MT>&& other) noexcept
		requires(std::is_base_of_v<U, T>)
	{
		// This seems a bit convoluted, but avoid creating another stack trace if stack traces are enabled, and stack traces are expensive
		return details::BasicSharedPtr<T, MT>::_internal_stealBlockAndCreate(other);
	}

	template <class T, class U, bool MT>
	bool operator==(const details::BasicSharedPtr<T, MT>& left, const details::BasicSharedPtr<U, MT>& right) noexcept
	{
		return left.get() == right.get();
	}

	template <class T, bool MT>
	bool operator==(const details::BasicSharedPtr<T, MT>& left, std::nullptr_t) noexcept
	{
		return left.get() == nullptr;
	}

	template <class T, bool MT>
	bool operator==(std::nullptr_t, const details::BasicSharedPtr<T, MT>& right) noexcept
	{
		return nullptr == right.get();
	}

	//The <, <=, >, >=, and != operators are synthesized from operator<=> and operator== respectively.
	template<typename T1, typename T2, bool MT>
	std::strong_ordering operator<=>(const details::BasicSharedPtr<T1, MT>& left, const details::BasicSharedPtr<T2, MT>& right) noexcept
	{
		return left.get() <=> right.get();
	}

	//
	// BasicSharedRef utilities
	//

	// Casting 

	template<class T, class U, bool MT>
	details::BasicSharedRef<T, MT> static_pointer_cast(const details::BasicSharedRef<U, MT>& other) noexcept
		requires(std::is_base_of_v<U, T>)
	{
		// other is guaranteed non-null, so resulting SharedPtr<T> is also non-null
		details::BasicSharedPtr<T, MT> casted = static_pointer_cast<T>(other.toSharedPtr());
		CZ_CHECK(casted.get() != nullptr);
		return details::BasicSharedRef<T, MT>(std::move(casted));
	}

	// Comparisons

	template <class T, class U, bool MT>
	bool operator==(const details::BasicSharedRef<T, MT>& left, const details::BasicSharedRef<U, MT>& right) noexcept
	{
		return left.get() == right.get();
	}

	template<typename T1, typename T2, bool MT>
	std::strong_ordering operator<=>(const details::BasicSharedRef<T1, MT>& left, const details::BasicSharedRef<T2, MT>& right) noexcept
	{
		return left.get() <=> right.get();
	}


	//////////////////////////////////////////////////////////////////////////
	//
	// Utilities to make it easier to use EnableSharedFromThis with class hierarchies by doing the required casts.
	//
	//////////////////////////////////////////////////////////////////////////

	template<typename T>
	details::BasicSharedPtr<T, T::BasicEnableSharedFromThis_MT> sharedFrom(T* obj)
	{
		CZ_CHECK(obj != nullptr);
		return static_pointer_cast<T, T::BasicEnableSharedFromThis_MT>(obj->sharedFromThis());
	}

	template<typename T>
	details::BasicSharedRef<T, T::BasicEnableSharedFromThis_MT> sharedRefFrom(T* obj)
	{
		return sharedFrom(obj).toSharedRef();
	}

	/**
	 * Given a raw pointer to an object that inherits from EnableSharedFromThis, returns a WeakPtr<T> to that object.
	 * This is sometimes easier than calling obj->weakFromThis(), since that will return a WeakPtr of the base class specified in
	 * the EnableSharedFromThis.
	 *
	 * By using this utility function, the returned WeakPtr will be of the type T already.
	 * 
	 * A typical use case is when passing a WeakPtr to a lambda, which we then want to lock to do some changes.
	 * By using this, we avoid having to do any casts in the lambda.
	 */
	template<typename T>
	details::BasicWeakPtr<T, T::BasicEnableSharedFromThis_MT, false> weakFrom(T* obj)
	{
		CZ_CHECK(obj != nullptr);
		return details::BasicWeakPtr<T, T::BasicEnableSharedFromThis_MT, false>(sharedFrom<T>(obj));
	}


} // namespace cz

