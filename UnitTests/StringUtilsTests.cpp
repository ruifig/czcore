#include "TestUtils.h"
#include "crazygaze/core/StringUtils.h"


TEST_CASE("trim_char", "[StringUtils]")
{
	CHECK(std::string(cz::ltrim("   Hello World   "))       == "Hello World   ");
	CHECK(std::string(cz::ltrim("\n\t  Hello World  \n\t")) == "Hello World  \n\t");
	CHECK(std::string(cz::ltrim("NoSpaces"))                == "NoSpaces");
	CHECK(std::string(cz::ltrim("    "))                    == "");
	CHECK(std::string(cz::ltrim(""))                        == "");

	CHECK(std::string(cz::rtrim("   Hello World   "))       == "   Hello World");
	CHECK(std::string(cz::rtrim("\n\t  Hello World  \n\t")) == "\n\t  Hello World");
	CHECK(std::string(cz::rtrim("NoSpaces"))                == "NoSpaces");
	CHECK(std::string(cz::rtrim("    "))                    == "");
	CHECK(std::string(cz::rtrim(""))                        == "");

	CHECK(std::string( cz::trim("   Hello World   "))       == "Hello World");
	CHECK(std::string( cz::trim("\n\t  Hello World  \n\t")) == "Hello World");
	CHECK(std::string( cz::trim("NoSpaces"))                == "NoSpaces");
	CHECK(std::string( cz::trim("    "))                    == "");
	CHECK(std::string( cz::trim(""))                        == "");
}

TEST_CASE("trim_string_view", "[StringUtils]")
{
	CHECK(std::string(cz::ltrim(std::string_view("   Hello World   ")))       == "Hello World   ");
	CHECK(std::string(cz::ltrim(std::string_view("\n\t  Hello World  \n\t"))) == "Hello World  \n\t");
	CHECK(std::string(cz::ltrim(std::string_view("NoSpaces")))                == "NoSpaces");
	CHECK(std::string(cz::ltrim(std::string_view("    ")))                    == "");
	CHECK(std::string(cz::ltrim(std::string_view("")))                        == "");
	CHECK(std::string(cz::rtrim(std::string_view("   Hello World   ")))       == "   Hello World");
	CHECK(std::string(cz::rtrim(std::string_view("\n\t  Hello World  \n\t"))) == "\n\t  Hello World");
	CHECK(std::string(cz::rtrim(std::string_view("NoSpaces")))                == "NoSpaces");
	CHECK(std::string(cz::rtrim(std::string_view("    ")))                    == "");
	CHECK(std::string(cz::rtrim(std::string_view("")))                        == "");
	CHECK(std::string( cz::trim(std::string_view("   Hello World   ")))       == "Hello World");
	CHECK(std::string( cz::trim(std::string_view("\n\t  Hello World  \n\t"))) == "Hello World");
	CHECK(std::string( cz::trim(std::string_view("NoSpaces")))                == "NoSpaces");
	CHECK(std::string( cz::trim(std::string_view("    ")))                    == "");
	CHECK(std::string( cz::trim(std::string_view("")))                        == "");
}

TEST_CASE("trim_string", "[StringUtils]")
{
	CHECK(cz::ltrim(std::string("   Hello World   "))       == "Hello World   ");
	CHECK(cz::ltrim(std::string("\n\t  Hello World  \n\t")) == "Hello World  \n\t");
	CHECK(cz::ltrim(std::string("NoSpaces"))                == "NoSpaces");
	CHECK(cz::ltrim(std::string("    "))                    == "");
	CHECK(cz::ltrim(std::string(""))                        == "");
	CHECK(cz::rtrim(std::string("   Hello World   "))       == "   Hello World");
	CHECK(cz::rtrim(std::string("\n\t  Hello World  \n\t")) == "\n\t  Hello World");
	CHECK(cz::rtrim(std::string("NoSpaces"))                == "NoSpaces");
	CHECK(cz::rtrim(std::string("    "))                    == "");
	CHECK(cz::rtrim(std::string(""))                        == "");
	CHECK( cz::trim(std::string("   Hello World   "))       == "Hello World");
	CHECK( cz::trim(std::string("\n\t  Hello World  \n\t")) == "Hello World");
	CHECK( cz::trim(std::string("NoSpaces"))                == "NoSpaces");
	CHECK( cz::trim(std::string("    "))                    == "");
	CHECK( cz::trim(std::string(""))                        == "");
}

// For any other CharT, we just do one single test to verify it compiles and works as expected
TEST_CASE("trim_wchar", "[StringUtils]")
{
	CHECK(std::wstring(cz::ltrim(L"   Hello World   "))       == L"Hello World   ");
	CHECK(std::wstring(cz::ltrim(L"\n\t  Hello World  \n\t")) == L"Hello World  \n\t");
	CHECK(std::wstring(cz::ltrim(L"NoSpaces"))                == L"NoSpaces");
	CHECK(std::wstring(cz::ltrim(L"    "))                    == L"");
	CHECK(std::wstring(cz::ltrim(L""))                        == L"");
	CHECK(std::wstring(cz::rtrim(L"   Hello World   "))       == L"   Hello World");
	CHECK(std::wstring(cz::rtrim(L"\n\t  Hello World  \n\t")) == L"\n\t  Hello World");
	CHECK(std::wstring(cz::rtrim(L"NoSpaces"))                == L"NoSpaces");
	CHECK(std::wstring(cz::rtrim(L"    "))                    == L"");
	CHECK(std::wstring(cz::rtrim(L""))                        == L"");
	CHECK(std::wstring( cz::trim(L"   Hello World   "))       == L"Hello World");
	CHECK(std::wstring( cz::trim(L"\n\t  Hello World  \n\t")) == L"Hello World");
	CHECK(std::wstring( cz::trim(L"NoSpaces"))                == L"NoSpaces");
	CHECK(std::wstring( cz::trim(L"    "))                    == L"");
	CHECK(std::wstring( cz::trim(L""))                        == L"");
}

TEST_CASE("toString", "[StringUtils]")
{
	std::string a = "Hello world! HELLO WORLD!";

	SECTION("Passing a std::string should result in returning a const &")
	{
		auto&& b = toString(a);
		static_assert(std::is_same_v<decltype(b), const std::string&>);
		CHECK(&a == &b); // same string object
	}

	SECTION("Passing a std::string should result in returning std::string")
	{
		auto getString = [&a]() -> std::string
		{
			return a;
		};

		auto&& b = toString(getString());
		static_assert(std::is_same_v<decltype(toString(getString())), std::string>);
		CHECK(a == b);
	}

	SECTION("Passing std::string_view should return a std::string_view")
	{
		auto&& b = toString(std::string_view(a));
		static_assert(std::is_same_v<decltype(toString(std::string_view(a))), std::string_view>);
		CHECK(std::string(a) == std::string(b));

	}
}

