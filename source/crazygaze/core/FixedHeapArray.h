#pragma once

#include "Common.h"
#include "TaggedPtr.h"

namespace cz
{

namespace details
{

	template <typename T, bool UseTaggedPointer>
	struct FixedHeapArrayStorage
	{
	};

	template <typename T>
	struct FixedHeapArrayStorage<T, true>
	{
		TaggedPtr<T> c;

		FixedHeapArrayStorage() = default;
		FixedHeapArrayStorage(T* ptr, size_t count)
		{
			c.setPtr(ptr);
			c.setTag(static_cast<uint32_t>(count));
		}

		inline const T* data() const
		{
			return c.getPtr();
		}

		inline T* data()
		{
			return c.getPtr();
		}

		inline size_t size() const
		{
			return c.getTag();
		}
	};

	template <typename T>
	struct FixedHeapArrayStorage<T, false>
	{
		T* ptr = nullptr;
		size_t count = 0;

		FixedHeapArrayStorage() = default;
		FixedHeapArrayStorage(T* ptr, size_t count)
			: ptr(ptr)
			, count(count)
		{
		}

		inline const T* data() const
		{
			return ptr;
		}

		inline T* data()
		{
			return ptr;
		}

		inline size_t size() const
		{
			return count;
		}
	};
}


/**
 * A fixed size array allocated on the heap.
 *
 * The purpose of this container is to have the smallest overhead possible.
 * E.g, on a 64-bits architecture, sizeof(std::vector<T>) typically is 24 bytes (3*8).
 * This container can be 16 or 8 bytes, depending if pointer tagging is used.
 *
 * - The size is set at construction time
 * - No methods to insert/remove elements, since the size is fixed.
 * - For scenarios where 
 * - If using pointer tagging, then sizeof(*this) is only 8 bytes, at the expense of having a limited maximum size.
 *		- The maximum size can be queried with the ::max_size constexpr variable, or the maxSize() method.
 *
 * WARNING: When setting UseTaggedPointer to true, check if max_size is large enough for your needs.
 * 
 */
template <typename T, bool UseTaggedPointer>
class FixedHeapArray
{
  public:
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = T*;
	using const_iterator = const T*;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// This allows the user code to query what's the maximum size when using tagged pointers
	static constexpr size_t max_size = UseTaggedPointer ? TaggedPtr<T>::MaxTagValue : std::numeric_limits<size_t>::max();

	// malloc guarantees an alignment suitable for max_align_t (see https://en.cppreference.com/w/c/types/max_align_t.html)
	// This class uses malloc/free, therefore we can't support alignments higher than max_align_t's alignment
	static_assert(alignof(T) <= alignof(max_align_t), "No alignment higher than max_align_t's alignment supported");

  private:

	details::FixedHeapArrayStorage<T, UseTaggedPointer> m_c;

  public:
	FixedHeapArray() noexcept = default;

	explicit FixedHeapArray(size_type count)
		: m_c(reinterpret_cast<T*>(malloc(count * sizeof(T))), count)
	{
		std::uninitialized_default_construct_n(m_c.data(), m_c.size());
	}

	explicit FixedHeapArray(size_type count, const T& value)
		: m_c(reinterpret_cast<T*>(malloc(count * sizeof(T))), count)
	{
		std::uninitialized_fill_n(m_c.data(), m_c.size(), value);
	}

	FixedHeapArray(const T* first, const T* last)
		: m_c(reinterpret_cast<T*>(malloc((last-first)*sizeof(T))), static_cast<size_t>(last-first))
	{
		std::uninitialized_copy_n(first, last - first, m_c.data());
	}

	FixedHeapArray(std::initializer_list<T> init)
		: FixedHeapArray(init.begin(), init.end())
	{
	}

	explicit FixedHeapArray(const FixedHeapArray& other)
		: m_c(reinterpret_cast<T*>(malloc(other.size() * sizeof(T))), other.size())
	{
		std::uninitialized_copy_n(other.data(), other.size(), m_c.data());
	}

	explicit FixedHeapArray(FixedHeapArray&& other)
	{
		std::swap(m_c, other.m_c);
	}

	~FixedHeapArray()
	{
		destroy();
	}


	FixedHeapArray& operator=(const FixedHeapArray& other)
	{
		if (this == &other)
			return *this;

		FixedHeapArray tmp(other);
		swap(tmp, *this);
		return *this;
	}

	FixedHeapArray& operator=(FixedHeapArray&& other) noexcept
	{
		if (this == &other)
			return *this;

		destroy();
		std::swap(m_c, other.m_c);
		return *this;
	}

	reference operator[](size_type index) noexcept
	{
		CZ_CHECK(index < size());
		return data()[index];
	}

	const_reference operator[](size_type index) const noexcept
	{
		CZ_CHECK(index < size());
		return data()[index];
	}

	[[nodiscard]] reference at(size_type index)
	{
		if (index >= size())
			throw std::out_of_range("FixedHeapArray::at");
		return data()[index];
	}

	[[nodiscard]] const_reference at(size_type index) const
	{
		if (index >= size())
			throw std::out_of_range("FixedHeapArray::at");
		return data()[index];
	}

	[[nodiscard]] reference front() noexcept
	{
		CZ_CHECK(size() > 0);
		return data()[0];
	}

	[[nodiscard]] const_reference front() const noexcept
	{
		CZ_CHECK(size() > 0);
		return data()[0];
	}

	[[nodiscard]] reference back() noexcept
	{
		CZ_CHECK(size() > 0);
		return data()[size() - 1];
	}

	[[nodiscard]] const_reference back() const noexcept
	{
		CZ_CHECK(size() > 0);
		return data()[size() - 1];
	}

	[[nodiscard]] iterator begin() noexcept
	{
		return data();
	}

	[[nodiscard]] const_iterator begin() const noexcept
	{
		return data();
	}

	[[nodiscard]] const_iterator cbegin() const noexcept
	{
		return data();
	}

	[[nodiscard]] iterator end() noexcept
	{
		return data() + size();
	}

	[[nodiscard]] const_iterator end() const noexcept
	{
		return data() + size();
	}

	[[nodiscard]] const_iterator cend() const noexcept
	{
		return data() + size();
	}

	[[nodiscard]] reverse_iterator rbegin() noexcept
	{
		return reverse_iterator(end());
	}

	[[nodiscard]] const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator(end());
	}

	[[nodiscard]] const_reverse_iterator crbegin() const noexcept
	{
		return const_reverse_iterator(cend());
	}

	[[nodiscard]] reverse_iterator rend() noexcept
	{
		return reverse_iterator(begin());
	}

	[[nodiscard]] const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator(begin());
	}

	[[nodiscard]] const_reverse_iterator crend() const noexcept
	{
		return const_reverse_iterator(cbegin());
	}

	T* data()
	{
		return m_c.data();
	}

	const T* data() const
	{
		return m_c.data();
	}

	size_t size() const
	{
		return m_c.size();
	}

	bool empty() const noexcept
	{
		return size() == 0;
	}

	friend void swap(FixedHeapArray& a, FixedHeapArray& b) noexcept
	{
		std::swap(a.m_c, b.m_c);
	}

	constexpr size_t maxSize() const
	{
		return max_size;
	}

  private:
	void destroy()
	{
		std::destroy_n(m_c.data(), m_c.size());
		free(m_c.data());
		m_c = {};
	}
};

}

