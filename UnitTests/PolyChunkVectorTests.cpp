#include <print>
#include "crazygaze/core/PolyChunkVector.h"
#include "crazygaze/core/ScopeGuard.h"


// TODO
// X - Test when 1 single object is larger than chunk size
//		X - Test when sizeof(Header)+sizeof(Derived) is exactly equal and larger than chunk size
//		X - Test when there is an already allocated free chunk, but it's smaller than the needed size.
// X - Test if all chunks are deleted
// - Test oob data

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

} // anonymous namespace

// Create a testable container
class PV : public PolyChunkVector<Base>
{
  public:
	using PolyChunkVector::PolyChunkVector;
	static constexpr int baseSize = sizeof(Header) + sizeof(Base);
	static constexpr int fooSize  = sizeof(Header) + sizeof(Foo);

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
	 * Returns a vector of pairs, with each pair being the used (first) and total capacity of each chunk (second)
	 */
	std::vector<std::pair<int, int>> _dbgGetChunks() const
	{
		std::vector<std::pair<int, int>> chunks;
		for (const Chunk* c = m_head; c != nullptr; c = c->next)
		{
			chunks.emplace_back(static_cast<int>(c->usedCap), static_cast<int>(c->cap));
		}
		return chunks;
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

void checkChunks(const PV& v, const std::vector<std::pair<int, int>>& expected)
{
	auto chunks = v._dbgGetChunks();
	CHECK(chunks.size() == expected.size());
	for (size_t i = 0; i < expected.size(); i++)
	{
		CHECK(chunks[i].first == expected[i].first);
		CHECK(chunks[i].second == expected[i].second);
	}
}

TEST_CASE("PolyChunkVector", "[PolyChunkVector]")
{
	SECTION("2 full chunks")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		std::println("sizeof(Base) = {}", sizeof(Base));

		PV v(sizeof(Base));

		// We tried to initialize with sizeof(Base), but it gets aligned, because it needs to fit at least 1 header + 1 object
		checkChunks(v, {{0, PV::baseSize}});

		// Use one chunk completely
		v.emplace_back<Base>(0x1122334455667788u);
		checkChunks(v, {{PV::baseSize, PV::baseSize}});

		// Use the second chunk completely
		v.emplace_back<Base>(0x1122334455667788u);
		checkChunks(v, {{PV::baseSize, PV::baseSize}, {PV::baseSize, PV::baseSize}});

		v.clear();
		checkChunks(v, {{0, PV::baseSize}, {0, PV::baseSize}});
	}

	SECTION("2 partially used chunks")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		constexpr int chunkSize = PV::baseSize + 16;
		PV v(chunkSize);

		checkChunks(v, {{0, chunkSize}});

		// Use one chunk partially
		v.emplace_back<Base>(0x1122334455667788u);
		checkChunks(v, {{PV::baseSize, chunkSize}});

		// Try to insert something that doesn't fit in the first chunk even tough it has some space left
		v.emplace_back<Base>(0x1122334455667788u);
		checkChunks(v, {{PV::baseSize, chunkSize}, {PV::baseSize, chunkSize}});
	}

	SECTION("Iterators")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		constexpr int chunkSize = PV::baseSize + 8;
		PV v(chunkSize);
		v.emplace_back<Base>(1u);
		v.emplace_back<Base>(2u);
		v.emplace_back<Base>(3u);
		v.emplace_back<Base>(4u);
		v.emplace_back<Base>(5u);
		checkChunks(v,
			{{PV::baseSize, chunkSize},
			 {PV::baseSize, chunkSize},
			 {PV::baseSize, chunkSize},
			 {PV::baseSize, chunkSize},
			 {PV::baseSize, chunkSize}});

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

		constexpr int chunkSize = PV::baseSize * 2 + PV::fooSize * 2 + 8;
		PV v(chunkSize);

		// 1 chunk
		v.emplace_back<Base>(1u);
		v.emplace_back<Foo> (2u);
		v.emplace_back<Base>(3u);
		v.emplace_back<Foo> (4u);
		checkChunks(v, {{chunkSize - 8, chunkSize}});

		// 1 chunk
		v.emplace_back<Base>(5u);
		v.emplace_back<Foo> (6u);
		v.emplace_back<Base>(7u);
		v.emplace_back<Foo> (8u);
		checkChunks(v,
			{{chunkSize - 8, chunkSize},
			 {chunkSize - 8, chunkSize}});

		// 1 chunk
		v.emplace_back<Base>(9u);
		v.emplace_back<Foo> (10u);
		checkChunks(v,
			{{chunkSize - 8, chunkSize},
			 {chunkSize - 8, chunkSize},
			 {PV::baseSize + PV::fooSize, chunkSize}});

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
		checkChunks(v,
			{{PV::baseSize, PV::baseSize},
			 {PV::baseSize, PV::baseSize},
			 {PV::baseSize, PV::baseSize}});

