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

namespace
{

std::vector<uint32_t> decode(std::string_view str)
{
	std::vector<uint32_t> res;
	for (uint32_t cp : Utf8CodePoints{str})
		res.push_back(cp);
	return res;
}

constexpr uint32_t RC = Utf8CodePoints::REPLACEMENT_CHARACTER;

} // anonymous namespace

TEST_CASE("Utf8CodePoints", "[StringUtils]")
{
	using namespace cz;

	// NOTE: The literals are byte-escaped, so they don't depend on the source file encoding. Literals are split when the
	// next character is a hex digit (e.g. "\xE3" "b"), otherwise it's parsed as part of the escape.

	SECTION("Valid")
	{
		CHECK(decode("") == std::vector<uint32_t>{});
		CHECK(decode(std::string_view()) == std::vector<uint32_t>{});
		CHECK(decode("abc") == std::vector<uint32_t>{'a', 'b', 'c'});
		CHECK(decode("\xC3\xA3") == std::vector<uint32_t>{0xE3});					// U+00E3 (2 bytes)
		CHECK(decode("\xE2\x82\xAC") == std::vector<uint32_t>{0x20AC});				// U+20AC (3 bytes)
		CHECK(decode("\xF0\x9F\x98\x80") == std::vector<uint32_t>{0x1F600});		// U+1F600 (4 bytes)
		CHECK(decode("a\xC3\xA3" "b") == std::vector<uint32_t>{'a', 0xE3, 'b'});
	}

	SECTION("Truncated sequences")
	{
		CHECK(decode("\xE3") == std::vector<uint32_t>{RC});
		CHECK(decode("\xE3\x81") == std::vector<uint32_t>{RC});
		CHECK(decode("a\xF0\x9F\x98") == std::vector<uint32_t>{'a', RC});
	}

	SECTION("Invalid lead bytes")
	{
		CHECK(decode("\x80") == std::vector<uint32_t>{RC});
		CHECK(decode("\x80\x80\x80") == std::vector<uint32_t>{RC, RC, RC});
		CHECK(decode("\xFF") == std::vector<uint32_t>{RC});
		CHECK(decode("a\xA9" "b") == std::vector<uint32_t>{'a', RC, 'b'});				// Windows-1252 copyright sign
	}

	SECTION("Incomplete sequences")
	{
		CHECK(decode("\xE3" "b") == std::vector<uint32_t>{RC, 'b'});
		CHECK(decode("\xE3\x81" "b") == std::vector<uint32_t>{RC, 'b'});
		// Windows-1252 "ãçé". Each byte is a lead byte, but none is followed by trail bytes.
		CHECK(decode("\xE3\xE7\xE9") == std::vector<uint32_t>{RC, RC, RC});
	}

	SECTION("Overlong sequences and invalid code points")
	{
		CHECK(decode("\xC0\xAF") == std::vector<uint32_t>{RC});						// Overlong '/'
		CHECK(decode("\xED\xA0\x80") == std::vector<uint32_t>{RC});					// Surrogate U+D800
		CHECK(decode("\xF4\x90\x80\x80") == std::vector<uint32_t>{RC});				// U+110000 (above max)
		CHECK(decode("\xC0\xAF" "a") == std::vector<uint32_t>{RC, 'a'});
	}
}

TEST_CASE("widen", "[StringUtils]")
{
	using namespace cz;

	// NOTE: Expected values are built from explicit code units instead of wide literals with \u escapes, since without
	// /utf-8 MSVC doesn't encode those as expected.
	auto ws = [](std::initializer_list<uint32_t> units)
	{
		std::wstring res;
		for (uint32_t unit : units)
			res.push_back(static_cast<wchar_t>(unit));
		return res;
	};

	CHECK(widen("abc") == L"abc");
	CHECK(widen("\xC3\xA3") == ws({0xE3}));
	if constexpr (sizeof(wchar_t) == 2)
		CHECK(widen("\xF0\x9F\x98\x80") == ws({0xD83D, 0xDE00}));		// UTF-16 surrogate pair for U+1F600
	else
		CHECK(widen("\xF0\x9F\x98\x80") == ws({0x1F600}));
	CHECK(widen("a\xE3") == ws({'a', RC}));
	CHECK(widen("\x80" "b") == ws({RC, 'b'}));
}

TEST_CASE("narrow", "[StringUtils]")
{
	using namespace cz;

	auto u16 = [](std::initializer_list<uint32_t> units)
	{
		std::u16string res;
		for (uint32_t unit : units)
			res.push_back(static_cast<char16_t>(unit));
		return res;
	};

	auto u32 = [](std::initializer_list<uint32_t> units)
	{
		std::u32string res;
		for (uint32_t unit : units)
			res.push_back(static_cast<char32_t>(unit));
		return res;
	};

	// UTF-8 for U+FFFD.
	// NOTE: std::string instead of std::string_view, since Catch2 is built without std::string_view support.
	const std::string RCUtf8 = "\xEF\xBF\xBD";

	SECTION("UTF-16")
	{
		CHECK(narrow(u16({})) == "");
		CHECK(narrow(u16({'a', 'b'})) == "ab");
		CHECK(narrow(u16({0xE3})) == "\xC3\xA3");
		CHECK(narrow(u16({0xD83D, 0xDE00})) == "\xF0\x9F\x98\x80");					// Surrogate pair for U+1F600

		// Unpaired surrogates
		CHECK(narrow(u16({0xD83D})) == RCUtf8);											// Lead surrogate at the end
		CHECK(narrow(u16({0xD83D, 'a'})) == std::string(RCUtf8) + "a");					// Lead surrogate not followed by a trail
		CHECK(narrow(u16({0xDE00, 'a'})) == std::string(RCUtf8) + "a");					// Lone trail surrogate
		CHECK(narrow(u16({0xD83D, 0xD83D, 0xDE00})) == std::string(RCUtf8) + "\xF0\x9F\x98\x80");
	}

	SECTION("UTF-32")
	{
		CHECK(narrow(u32({})) == "");
		CHECK(narrow(u32({'a', 0xE3, 0x1F600})) == "a\xC3\xA3\xF0\x9F\x98\x80");
		CHECK(narrow(u32({0xD800})) == RCUtf8);											// Surrogate
		CHECK(narrow(u32({0x110000})) == RCUtf8);										// Above U+10FFFF
		CHECK(narrow(u32({0xFFFFFFFF, 'b'})) == std::string(RCUtf8) + "b");
	}

	SECTION("wstring")
	{
		std::wstring str;
		if constexpr (sizeof(wchar_t) == 2)
			str = {L'a', static_cast<wchar_t>(0xD83D), L'b'};								// Unpaired lead surrogate
		else
			str = {L'a', static_cast<wchar_t>(0xD800), L'b'};								// Surrogate
		CHECK(narrow(str) == "a" + std::string(RCUtf8) + "b");
	}
}

