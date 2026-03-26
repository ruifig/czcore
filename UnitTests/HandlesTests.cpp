#include "TestUtils.h"
#include "crazygaze/core/Handles.h"

using namespace cz;

struct HandleFoo
{
	int id = ms_idCounter++;
	HandleFoo& operator=(HandleFoo&) = default;
	HandleFoo& operator=(const HandleFoo&) = default;

	HandleFoo() = default;
	HandleFoo(std::string_view extra)
	{
		str += extra;
	}

	~HandleFoo()
	{
		id = -1;
	}
	std::string str = std::format("{}---------------------", id);
	static inline int ms_idCounter = 0;
};

TEMPLATE_TEST_CASE("Handles", "[Handles]", uint32_t, uint64_t)
{
	using HT = HandleImpl<HandleFoo, TestType>;
	static_assert(sizeof(HT) == sizeof(TestType));
	HandleFoo::ms_idCounter = 0;

	HT h0;
	HT h1;
	HT h2;
	HT h3;
	CHECK(h0.isValid() == false);

	h0 = HT::create("Handle 0");
	h1 = HT::create("Handle 1");
	h2 = HT::create("Handle 2");
	h3 = HT::create("Handle 3");

	CHECK(h0.isValid());
	CHECK(h1.isValid());
	CHECK(h2.isValid());
	CHECK(h3.isValid());

	h3.release();
	CHECK(h3.isValid() == false);

	h1.release();
	CHECK(h1.isValid() == false);

	CHECK(h0.isValid());
	CHECK(h2.isValid());

	// Test iterator
	{
		std::vector<int> expectedIds = {0,2};

		// non-const
		size_t idx = 0;
		for(HandleFoo& f : HT::storage)
		{
			CHECK(f.id == expectedIds[idx]);
			idx++;
		}

		// const
		idx = 0;
		for(const HandleFoo& f : (const decltype(HT::storage)&)HT::storage)
		{
			CHECK(f.id == expectedIds[idx]);
			idx++;
		}
	}

	// Due to the way the handles are implemented, we know what index new handles should end up...
	h1 = HT::create("New Handle 1");
	h3 = HT::create("New Handle 3");
	CHECK(h0.isValid());
	CHECK(h1.isValid());
	CHECK(h2.isValid());
	CHECK(h3.isValid());

	// Now insert a fresh one into a non-existing slot, to see if freeNext works propertly
	HT h4 = HT::create("Handle 4");
	CHECK(h4.isValid());
}



