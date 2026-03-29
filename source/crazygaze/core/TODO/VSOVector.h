#pragma once

#include "Common.h"
#include "Math.h"

namespace cz
{

/**
 *
 * EXPERIMENTAL
 *
 * (V)ariable (S)sized (Ob)jects vector
 * Vector for objects that can have variable sizes.
 *
 * The idea behind this container is to make it easier to have variable sized objects laid out sequential in memory, to make better use
 * of CPU cache.
 *
 * Objects need to be copiable with just memcpy, so makes the following assumptions:
 *	- A base class needs to be specified
 *	- Objects need to be trivially destructible (std::is_trivially_destructible<T> == true)
 *		- This is because the destructors are NOT called.
 *	- Objects can have a vtable.
 *	- Objects should not have assignent or copy operators, since only memcpy is used internally when objects need to be copied.
 *  - Derived classes should not need an higher alignment than the base class
 *  - Once added, objects can't be deleted
 *  - User code holds the provided Ref objects, and not actual pointers to the objects.
 */
template<typename BaseT>
class VSOVector
{
  public:

	// At the moment, it supports objects with vtables, but not destructors at all.
	static_assert(std::is_trivially_destructible_v<BaseT> == true);

	using SizeType = uint32_t;

	struct Ref
	{
		Ref() = default;
		explicit Ref(SizeType pos)
			: pos(pos)
		{
		}

		inline static constexpr SizeType InvalidValue = std::numeric_limits<SizeType>::max();

		/**
		 * Returns true if the iterator is set (even if pointing to end())
		 * This is only useful when the user code needs to check if a reference was set to point to something or not. 
		 */
		bool isSet() const noexcept
		{
			return pos != InvalidValue;
		}

		SizeType pos = InvalidValue;
		friend auto operator<=>(Ref lhs, Ref rhs) = default;
	};

	template<typename T>
	struct ObjWrapper
	{
		ObjWrapper() = default;
		ObjWrapper(const ObjWrapper&) = delete;

		// Size of the block used by this object wrapper
		// This is NOT the size of the object itself, but how many bytes we need to increment to get
		// to the next object.
		// This is because the container allows inserting raw data that are not objects.
		SizeType size;

		// Make sure T is inherits from BaseT
		static_assert(std::is_base_of_v<BaseT, T>);

		// At the moment we support vtable, but not destructors at all, since the container doesn't call the destructors.
		static_assert(std::is_trivially_destructible_v<T> == true);

		T obj;

		// We need this to use memcpy, so vtable is copied if BaseT has a vtable
		ObjWrapper& operator = (const ObjWrapper& other)
		{
			#if CZ_LINUX
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wdynamic-class-memaccess"
			#endif

			memcpy(this, &other, sizeof(*this));

			#if CZ_LINUX
				#pragma GCC diagnostic pop
			#endif

			return *this;
		}
	};

	class Iterator
	{
		public:

		BaseT& get() const
		{
			// When casting, we need to account for the fact there is a header, so we cast to ObjWrapper<BaseT> and get the obj field
			return (reinterpret_cast<ObjWrapper<BaseT>*>(ptr))->obj;
		}

		template<typename T>
		T& as()
		{
			static_assert(std::is_base_of_v<BaseT, T>);
			static_assert(alignof(BaseT) >= alignof(T));
			return *static_cast<T*>(&get());
		}

		using reference = BaseT&;
		using pointer = BaseT*;

		reference operator*() const
		{
			return get();
		}

		pointer operator->() const
		{
			return &get();
		}

		Iterator& operator++()
		{
			SizeType size = *reinterpret_cast<SizeType*>(ptr);
			ptr += size;
			return *this;
		}

		friend auto operator<=>(const Iterator& lhs, const Iterator& rhs) = default;

		private:

		friend VSOVector;
		explicit Iterator(uint8_t* ptr)
			: ptr(ptr)
		{
		}

		uint8_t* ptr = nullptr;
	};

	VSOVector() = default;

	~VSOVector()
	{
		if (m_data)
		{
			free(m_data);
		}
	}

	VSOVector(SizeType capacity)
	{
		m_data = static_cast<uint8_t*>(malloc(capacity));
		m_capacity = capacity;
	}

	VSOVector& operator=(const VSOVector& other) = delete;
	VSOVector& operator=(VSOVector&& other) = delete;
	VSOVector(VSOVector&& other) = delete;
	VSOVector(const VSOVector& other) = delete;

	static constexpr SizeType getHeaderSize()
	{
		return offsetof(ObjWrapper<BaseT>, obj);
	}

	template<typename Deleter>
	void clear(Deleter&& deleter)
	{
		if (m_data)
		{
			// Try with range loop
			for(BaseT& o : *this)
			{
				deleter(o);
			}

			m_usedCapacity = 0;
			m_numElements = 0;
			m_first = {};
			m_last = {};
		}
	}

	void clear()
	{
		m_usedCapacity = 0;
		m_numElements = 0;
		m_first = {};
		m_last = {};
	}