		// By default, clear keeps the chunks
		v.clear();
		CHECK(v._dbgGetNumChunks() == std::make_pair(0, 3));
		CHECK(v._dbgCheckTailUsedCapacity() == std::make_pair(0, PV::baseSize));
		CHECK(v.calcCapacity() == std::make_pair(0, PV::baseSize * 3));
		checkChunks(v, {{0, PV::baseSize}, {0, PV::baseSize}, {0, PV::baseSize}});

		// Foo is bigger, so it will need a new chunk, which is added at the end of the chain
		v.emplace_back<Foo>(4u);
		checkChunks(v,
			{{0, PV::baseSize},
			 {0, PV::baseSize},
			 {0, PV::baseSize},
			 {PV::fooSize, PV::fooSize}});

		// Inserting a Base will cause another chunk to be allocated, with a capacity that is now bigger than the first 3,
		// because it picks up the capacity of the last chunk in the chain
		v.emplace_back<Base>(5u);
		CHECK(v._dbgGetNumChunks() == std::make_pair(2, 3));
		CHECK(v.calcCapacity() == std::make_pair(PV::fooSize + PV::baseSize, PV::baseSize * 3 + PV::fooSize*2));
		checkChunks(v,
			{{0, PV::baseSize},
			 {0, PV::baseSize},
			 {0, PV::baseSize},
			 {PV::fooSize, PV::fooSize},
			 {PV::baseSize, PV::fooSize}});
	}

	SECTION("Iterators with holes")
	{
		resetCounters();
		CZ_SCOPE_EXIT { checkCounters(); };

		PV v(PV::baseSize);

		uint64_t count = 0;
		for(Base& obj : v)
		{
			count++;
			CHECK(obj.a == 0); // unused
		}
		CHECK(count == 0);

		// Fill 3 chunk
		v.emplace_back<Base>(1u);
		v.emplace_back<Base>(2u);
		v.emplace_back<Base>(3u);

		// By default, clear keeps the chunks
		v.clear();

		// Foo is bigger, so it will need a new chunk, leaving the first 3 chunks empty and unused
		v.emplace_back<Foo>(4u);
		checkChunks(v,
			{{0, PV::baseSize},
			 {0, PV::baseSize},
			 {0, PV::baseSize},
			 {PV::fooSize, PV::fooSize}});

		count = 0;
		for(Base& obj : v)
		{
			count++;
			CHECK(obj.a == 4);
		}

		CHECK(count == 1);

	}

}

namespace
{
	
template<size_t ChunkSize>
class CmdQueue
{
  public:

	struct Cmd
	{
		virtual ~Cmd() = default;
		virtual void execute(std::vector<int>& dst) = 0;
	};

	PolyChunkVector<Cmd> m_cmds;

	CmdQueue()
		: m_cmds(ChunkSize)
	{
	}

	template<typename F>
	requires std::invocable<F, std::vector<int>&>
	struct CmdWrapper : public Cmd
	{
		CmdWrapper(F&& f)
			: payload(std::forward<F>(f))
		{
		}
		
		void execute(std::vector<int>& dst) override
		{
			payload(dst);
		}

		F payload;
	};

	template<typename F>
	Cmd& push(F&& f)
	{
		return m_cmds.emplace_back<CmdWrapper<F>>(std::forward<F>(f));
	}

	void executeAll(std::vector<int>& dst)
	{
		for (Cmd& cmd : m_cmds)
		{
			cmd.execute(dst);
		}
	};
};

class CmdQueue2
{

  private: 

	std::vector<std::function<void(std::vector<int>&)>> m_cmds;

  public:

	template<typename F>
	requires std::invocable<F, std::vector<int>&>
	void push(F&& f)
	{
		m_cmds.emplace_back(std::forward<F>(f));
	}

	void executeAll(std::vector<int>& dst)
	{
		for (const auto& cmd : m_cmds)
		{
			cmd(dst);
		}
	}
};

}  // namespace

std::vector<int> gDummy;


template<typename QType>
double testCmdQueue(const int numCmds)
{
	QType q;

	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < numCmds; i++)
	{
		q.push([i](std::vector<int>& dst)
			{
				dst.push_back(i);
			});
	}
	auto delta = std::chrono::high_resolution_clock::now() - start;
	std::chrono::duration<double, std::milli> duration = delta;
	std::println("Duration: {}", duration.count());

	gDummy.reserve(static_cast<size_t>(numCmds));
	q.executeAll(gDummy);
	CHECK(gDummy.size() == static_cast<size_t>(numCmds));
	gDummy = std::vector<int>{};
	return duration.count();
};

TEST_CASE("PV_benchmark")
{
	double total = 0;
	int count = 10;
	for (int i = 0; i < count; i++)
	{
		constexpr int numCmds = 10000000;
		//total += testCmdQueue<CmdQueue<size_t(24)*100000000>>(100000000);
		//total += testCmdQueue<CmdQueue<10*1024*1024>>(numCmds);
		total += testCmdQueue<CmdQueue2>(numCmds);
	}
	std::println("Average = {} ms", total / count);
}



