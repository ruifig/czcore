#include "crazygaze/core/UUID.h"

using namespace cz;

TEST_CASE("UUID", "[UUID]")
{
	
	UUID u;
	CHECK((u.values.a == 0 && u.values.b == 0 && u.values.c == 0 && u.values.d == 0));
	CHECK(u.isValid() == false);

	std::string str1 = "123456789ABCDEF123456789ABCDEF12";
	std::string str2 = "12345678-9ABC-DEF1-2345-6789ABCDEF12";

	UUID u2(0x12345678, 0x9ABCDEF1, 0x23456789, 0xABCDEF12);
	CHECK(fromString("aaa", u) == false);

	u = u2;
	CHECK(u == u2);
	CHECK(toString(u, UUID::FormatType::N) == str1);
	CHECK(toString(u, UUID::FormatType::D) == str2);
	CHECK(toString(u, UUID::FormatType::B) == "{12345678-9ABC-DEF1-2345-6789ABCDEF12}");
	CHECK(toString(u, UUID::FormatType::P) == "(12345678-9ABC-DEF1-2345-6789ABCDEF12)");
	u = {};
	CHECK(u.isValid() == false);

	CHECK(fromString(str1, u) == true);
	CHECK(toString(u, UUID::FormatType::N) == str1);

	CHECK(fromString(str2, u) == true);
	CHECK(toString(u, UUID::FormatType::D) == str2);

	CHECK(fromString(std::string("{") + str2 + "}", u) == true);
	CHECK(toString(u, UUID::FormatType::B) == (std::string("{") + str2 + "}"));

	CHECK(fromString(std::string("(") + str2 + ")", u) == true);
	CHECK(toString(u, UUID::FormatType::P) == (std::string("(") + str2 + ")"));

	CHECK(u == u2);
	u = {};

	std::map<UUID, std::string> m;
	m[UUID(1,2,3,4)] = "1";
	m[UUID(1,2,3,5)] = "2";
	m[UUID(1,2,3,6)] = "3";
	m[UUID(1,2,3,6)] = "4"; // duplicate
	CHECK(m.size() == 3);
}
