#include "TestUtils.h"

using namespace cz;

namespace
{

template<typename T>
void testTaggedPtr()
{
	using TPtr = TaggedPtr<T>;

	// To test the tag value, we use the maximum tag value with a bit in the middle set to 0
	// This is to make sure we don't have any bugs related to the left-most or right-most bits of the
	// tag
	constexpr uint32_t tagValue = TPtr::MaxTagValue ^ (1 << (TPtr::TotalTagBits / 2));

	// Test sign extending the top bit (as required by x64)
	uint64_t v1 = (1ULL << (TPtr::CanonicalAddressSize-1)) + 8;
	TPtr ptr(reinterpret_cast<T*>(v1));
	ptr.setTag(tagValue);
	uint64_t v2 = reinterpret_cast<uint64_t>(ptr.getPtr());
	uint64_t v3 = ~((1ULL << TPtr::CanonicalAddressSize) -1); // What the higher bits should be
	CHECK(v2 == (v1 | v3));

	// test the tag
	uint32_t tag = ptr.getTag();
	CHECK(tag == tagValue);
}

} // anonymous namespace

TEST_CASE("TaggedPtr", "[TaggedPtr]")
{
	testTaggedPtr<void>();
	testTaggedPtr<uint8_t>();
	testTaggedPtr<int>();
	testTaggedPtr<double>();

	TaggedPtr<std::string> emptyPtr;
	emptyPtr.setTag(10);

	// Test dereferencing the pointer
	std::string str = "Hello World";
	TaggedPtr<std::string> ptr(&str);
	ptr.setTag(12345);
	CHECK(*ptr == "Hello World");
	CHECK(std::string(ptr->c_str()) == "Hello World");
}

TEST_CASE("TaggedPtr minAlign", "[TaggedPtr]")
{

	{
		TaggedPtr<uint8_t> c;
		CHECK(c.TotalTagBits == 16);
	}
	{
		TaggedPtr<uint8_t, 1> c;
		CHECK(c.TotalTagBits == 16);
	}
	{
		TaggedPtr<uint8_t, 2> c;
		CHECK(c.TotalTagBits == 17);
	}

	{
		TaggedPtr<uint16_t, 0> c;
		CHECK(c.TotalTagBits == 17);
	}
	{
		TaggedPtr<uint16_t, 1> c;
		CHECK(c.TotalTagBits == 17);
	}
	{
		TaggedPtr<uint16_t, 2> c;
		CHECK(c.TotalTagBits == 17);
	}
	{
		TaggedPtr<uint16_t, 4> c;
		CHECK(c.TotalTagBits == 18);
	}

}



