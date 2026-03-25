#include "TestUtils.h"
#include "crazygaze/core/Handles.h"

using namespace cz;

struct Foo
{
	int id = ms_idCounter++;
	Foo& operator=(Foo&) = default;
	Foo& operator=(const Foo&) = default;

	Foo() = default;

	Foo(std::string_view extra)
	{
		str += extra;
	}

	~Foo()
	{
		id = -1;
	}
	std::string str = std::format("{}---------------------", id);
	static inline int ms_idCounter = 0;
};


TEST_CASE("Handles", "[Handles]")
{
	Handle<Foo> h0;
	Handle<Foo> h1;
	Handle<Foo> h2;
	Handle<Foo> h3;
	CHECK(h0.isValid() == false);

	h0 = Handle<Foo>::create("Handle 0");
	h1 = Handle<Foo>::create("Handle 1");
	h2 = Handle<Foo>::create("Handle 2");
	h3 = Handle<Foo>::create("Handle 3");

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


	// Due to the way the handles are implemented, we know what index new handles should end up...
	h1 = Handle<Foo>::create("New Handle 1");
	h3 = Handle<Foo>::create("New Handle 3");
	CHECK(h0.isValid());
	CHECK(h1.isValid());
	CHECK(h2.isValid());
	CHECK(h3.isValid());

	// Now insert a fresh one into a non-existing slot, to see if freeNext works propertly
	Handle<Foo> h4 = Handle<Foo>::create("Handle 4");
	CHECK(h4.isValid());

	//Handle<Foo>::storage.reset();
	printf("");



}
