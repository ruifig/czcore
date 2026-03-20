#include "TestUtils.h"

using namespace cz;
using namespace cz::hash;


struct Foo
{
	int a;
	int b;
	double c;
};


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

	// Test derreferencing the pointer
	std::string str = "Hello World";
	TaggedPtr<std::string> ptr(&str);
	CHECK(*ptr == "Hello World");
}

