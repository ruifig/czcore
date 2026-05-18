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
 *		- Setting CZ_SHAREDPTR_STACKTRACES to 1 it compiles in stack trace support, but enabling it for a specific class is opt-in.
 *		  You can enable it for a specific class by adding a static member in your class: `static constexpr bool enable_sharedptr_stacktraces = true;`
 *	- The control block is ALWAYS allocated together with the object, therefore it doesn't allow using SharedPtr with objects that were allocated in a different way.
 *		- Allocation should be done with the "makeShared" helper functions.
 */
template<typename T, bool MT>
class BasicSharedPtr
{
  public:

	template<typename U, bool OtherMT, bool IsObserver>
	friend class BasicWeakPtrImpl;

	template<typename U, bool OtherMT>
	friend class BasicSharedPtr;

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
	{
		if (ptr)
		{
			static_assert(std::is_convertible_v<U*, T*>);
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
	{
		static_assert(std::is_convertible_v<U*, T*>);
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

	bool unique() const noexcept
	{
		return use_count() == 1;
	}

	void reset() noexcept
	{
		m_control.release();
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


  private:

	// This is private, since only BasicWeakPtr::lock can use it
	BasicSharedPtr(ControlBlock* control) noexcept
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
class BasicWeakPtrImpl
{
  public:

	using ControlBlock = details::SharedPtrControlBlock<T, MT>;

	using pointer = T*;
	using element_type = T;

	template<typename U, bool OtherMT, bool OtherIsObserver>
	friend class BasicWeakPtrImpl;

	constexpr BasicWeakPtrImpl() = default;

	BasicWeakPtrImpl(const BasicWeakPtrImpl& other) noexcept
	{
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtrImpl(const BasicWeakPtrImpl<U, MT, OtherIsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	template<typename U>
	BasicWeakPtrImpl(const BasicSharedPtr<U, MT>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control.ctrl);
	}

	BasicWeakPtrImpl(BasicWeakPtrImpl&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtrImpl(BasicWeakPtrImpl<U, MT, OtherIsObserver>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		std::swap(m_control, reinterpret_cast<ControlHolder&>(other.m_control));
	}

	~BasicWeakPtrImpl() noexcept
	{
		m_control.release();
	}

	BasicWeakPtrImpl& operator=(const BasicWeakPtrImpl& other) noexcept
	{
		BasicWeakPtrImpl(other).swap(*this);
		return *this;
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtrImpl& operator=(const BasicWeakPtrImpl<U, MT, OtherIsObserver>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		BasicWeakPtrImpl(other).swap(*this);
		return *this;
	}

	BasicWeakPtrImpl& operator=(BasicWeakPtrImpl&& other) noexcept
	{
		BasicWeakPtrImpl(std::move(other)).swap(*this);
		return *this;
	}

	template<typename U, bool OtherIsObserver>
	BasicWeakPtrImpl& operator=(BasicWeakPtrImpl<U, MT, OtherIsObserver>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		BasicWeakPtrImpl(std::move(other)).swap(*this);
		return *this;
	}

	void reset() noexcept
	{
		BasicWeakPtrImpl{}.swap(*this);
	}

	void swap(BasicWeakPtrImpl& other) noexcept
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
			return BasicSharedPtr<T, MT>(m_control.ctrl);
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
	 * NOTE: Assumes the object was allocated via makeShared or any of the other helper functions.
	 */
	BasicSharedPtr<T, MT> sharedFromThis()
	{
		T* derivedThis = static_cast<T*>(this);
		return BasicSharedPtr<T, MT>(derivedThis);
	}

	/**
	 * Returns a shared pointer to this object.
	 * NOTE: Assumes the object was allocated via makeShared or any of the other helper functions.
	 */
	BasicSharedPtr<const T, MT> sharedFromThis() const
	{
		const T* derivedThis = static_cast<const T*>(this);
		// Need to cast away const because SharedPtr constructor expects non-const
		return BasicSharedPtr<const T, MT>(const_cast<T*>(derivedThis));
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	BasicWeakPtrImpl<T, MT, false> weakFromThis()
	{
		return BasicWeakPtrImpl<T, MT, false>(sharedFromThis());
	}

	/**
	 * Returns a WeakPtr<T> to this object.
	 *
	 * Useful when you want to check if the object is still alive in async scenarios.
	 */
	BasicWeakPtrImpl<const T, MT, false> weakFromThis() const
	{
		return BasicWeakPtrImpl<const T, MT, false>(sharedFromThis());
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
	{
		return details::BasicSharedPtr<T, MT>(static_cast<T*>(other.get()));
	}

	template<class T, bool MT, class U>
	details::BasicSharedPtr<T, MT> static_pointer_cast(details::BasicSharedPtr<U, MT>&& other) noexcept
	{
		details::BasicSharedPtr<U, MT> other_(std::move(other));
		return details::BasicSharedPtr<T, MT>(static_cast<T*>(other_.get()));
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
	{
		// other is guaranteed non-null, so resulting SharedPtr<T> is also non-null
		details::BasicSharedPtr<T, MT> casted = static_pointer_cast<T, U, MT>(other.toSharedPtr());
		CZ_CHECK(casted.get() != nullptr);
		return details::BasicSharedRef<T, MT>(casted);
	}

	// #TODO : Do I need this? Probably not, since SharedRef doesn't have move semantics.
	template<class T, class U, bool MT>
	details::BasicSharedRef<T, MT> static_pointer_cast(details::BasicSharedRef<U, MT>&& other) noexcept
	{
		details::BasicSharedPtr<U, MT> tmp = other.toSharedPtr(); // copy, keep it simple. We can't actually move, because SharedRef doesn't have move semantics.
		details::BasicSharedPtr<T, MT> casted = static_pointer_cast<T, U, MT>(tmp);
		CZ_CHECK(casted.get() != nullptr);
		return details::BasicSharedRef<T, MT>(casted);
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


	//
	// Utilities to make it easier to use EnableSharedFromThis with class hierarchies by doing the required cast.
	//

	template <typename T, typename U>
	details::BasicSharedPtr<T, U::BasicEnableSharedFromThis_MT> toStrong(U* obj)
	{
		return static_pointer_cast<T, T::BasicEnableSharedFromThis_MT>(obj->sharedFromThis());
	}

	template <typename T>
	details::BasicSharedPtr<const T, T::BasicEnableSharedFromThis_MT> toStrong(const T* obj)
	{
		return static_pointer_cast<const T, T::BasicEnableSharedFromThis_MT>(obj->sharedFromThis());
	}

	template <typename T>
	details::BasicSharedPtr<T, T::BasicEnableSharedFromThis_MT> toStrong(T& obj)
	{
		return static_pointer_cast<T, T::BasicEnableSharedFromThis_MT>(obj.sharedFromThis());
	}

	template <typename T>
	details::BasicSharedPtr<const T, T::BasicEnableSharedFromThis_MT> toStrong(const T& obj)
	{
		return static_pointer_cast<const T, T::BasicEnableSharedFromThis_MT>(obj.sharedFromThis());
	}


} // namespace cz

