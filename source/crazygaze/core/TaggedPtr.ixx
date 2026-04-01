module;

#include "Logging_Macros.h"

//////////////////////////////////////////////////////////////////////////
export module czcore:taggedptr;

import std;
import :logging;

export namespace cz
{

// Only supported on x64 for now
#if defined(__x86_64__) || defined(_M_X64)

/**
 * See https://en.wikipedia.org/wiki/Tagged_pointer 
 *
 * It allows packing user data into a pointer's unused bits.
 *
 */
template<typename T, uint32_t MinAlign = 1>
class TaggedPtr
{
	static_assert(sizeof(void*) == 8, "TaggedPtr only supports 64 bits platforms");
	using pointer = T*;

	static consteval int calcLowTagBits()
	{
		if constexpr (std::is_same_v<void, T>)
			return std::countr_zero(static_cast<uint64_t>(std::max(1u, MinAlign)));
		else
			return std::countr_zero(std::max(static_cast<uint32_t>(alignof(T)), MinAlign));
	}

  public:
	

	static constexpr int CanonicalAddressSize = 48;
	static constexpr int LowTagBits = calcLowTagBits();
	static constexpr int HighTagBits = (sizeof(void*) * 8) - CanonicalAddressSize;
	static constexpr int TotalTagBits = LowTagBits + HighTagBits;
	static constexpr int PointerBits = sizeof(void*) * 8 - TotalTagBits;
	static constexpr uint32_t MaxTagValue = (1U << TotalTagBits) - 1;
	#if defined(__x86_64__) || defined(_M_X64)
	// Having this as a constexpr, so it's easier to create the natvis
	static constexpr uint64_t x64SignBit = 1ULL << (PointerBits-1); 
	#endif

	// Sanity checks to make sure everything is ok
	static_assert(CanonicalAddressSize > 0 && CanonicalAddressSize <= 64);
	static_assert(LowTagBits >= 0);
	static_assert(LowTagBits <= CanonicalAddressSize);
	static_assert(TotalTagBits < 64);
	static_assert(PointerBits > 0);

  protected:

	union
	{
		struct
		{
			uint64_t ptr : PointerBits;
			uint64_t tag : TotalTagBits;
		} m_bits;

		uint64_t m_raw;
	};

  public:

	TaggedPtr()
		: m_bits{0}
	{
		// If this one triggers then there is a bug in the implementation, as the union should ensure that the size of TaggedPtr is
		// the same as a pointer.
		static_assert(sizeof(TaggedPtr) == sizeof(void*), "TaggedPtr must be the same size as a pointer");
	}

	TaggedPtr(T* p, uint64_t tag = 0)
	{
		setPtr(p);
		setTag(static_cast<uint32_t>(tag));
	}

	void setPtr(T* ptr)
	{
		auto ch = reinterpret_cast<uint64_t>(ptr) & ((1ULL << LowTagBits) - 1);

		CZ_CHECK_F(
			ch == 0,
			"Pointer is not properly aligned to fit the low tag bits");

		m_bits.ptr = reinterpret_cast<uint64_t>(ptr) >> LowTagBits;
	}

	T* getPtr() const
	{
		uint64_t p = m_bits.ptr;
		#if defined(__x86_64__) || defined(_M_X64)
		p = (p ^ x64SignBit) - x64SignBit;
		p <<= LowTagBits;
		#endif
		return reinterpret_cast<T*>(p);
	}

	uint64_t getRaw()
	{
		return m_raw;
	}

	void setTag(uint32_t data)
	{
		// If this one triggers then the tag value is too big to fit in the available tag bits.
		assert(data <= MaxTagValue);

		m_bits.tag = data;
	}

	uint32_t getTag() const
	{
		return static_cast<uint32_t>(m_bits.tag);
	}

	template<
		typename U = T,
		typename = std::enable_if_t<!std::is_same_v<T, void>>
		>
	U& operator*() const
	{
		return *getPtr();
	}

	template<
		typename U = T,
		typename = std::enable_if_t<!std::is_same_v<T, void>>
		>
	U* operator->() const
	{
		return getPtr();
	}

};

#endif

} // namespace cz

