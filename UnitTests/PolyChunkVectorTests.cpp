#include <print>
#include "crazygaze/core/PolyChunkVector.h"
#include "crazygaze/core/ScopeGuard.h"


// TODO
// X - Test when 1 single object is larger than chunk size
//		X - Test when sizeof(Header)+sizeof(Derived) is exactly equal and larger than chunk size
//		- Test when there is an already allocated free chunk, but it's smaller than the needed size.
//			- In such cases it should deallocate the chunk and replace	with a new one.
// - Test oob data
// X - Test if all chunks are deleted

using namespace cz;

namespace
{

struct Base
{
	inline static int creationNum = 0;
	inline static int destructionNum = 0;
	Base( uint64_t a )
		: a(a)
	{
		baseNum = creationNum++;
		std::println(" Base() : {}, {}", baseNum, a);
	}

	virtual ~Base()
	{
		CHECK(baseNum == destructionNum);
		destructionNum++;
		std::println("~Base() : {}, {}", baseNum, a);
	}

	int baseNum;
	uint64_t a;
};

struct Foo : Base
{
	inline static int creationNum = 0;
	inline static int destructionNum = 0;

	Foo(uint64_t a)
		: Base(a)
	{
		fooNum = creationNum++;
		std::println(" Foo() : {}, {}", fooNum, a);
	}

	~Foo()
	{
		CHECK(fooNum == destructionNum);
		destructionNum++;
		std::println("~Foo() : {}, {}", fooNum, a);
	}

	int fooNum;
	int64_t dummy = -1;
};

void resetCounters()
{
	Base::creationNum = 0;
	Base::destructionNum = 0;
	Foo::creationNum = 0;
	Foo::destructionNum = 0;
}

void checkCounters()
{
	CHECK(Base::creationNum == Base::destructionNum);
	CHECK(Foo::creationNum == Foo::destructionNum);
}

} // anomymous namespace

// Create a testable container
class PV : public PolyChunkVector<Base>
{
  public:
	using PolyChunkVector::PolyChunkVector;
	static constexpr size_t baseSize = sizeof(Header) + sizeof(Base);
	static constexpr size_t fooSize = sizeof(Header) + sizeof(Foo);

	/**
	 * Used just for debugging
	 * first - Total number of chunks with elements
	 * second - Total number of empty chunks
	 */
	std::pair<int,int> _dbgGetNumChunks() const
	{
		int withData = 0;
		int withoutData = 0;
		for (const Chunk* c = m_head; c != nullptr; c = c->next)
		{
			if (c->usedCap > 0)
				withData++;
			else
				withoutData++;
		}

		return {withData, withoutData};
	}

	/**
	 * Used just for debugging.
	 * It returns the used and total capacity of the current chunk
	 */
	std::pair<size_t, size_t> _dbgCheckTailUsedCapacity() const
	{
		if (m_tail)
			return {m_tail->usedCap, m_tail->cap};
		else
			return {};
	}

};

TEST_CASE("PolyChunkVector", "[PolyChunkVector]")
{
	SECTION("")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		std::println("sizeof(Base) = {}", sizeof(Base));

		PV v(sizeof(Base));

		CHECK(v._dbgGetNumChunks() == std::make_pair(0, 1));
		// We tried to initialize with sizeof(Base), but it gets aligned, because it needs to fit at least 1 header + 1 object
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(0, PV::baseSize));

		// Use one chunk completely
		v.emplace_back<Base>(0x1122334455667788u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(1, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(PV::baseSize, PV::baseSize));

		// Use the second chunk completely
		v.emplace_back<Base>(0x1122334455667788u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(2, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(PV::baseSize, PV::baseSize));
		CHECK(v.calcCapacity().first == PV::baseSize * 2);

		v.clear();
		CHECK(v._dbgGetNumChunks() == std::make_pair(0, 2));
		CHECK(v.calcCapacity() == std::make_pair(0, PV::baseSize * 2));
	}

	SECTION("")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		constexpr size_t chunkSize = PV::baseSize + 16;
		PV v(chunkSize);

		CHECK(v._dbgGetNumChunks() == std::make_pair(0, 1));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(0, chunkSize));

		// Use one chunk partially
		v.emplace_back<Base>(0x1122334455667788u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(1, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(PV::baseSize, chunkSize));

		// Try to insert something that doesn't fit in the first chunk even tough it has some space left
		v.emplace_back<Base>(0x1122334455667788u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(2, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(PV::baseSize, chunkSize));
		CHECK(v.calcCapacity() == std::make_pair(PV::baseSize * 2, chunkSize * 2));
	}

	SECTION("Iterators")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		constexpr size_t chunkSize = PV::baseSize + 8;
		PV v(chunkSize);
		v.emplace_back<Base>(1u);
		v.emplace_back<Base>(2u);
		v.emplace_back<Base>(3u);
		v.emplace_back<Base>(4u);
		v.emplace_back<Base>(5u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(5, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(PV::baseSize, chunkSize));
		CHECK(v.calcCapacity() == std::make_pair(PV::baseSize * 5, chunkSize * 5));

		uint64_t count = 0;
		for(Base& obj : v)
		{
			count++;
			CHECK(obj.a == count);
		}

		CHECK(count == 5);
	}

	SECTION("Derived")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		constexpr size_t chunkSize = PV::baseSize * 2 + PV::fooSize * 2 + 8;
		PV v(chunkSize);

		// 1 chunk
		v.emplace_back<Base>(1u);
		v.emplace_back<Foo> (2u);
		v.emplace_back<Base>(3u);
		v.emplace_back<Foo> (4u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(1, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(chunkSize - 8, chunkSize));

		// 1 chunk
		v.emplace_back<Base>(5u);
		v.emplace_back<Foo> (6u);
		v.emplace_back<Base>(7u);
		v.emplace_back<Foo> (8u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(2, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(chunkSize - 8, chunkSize));

		// 1 chunk
		v.emplace_back<Base>(9u);
		v.emplace_back<Foo> (10u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(3, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(PV::baseSize + PV::fooSize, chunkSize));

		CHECK(v.calcCapacity() == std::make_pair(PV::baseSize * 5 + PV::fooSize * 5, chunkSize * 3));

		CHECK(v.size() == 10);

		uint64_t count = 0;
		for(Base& obj : v)
		{
			count++;
			CHECK(obj.a == count);
		}

		CHECK(count == 10);
	}

	SECTION("clear")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		PV v(PV::baseSize);

		// Fill 3 chunk
		v.emplace_back<Base>(1u);
		v.emplace_back<Base>(2u);
		v.emplace_back<Base>(3u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(3, 0));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(PV::baseSize, PV::baseSize));
		CHECK(v.calcCapacity() == std::make_pair(PV::baseSize * 3, PV::baseSize * 3));

		// By default, clear keeps the chunks
		v.clear();
		CHECK(v._dbgGetNumChunks() == std::make_pair(0, 3));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(0, PV::baseSize));
		CHECK(v.calcCapacity() == std::make_pair(0, PV::baseSize * 3));

		SECTION("Deallocate first")
		{
			// Foo is bigger, so it will need a new chunk
			v.emplace_back<Foo>(4u);
		}



	}
}

