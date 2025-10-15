
#include "crazygaze/core/LogOutputs.h"
#include "TestUtils.h"

using namespace cz;
using namespace cz::hash;

TEST_CASE("FNVHash", "[FNVHash]")
{
	//LogOutputs logs;

	{
		using hasher = hash::fnv1a<uint32_t>;
		static_assert( hasher::hash("FNV Hash Test") == 0xf38b3db9, "fnv1a_32::hash failure" );
		static_assert( hasher::hash("FNV Hash Test") == "FNV Hash Test"_fnv1a_32, "fnv1a_32::hash failure" );
		static_assert( hasher::hash("FNV Hash Test", 13, hasher::default_offset_basis) == "FNV Hash Test"_fnv1a_32, "fnv1a_32::hash failure" );
	}

	{
		using hasher = hash::fnv1a<uint64_t>;
		static_assert( hasher::hash("FNV Hash Test") == 0xa4f804419ad5b1f9, "fnv1a_32::hash failure" );
		static_assert( hasher::hash("FNV Hash Test") == "FNV Hash Test"_fnv1a_64, "fnv1a_32::hash failure" );
		static_assert( hasher::hash("FNV Hash Test", 13, hasher::default_offset_basis) == "FNV Hash Test"_fnv1a_64, "fnv1a_32::hash failure" );
	}

	const char* str = "Hello World!";

	// 32 bits
	{
		CHECK(Hash::fnv_32a_str(str) == 0xb1ea4872);
		// Test appending
		CHECK(Hash::fnv_32a_str(str, Hash::fnv_32a_str(str)) == 0x6b5b4eb9);

		CHECK(Hash::fnv_32a_buf(str, 12) == 0xb1ea4872);
		// Test appending
		CHECK(Hash::fnv_32a_buf(str, 12, Hash::fnv_32a_buf(str, 12)) == 0x6b5b4eb9);
	}

	// 64 bits
	{
		CHECK(Hash::fnv_64a_str(str) == 0x8c0ec8d1fb9e6e32);
		// Test appending
		CHECK(Hash::fnv_64a_str(str, Hash::fnv_64a_str(str)) == 0xee5e71628b3d7719);

		CHECK(Hash::fnv_64a_buf(str, 12) == 0x8c0ec8d1fb9e6e32);
		// Test appending
		CHECK(Hash::fnv_64a_buf(str, 12, Hash::fnv_64a_buf(str, 12)) == 0xee5e71628b3d7719);
	}
}


