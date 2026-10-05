#include "crazygaze/core/LogOutputs.h"

using namespace cz;

namespace
{

void createTestFile(std::string_view filename, std::string_view contents)
{
	std::unique_ptr<File> f = File::open(filename, File::Mode::Write);
	CHECK(f);
	f->write(contents);
}

}

TEST_CASE("Iterators", "[File]")
{

	SECTION("When the file exists but is empty")
	{
		createTestFile("czcore_test_file.txt", "");

		File::Buffer fbuffer = File::tryReadAll("czcore_test_file.txt");
		CZ_CHECK(fbuffer);
		CZ_CHECK(fbuffer.size == 0);

		int count = 0;
		for(uint8_t& ch : fbuffer)
			count++;

		CHECK(count == 0);
	}

	SECTION("When it failed to read")
	{
		File::Buffer fbuffer = File::tryReadAll("Z:/superduper.txt");

		CHECK((!fbuffer));

		int count = 0;
		for(uint8_t& ch : fbuffer)
			count++;

		CHECK(count == 0);
	}

	SECTION("When the file has contents")
	{
		createTestFile("czcore_test_file.txt", "Hello World!");

		File::Buffer fbuffer = File::tryReadAll("czcore_test_file.txt");
		CZ_CHECK(fbuffer);
		CZ_CHECK(fbuffer.size == 12);

		std::string str;
		for(uint8_t& ch : fbuffer)
			str.push_back(static_cast<char>(ch));

		CHECK(str == "Hello World!");
	}



}