	/**
	 * Adds an element to the end of the vector
	 * @param obj Element to add
	 * @param extraBytes extra bytes to add at the end. This allows the caller to add space after the object for data the object
	 * might want to add (e.g: a string that whose size is not known at compile time);
	 */
	template<typename T>
	Ref push_back(const T& obj, SizeType extraBytes = 0)
	{
		static_assert(alignof(BaseT) >= alignof(T));

		ObjWrapper<T> tmp;
		tmp.size = sizeof(tmp);
		if (extraBytes)
		{
			tmp.size += extraBytes;
			auto remainder = tmp.size % alignof(ObjWrapper<T>);
			tmp.size += remainder ? SizeType(alignof(ObjWrapper<T>) - remainder) : 0;
		}

		tmp.obj = obj;
		if (getFreeCapacity() < tmp.size)
		{
			grow(tmp.size);
		}

		Ref res(m_usedCapacity);
		m_usedCapacity += tmp.size;
		internalAt<T>(res) = tmp;
		++m_numElements;

		if (!m_first.isSet())
		{
			m_first = res;
		}

		m_last = res;
		return res;
	}

	template<typename T, typename... Args>
	Ref emplace_back(Args&& ... args)
	{
		T obj(std::forward<Args>(args)...);
		return push_back(obj, 0);
	}

	/*!
	 * Creates the space for OOB data
	 */
	template<typename T>
	Ref oob_push_back_empty(const T* data, SizeType count)
	{
		static_assert(std::is_standard_layout_v<T>);
		static_assert(std::is_trivially_destructible_v<T>);

		SizeType alignedNeededCapacity = static_cast<SizeType>(roundUpToMultipleOf(count * sizeof(T), sizeof(SizeType)));
		if (getFreeCapacity() < alignedNeededCapacity)
		{
			grow(alignedNeededCapacity);
		}

		Ref res(m_usedCapacity);
		m_usedCapacity += alignedNeededCapacity;
		if (m_last.isSet())
		{
			internalAt<BaseT>(m_last).size += alignedNeededCapacity;
		}

		return res;
	}

	/*!
	 * Pushes an OOB raw data.
	 * OOB means "out of band", since it's data that the iterators don't see.
	 * The purpose of this kind of data is for when you need to insert raw data into the vector that other objects need to use.
	 */
	template<typename T>
	Ref oob_push_back(const T* data, SizeType count)
	{
		Ref res = oob_push_back_empty(data, count);
		uint8_t* ptr = m_data + res.pos;
		memcpy(ptr, data, count * sizeof(T));
		return res;
	}

	SizeType getNumElements() const
	{
		return m_numElements;
	}

	Iterator begin()
	{
		return m_first.isSet() ? refToIterator(m_first) : end();
	}

	Iterator end()
	{
		return Iterator(m_data + m_usedCapacity);
	}

	BaseT& at(Ref ref)
	{
		return internalAt<BaseT>(ref).obj;
	}

	Ref iteratorToRef(Iterator it)
	{
		return Ref(SizeType(it.ptr - m_data));
	}

	Iterator refToIterator(Ref ref)
	{
		assert(ref.isSet());
		return Iterator(m_data + ref.pos);
	}

	template<typename T>
	T& atAs(Ref ref)
	{
		static_assert(std::is_base_of_v<BaseT, T>);
		static_assert(alignof(BaseT) >= alignof(T));
		return *static_cast<T*>(&internalAt<BaseT>(ref).obj);
	}


	uint8_t* oobAt(Ref ref) const
	{
		assert(ref.pos < m_usedCapacity);
		return m_data + ref.pos; 
	}

	template<typename T>
	T& oobAtAs(Ref ref) const
	{
		static_assert(std::is_standard_layout_v<T>);
		static_assert(std::is_trivially_destructible_v<T>);
		return *reinterpret_cast<T*>(oobAt(ref));
	}

	Ref next(Ref ref)
	{
		return Ref(ref.pos + internalAt<BaseT>(ref).size);
	}

	Ref beginRef()
	{
		return Ref(0);
	}

	Ref endRef()
	{
		return Ref(m_usedCapacity);
	}

	SizeType getCapacity() const
	{
		return m_capacity;
	}

	SizeType getUsedCapacity() const
	{
		return m_usedCapacity;
	}

	SizeType getFreeCapacity() const
	{
		return m_capacity - m_usedCapacity;
	}

  protected:

	template<typename T>
	ObjWrapper<T>& internalAt(Ref ref)
	{
		static_assert(std::is_base_of_v<BaseT, T>);
		static_assert(alignof(BaseT) >= alignof(T));

		assert(ref.pos < m_usedCapacity);
		return *reinterpret_cast<ObjWrapper<T>*>(m_data + ref.pos);
	}

	// While std::vector::capacity tells us how many elements we can store, capacity here means the total memory we can store.
	// Because objects have variable size, we can't know how many objects we'll store
	SizeType m_capacity = 0;
	SizeType m_usedCapacity = 0;
	uint8_t* m_data = nullptr;
	SizeType m_numElements = 0;

	// Reference to the first obj.
	// This is needed, in case the user inserted OOB dat first
	Ref m_first;
	Ref m_last;

	void grow(SizeType requiredFreeCapacity)
	{
		SizeType newCapacity = static_cast<SizeType>(round_pow2(m_usedCapacity + requiredFreeCapacity));

		// Allocate new block
		uint8_t* newData = reinterpret_cast<uint8_t*>(malloc(newCapacity));

		// Copy current block to new one and adjust the header information
		if (m_data)
		{
			memcpy(newData, m_data, m_usedCapacity);
			free(m_data);
		}

		m_data = newData;
		m_capacity = newCapacity;
	}

};

} // namespace cz

