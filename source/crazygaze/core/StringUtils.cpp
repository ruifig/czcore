#include "StringUtils.h"

namespace cz
{

namespace
{

constexpr uint32_t REPLACEMENT_CHARACTER = 0xFFFD;

// unchecked::utf16to8 can crash if given invalid input. See https://github.com/nemtrif/utfcpp/issues/78
// The author wants to leave it as-is, since it's in the "unckecked" namespace, so we have this function copied and adapted.
// Unpaired surrogates are converted to REPLACEMENT_CHARACTER, so the result is always valid UTF-8.
template <typename u16bit_iterator, typename octet_iterator>
octet_iterator utf16to8_lenient (u16bit_iterator start, u16bit_iterator end, octet_iterator result)
{
	while (start != end)
	{
		uint32_t cp = utf8::internal::mask16(*start++);
		if (utf8::internal::is_lead_surrogate(cp))
		{
			if (start != end && utf8::internal::is_trail_surrogate(utf8::internal::mask16(*start)))
				cp = (cp << 10) + utf8::internal::mask16(*start++) + utf8::internal::SURROGATE_OFFSET;
			else
				cp = REPLACEMENT_CHARACTER;	// Unpaired lead surrogate. The next unit is not consumed, since it's not part of the pair
		}
		else if (utf8::internal::is_trail_surrogate(cp))
		{
			cp = REPLACEMENT_CHARACTER;	// Lone trail surrogate
		}

		result = utf8::unchecked::append(cp, result);
	}
	return result;
}

// Same as unchecked::utf32to8, but invalid code points (surrogates, and values above U+10FFFF) are converted to
// REPLACEMENT_CHARACTER, so the result is always valid UTF-8.
template <typename u32bit_iterator, typename octet_iterator>
octet_iterator utf32to8_lenient(u32bit_iterator start, u32bit_iterator end, octet_iterator result)
{
	while (start != end)
	{
		uint32_t cp = static_cast<uint32_t>(*start++);
		if (!utf8::internal::is_code_point_valid(cp))
			cp = REPLACEMENT_CHARACTER;

		result = utf8::unchecked::append(cp, result);
	}
	return result;
}


} // unnamed namespace

std::wstring widen(std::string_view str)
{
	std::wstring result;
	result.reserve(str.size());

	// NOTE: Decoding with Utf8CodePoints, since the utfcpp unchecked conversions read out of bounds on invalid UTF8
	for (uint32_t cp : Utf8CodePoints{str})
	{
		if constexpr(sizeof(std::wstring::value_type)==2)
			utf8::unchecked::append16(cp, std::back_inserter(result));
		else
			result.push_back(static_cast<wchar_t>(cp));
	}
	return result;
}

std::string narrow(std::wstring_view str)
{
	std::string result;
	if constexpr(sizeof(std::wstring::value_type)==2)
	{
		utf16to8_lenient(str.begin(), str.end(), std::back_inserter(result));
	}
	else
	{
		utf32to8_lenient(str.begin(), str.end(), std::back_inserter(result));
	}
	return result;
}

std::string narrow(std::u16string_view str)
{
	std::string result;
	utf16to8_lenient(str.begin(), str.end(), std::back_inserter(result));
	return result;
}

std::string narrow(std::u32string_view str)
{
	std::string result;
	utf32to8_lenient(str.begin(), str.end(), std::back_inserter(result));
	return result;
}

namespace
{

inline char asciiCharToLower(char ch)
{
	return (ch >= 'A' && ch <= 'Z') ? (ch + ('a' - 'A')) : ch;
};

}

bool asciiStrEqualsCi(std::string_view str1, std::string_view str2)
{
	if (str1.size() != str2.size())
	{
		return false;
	}

	std::string_view::const_iterator it1 = str1.begin();
	std::string_view::const_iterator it2 = str2.begin();
	while (it1 != str1.end())
	{
		if (asciiCharToLower(*it1) != asciiCharToLower(*it2))
		{
			return false;
		}

		++it1;
		++it2;
	}

	return true;
}

std::string asciiToLower(std::string_view str)
{
	std::string res(str);
	asciiToLowerInPlace(res);
	return res;
}

void asciiToLowerInPlace(std::span<char> str)
{
	for(auto&& ch : str)
	{
		ch = asciiCharToLower(ch);
	}
}

std::string replace(std::string_view input, std::string_view from, std::string_view to)
{
	std::string result;
	result.reserve(input.size()*2);

	size_t pos = 0;
	while(true)
	{
		size_t idx = input.find(from, pos);

		if (idx == std::string_view::npos)
		{
			// No more occurrences. Append the rest and finish
			result.append(input.substr(pos));
			break;
		}

		// append up to the match
		result.append(input.substr(pos, idx - pos));
		// append replacement
		result.append(to);
		// advance past the match
		pos = idx + from.size();
	}

	return result;
}

std::string replace(std::string_view input, std::initializer_list<std::pair<std::string_view, std::string_view>>&& replacements)
{
	std::string result;
	for(auto&& p : replacements )
	{
		if (result.size() == 0)
			result = replace(input, p.first, p.second);
		else
			result = replace(result, p.first, p.second);
	}

	return result;
}

std::string changeEOL(std::string_view str, EOL eol)
{
	std::string res;
	for(auto l : StringLineSplit(str, false))
	{
		res += l;
		res += eol==EOL::Windows ? "\x0D\x0A" : "\0A";
	}

	return res;
}

std::vector<std::string> stringSplitIntoLinesVector(std::string_view text, bool dropEmptyLines)
{
	std::vector<std::string> lines;
	for(auto l : StringLineSplit(text, dropEmptyLines))
	{
		lines.emplace_back(l);
	}
	return lines;
}

bool containsCi(const std::string_view where, const std::string_view what)
{
	const auto equalsIgnoreCase = [](char a, char b)
	{
		return asciiCharToLower(a) == asciiCharToLower(b);
	};

	return std::search(where.begin(), where.end(), what.begin(), what.end(), equalsIgnoreCase) != where.end();
}

bool whitespaceCharacter(int ch)
{
// See https://www.asciitable.com/
    return
		ch==' '  || // space
		ch=='\t' || // tab
		ch==0xA  || // line feed
		ch==0xB  || // vertical tab
		ch==0xC  || // form feed
		ch==0xD;    // carriage return
}

void StringLineSplit::Iterator::advanceImpl()
{
	if (m_pos >= m_str.size())
	{
		m_pos = std::string_view::npos;
		m_current = {};
		return;
	}

	size_t start = m_pos;
	size_t len = 0;

	// Find next line ending
	while (m_pos < m_str.size())
	{
		char c = m_str[m_pos];
		if (c == '\n' || c == '\r')
		{
			break;
		}
		++m_pos;
	}

	len = m_pos - start;
	m_current = m_str.substr(start, len);

	// Handle EOLs: \n, \r, \r\n
	if (m_pos < m_str.size())
	{
		if (m_str[m_pos] == '\r')
		{
			++m_pos;
			if (m_pos < m_str.size() && m_str[m_pos] == '\n')
			{
				++m_pos;
			}
		}
		else if (m_str[m_pos] == '\n')
		{
			++m_pos;
		}
	}
}


// From https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C.2B.2B
int levenshtein_distance(std::string_view s1, std::string_view s2)
{
	// To change the type this function manipulates and returns, change
	// the return type and the types of the two variables below.
	int s1len = static_cast<int>(s1.size());
	int s2len = static_cast<int>(s2.size());

	auto column_start = (decltype(s1len))1;

	auto column = new decltype(s1len)[static_cast<size_t>(s1len + 1)];
	std::iota(column + column_start, column + s1len + 1, column_start);

	for (auto x = column_start; x <= s2len; x++)
	{
		column[0] = x;
		auto last_diagonal = x - column_start;
		for (auto y = column_start; y <= s1len; y++)
		{
			auto old_diagonal = column[y];
			auto possibilities = {column[y] + 1, column[y - 1] + 1, last_diagonal + (s1[static_cast<size_t>(y - 1)] == s2[static_cast<size_t>(x - 1)] ? 0 : 1)};
			column[y] = std::min(possibilities);
			last_diagonal = old_diagonal;
		}
	}
	auto result = column[s1len];
	delete[] column;
	return result;
}

} // namespace cz


