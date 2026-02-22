#include "StringUtils.h"

namespace cz
{

namespace
{

// As far as I've noticed, utfcpp doesn't have a ready to use function to validate a UTF16 string,
// so this based on the utf8::utf16to8 function. Instead of throwing we return a boolean.
template <typename u16bit_iterator>
bool isValidUtf16_Impl(u16bit_iterator start, u16bit_iterator end)
{
	while (start != end) {
		uint32_t cp = utf8::internal::mask16(*start++);
		// Take care of surrogate pairs first
		if (utf8::internal::is_lead_surrogate(cp)) {
			if (start != end) {
				uint32_t trail_surrogate = utf8::internal::mask16(*start++);
				if (utf8::internal::is_trail_surrogate(trail_surrogate))
					cp = (cp << 10) + trail_surrogate + utf8::internal::SURROGATE_OFFSET;
				else
					return false; // throw invalid_utf16(static_cast<uint16_t>(trail_surrogate));
			}
			else
				return false; // throw invalid_utf16(static_cast<uint16_t>(cp));

		}
		// Lone trail surrogate
		else if (utf8::internal::is_trail_surrogate(cp))
			return false; // throw invalid_utf16(static_cast<uint16_t>(cp));
	}

	return true;
}

// unchecked::utf16to8 can crash if given invalid input. See https://github.com/nemtrif/utfcpp/issues/78
// The author wants to leave it as-is, since it's in the "unckecked" namespace, so we have this function copied and adapted.
template <typename u16bit_iterator, typename octet_iterator>
octet_iterator utf16to8_lenient (u16bit_iterator start, u16bit_iterator end, octet_iterator result)
{
	while (start != end) {
		uint32_t cp = utf8::internal::mask16(*start++);
		// Take care of surrogate pairs first
		if (utf8::internal::is_lead_surrogate(cp)) {
			if (start >= end) // #RVF : This is the change compared to the original code
				return result;
			uint32_t trail_surrogate = utf8::internal::mask16(*start++);
			cp = (cp << 10) + trail_surrogate + utf8::internal::SURROGATE_OFFSET;
		}
		result = utf8::unchecked::append(cp, result);
	}
	return result;
}


} // unnamed namespace

std::wstring widen(std::string_view str)
{
	std::wstring result;
	if constexpr(sizeof(std::wstring::value_type)==2)
	{
		utf8::unchecked::utf8to16(str.begin(), str.end(), std::back_inserter(result));
	}
	else
	{
		utf8::unchecked::utf8to32(str.begin(), str.end(), std::back_inserter(result));
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
		utf8::unchecked::utf32to8(str.begin(), str.end(), std::back_inserter(result));
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
	utf8::unchecked::utf32to8(str.begin(), str.end(), std::back_inserter(result));
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


