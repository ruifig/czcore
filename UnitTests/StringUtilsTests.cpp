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

