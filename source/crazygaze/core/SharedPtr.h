#pragma once

#include "Common.h"
#include "Logging.h"


/*
This controls if memory should be cleared when the last strong reference is gone and the object destroyed.
Since the way SharedPtr is implemented is that it allocates memory for Control Block + Object in one go, it means the object's memory
will only be deallocated once the Control Block is also gone.

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
// Keep a WeakPtr means once the MyFoo object is destroyed, the memory is not actually deallocated
WeakPtr<MyFoo> wfoo = foo;
// Passing a raw pointer to Logger
Logger logger(foo.get());
// This destroys MyFoo, but doesn't deallocated the memory, because of the WeakPtr.
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


namespace cz
{

template<typename T>
inline void sharedPtrDeleter(T* obj)
{
	obj->~T();
}

namespace details
{

	class BaseSharedPtrControlBlock
	{
	  public:

#if CZ_SHAREDPTR_CLEAR_MEM
		BaseSharedPtrControlBlock(size_t size) : size(size) {}
#else
		BaseSharedPtrControlBlock(size_t) {}
#endif

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
			return control + 1;
		}

	  protected:
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

	template<typename U, typename UDeleter>
	friend class WeakPtr;

	template<typename U, typename UDeleter>
	friend class SharedPtr;

	using ControlBlock = details::SharedPtrControlBlock<T, Deleter>;	

	SharedPtr() noexcept {}

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

	~SharedPtr()
	{
		releaseBlock();
	}

	SharedPtr(const SharedPtr& other) noexcept
	{
		acquireBlock(other.m_control);
	}

	template<typename U>
	SharedPtr(const SharedPtr<U, Deleter>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control);
	}

	SharedPtr(SharedPtr&& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	template<typename U>
	SharedPtr(SharedPtr<U>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		m_control = reinterpret_cast<ControlBlock*>(other.m_control);
		other.m_control = nullptr;
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
		CZ_CHECK(m_control);
		return m_control->toObject<T>();
	}

	T* get() const noexcept
	{
		if(m_control)
		{
			return m_control->template toObject<T>();
		}
		else
		{
			return nullptr;
		}
	}

	T& operator*() const noexcept
	{
		CZ_CHECK(m_control);
		return *m_control->toObject<T>();
	}

	explicit operator bool() const noexcept
	{
		return m_control ? true : false;
	}

	unsigned int use_count() const noexcept
	{
		return m_control ? m_control->strongRefs() : 0;
	}

	bool unique() const noexcept
	{
		return use_count() == 1;
	}

	void reset() noexcept
	{
		releaseBlock();
	}

	void swap(SharedPtr& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

  private:

	void releaseBlock() noexcept
	{
		if (m_control)
		{
			m_control->decStrong();
			m_control = nullptr;
		}
	}

	template<typename U>
	void acquireBlock(details::SharedPtrControlBlock<U, Deleter>* control) noexcept
	{
		static_assert(std::is_convertible_v<U*,T*>);
		m_control = reinterpret_cast<ControlBlock*>(control);
		if (m_control)
		{
			m_control->incStrong();
		}
	}

	ControlBlock* m_control = nullptr;
};

template<typename T, typename Deleter = details::SharedPtrDefaultDeleter>
class WeakPtr
{
  public:

	using ControlBlock = details::SharedPtrControlBlock<T>;

	template<typename U, typename UDeleter>
	friend class WeakPtr;

	constexpr WeakPtr() = default;

	WeakPtr(const WeakPtr& other)
	{
		acquireBlock(other.m_control);
	}

	template<typename U>
	WeakPtr(const WeakPtr<U, Deleter>& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control);
	}

	template<typename U>
	WeakPtr(const SharedPtr<U, Deleter>& other)
	{
		static_assert(std::is_convertible_v<U*, T*>);
		acquireBlock(other.m_control);
	}

	WeakPtr(WeakPtr&& other) noexcept
	{
		m_control = other.m_control;
		other.m_control = nullptr;
	}

	~WeakPtr()
	{
		releaseBlock();
	}

	template<typename U>
	WeakPtr(WeakPtr<U, Deleter>&& other) noexcept
	{
		static_assert(std::is_convertible_v<U*, T*>);
		m_control = reinterpret_cast<ControlBlock*>(other.m_control);
		other.m_control = nullptr;
	}

	WeakPtr& operator=(const WeakPtr& other)
	{
		WeakPtr(other).swap(*this);
		return *this;
	}

	template<typename U>
	WeakPtr& operator=(const WeakPtr<U, Deleter>& other)
	{
		static_assert(std::is_convertible_v<U*, T*>);
		WeakPtr(other).swap(*this);
		return *this;
	}

	template<typename U>
	WeakPtr& operator=(const SharedPtr<U, Deleter>& other)
	{
		static_assert(std::is_convertible_v<U*, T*>);
		WeakPtr(other).swap(*this);
		return *this;
	}

	template<typename U>
	WeakPtr& operator=(WeakPtr<U, Deleter>&& other)
	{
		static_assert(std::is_convertible_v<U*, T*>);
		WeakPtr(std::move(other)).swap(*this);
		return *this;
	}

	void reset() noexcept
	{
		WeakPtr{}.swap(*this);
	}

	void swap(WeakPtr& other) noexcept
	{
		std::swap(m_control, other.m_control);
	}

	unsigned int use_count() const
	{
		return (m_control) ? m_control->strongRefs() : 0;
	}

	bool expired() const
	{
		return use_count() == 0 ? true : false;
	}

	SharedPtr<T, Deleter> lock() const
	{
		if (use_count())
		{
			return SharedPtr<T, Deleter>(m_control->obj());
		}
		else
		{
			return SharedPtr<T, Deleter>();
		}
	}

  private:

	void releaseBlock()
	{
		if (m_control)
		{
			m_control->decWeak();
			m_control = nullptr;
		}
	}

	template<typename U>
	void acquireBlock(details::SharedPtrControlBlock<U, Deleter>* control)
	{
		static_assert(std::is_convertible_v<U*,T*>);
		m_control = reinterpret_cast<ControlBlock*>(control);
		if (m_control)
		{
			m_control->incWeak();
		}
	}

	ControlBlock* m_control = nullptr;
};


template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&& ... args)
{
	static_assert(!std::is_abstract_v<T>, "Type is abstract.");
	void* ptr = details::BaseSharedPtrControlBlock::allocBlock<T>();
	return SharedPtr<T>(new(ptr) T(std::forward<Args>(args)...));
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


} // namespace cz

