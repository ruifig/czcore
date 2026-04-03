#include "crazygaze/core/LogOutputs.h"

using namespace cz;
using namespace cz::hash;

namespace FixedHeapArrayTests_details
{

int gIdCounter = 0;
struct Counters
{
	int defaultContruct = 0;
	int copyConstruct = 0;
	int moveConstruct = 0;
	int copy = 0;
	int move = 0;
	int destruct = 0;

	void check(int defaultConstruct_, int copyConstruct_, int moveConstruct_, int copy_, int move_, int destruct_)
	{
		CHECK(this->defaultContruct == defaultConstruct_);
		CHECK(this->copyConstruct == copyConstruct_);
		CHECK(this->moveConstruct == moveConstruct_);
		CHECK(this->copy == copy_);
		CHECK(this->move == move_);
		CHECK(this->destruct == destruct_);	
	}

} gCounters;

struct Foo
{
	int id;
	Foo()
		: id(gIdCounter++)
	{
		gCounters.defaultContruct++;
	}

	~Foo()
	{
		gCounters.destruct++;
	}

	Foo(const Foo& other)
	{
		id = other.id;
		gCounters.copyConstruct++;
	}

	Foo(Foo&& other)
	{
		id = other.id;
		other.id = -1;
		gCounters.moveConstruct++;
	}

	Foo& operator=(const Foo& other)
	{
		id = other.id;
		gCounters.copy++;
		return *this;
	}

	Foo& operator=(Foo&& other)
	{
		id = other.id;
		other.id = -1;
		gCounters.move++;
		return *this;
	}
};

/**
 * This tests if FixedHeapArray has the same behaviour as std::vector (for the supported API)
 *
 * Since this test checks how many times Foo constructors/destructors/assignments are called to make sure
 * FixedHeapArray behaves the same was as std::vector, it is very dependent on how std::vector does things internaly.
 * This was tested with msvc, and thus any other STL implementations might cause this to fail.
 *
 */
template <typename V>
void test1()
{

	{
		gCounters = {};
		V a;
		CHECK(a.size() == 0);
		CHECK(a.data() == nullptr);
		gCounters.check(0, 0, 0, 0, 0, 0);

	}

	// Test constructor (count)
	{
		gCounters = {};
		V a(2);
		gCounters.check(2, 0, 0, 0, 0, 0);
	}

	// Test constructor (count, value)
	{
		gCounters = {};
		V a(2, Foo());
		gCounters.check(1, 2, 0, 0, 0, 1);
	}

	// Test copy constructor
	{
		V a(2, Foo());
		gCounters = {};
		V b(a);
		gCounters.check(0, 2, 0, 0, 0, 0);
		a = {};
		b = {};
		// Test destructors
		gCounters.check(0, 2, 0, 0, 0, 4);
	}

	// Test move constructor
	{
		V a(2, Foo());
		gCounters = {};
		V b(std::move(a));
		// NOTE: Because the implementations just swap the internals, no Foo constructors actually get called
		gCounters.check(0, 0, 0, 0, 0, 0);
		a = {};
		b = {};
		// Test destructors
		gCounters.check(0, 0, 0, 0, 0, 2);
	}

	// Test copy assignment
	{
		V a(2, Foo());
		gCounters = {};
		V b(1);
		b = a;
		// NOTE: Because the implementations just swap the internals, no Foo constructors actually get called
		gCounters.check(1, 2, 0, 0, 0, 1);
		a = {};
		b = {};
		// Test destructors
		gCounters.check(1, 2, 0, 0, 0, 5);
	}

	// Test move assignment
	{
		V a(2, Foo());
		gCounters = {};
		V b(1);
		b = std::move(a);
		// NOTE: Because the implementations just swap the internals, no Foo constructors actually get called
		gCounters.check(1, 0, 0, 0, 0, 1);
		a = {};
		b = {};
		// Test destructors
		gCounters.check(1, 0, 0, 0, 0, 3);
	}

	// Test destructor
	{
		gCounters = {};
		V a(2, Foo());
		CHECK(a.size() == 2);
		a = V();
		CHECK(a.size() == 0);
		CHECK(a.data() == nullptr);
		gCounters.check(1, 2, 0, 0, 0, 3);
	}

	// constructor with first and last
	{
		gCounters = {};
		Foo tmp[2] = { {}, {} };
		V a(std::begin(tmp), std::end(tmp));
		gCounters.check(2, 2, 0, 0, 0, 0);
		a = {};
		gCounters.check(2, 2, 0, 0, 0, 2);
	}

	// constructor with initializer list
	{
		gCounters = {};
		V a({Foo(), Foo()});
		gCounters.check(2, 2, 0, 0, 0, 2);
	}

	// [] operator
	{
		gCounters = {};
		V a({Foo(), Foo()});
		a[0].id = -10;
		a[1].id = -11;

		CHECK(a[0].id == -10);
		CHECK(a[1].id == -11);
	}

	{
		V a;
		// If empty, then begin() and end() need to be equal
		CHECK((a.begin() == a.end()));
	}

	// Test iterators
	{
		V a({Foo(), Foo()});
		a[0].id = -10;
		a[1].id = -11;

		std::vector<int> tmp;
		for(auto&& foo : a)
			tmp.push_back(foo.id);

		REQUIRE_THAT(tmp, Catch::Matchers::RangeEquals({-10, -11}));
	}

	// Test reverse iterators
	{
		V a({Foo(), Foo()});
		a[0].id = -10;
		a[1].id = -11;

		std::vector<int> tmp;
		for (auto it = a.rbegin(); it != a.rend(); ++it)
			tmp.push_back(it->id);

		REQUIRE_THAT(tmp, Catch::Matchers::RangeEquals({-11, -10}));
	}

	// span
	{
		V a({Foo(), Foo()});
		a[0].id = -10;
		a[1].id = -11;
		std::span s{a};

		std::vector<int> tmp;
		for(auto&& foo : s)
			tmp.push_back(foo.id);

		REQUIRE_THAT(tmp, Catch::Matchers::RangeEquals({-10, -11}));
	}

}

}

using namespace FixedHeapArrayTests_details;

TEST_CASE("FixedHeapArray", "[FixedHeapArray]")
{
	test1<std::vector<Foo>>();
	test1<FixedHeapArray<Foo, true>>();
	test1<FixedHeapArray<Foo, false>>();
}

