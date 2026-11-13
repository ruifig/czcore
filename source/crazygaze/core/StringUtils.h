#pragma once

#include <array>
#include "PlatformUtils.h"

namespace cz
{

/**
 * Converts a UTF-16 or UTF-32 string to UTF-8.
 * std::wstring is UTF-16 or UTF-32 depending on the OS (UTF-16 on Windows).
 *
 * Invalid input doesn't fail the conversion. Each invalid code unit is converted to U+FFFD (the replacement character),
 * so the result is always valid UTF-8. Invalid code units are:
 * - UTF-16: Unpaired surrogates (e.g. Windows file names can have those)
 * - UTF-32: Surrogates, and values above U+10FFFF
 */
std::string narrow(std::wstring_view str);
std::string narrow(std::u16string_view str);
std::string narrow(std::u32string_view str);

/**
 * Converts a UTF-8 string to a wide string (OS dependent).
 * std::wstring character width is OS dependent, so this function is handy when interacting with the OS or some of the STL. E.g,
 * opening a file.
 *
 * Invalid input doesn't fail the conversion. Each invalid UTF-8 sequence is converted to U+FFFD (the replacement
 * character), the same way as Utf8CodePoints.
 */
 std::wstring widen(std::string_view str);

/**
 * Using this, so we can use std::string as a key to the map, but not having to allocate a std::string every time we need to do a
 * lookup.
 * Explanation: https://www.cppstories.com/2021/heterogeneous-access-cpp20/ 
 *
 * A map holding values Foo can be declared as:
 * std::unordered_map<std::string, Foo, string_hash, std::equal_to<>> values;
 * 
 */
 struct string_hash
 {
	 using is_transparent = void;
	 [[nodiscard]] size_t operator()(const char* txt) const
	 {
		 return std::hash<std::string_view>{}(txt);
	 }

	 [[nodiscard]] size_t operator()(std::string_view txt) const
	 {
		 return std::hash<std::string_view>{}(txt);
	 }

	 [[nodiscard]] size_t operator()(const std::string& txt) const
	 {
		 return std::hash<std::string>{}(txt);
	 }
 };


/**
  * Returns true if the specified character is a whitespace character
  */
bool whitespaceCharacter(int a);

/**
 * Returns true if the specified character is NOT space tab or line ending
 */
inline bool notWhitespaceCharacter(int ch)
{
	return !whitespaceCharacter(ch);
}

namespace details
{
	template<class CharT>
	constexpr std::basic_string_view<CharT> trim_wsChars()
	{
		static constexpr CharT w[] = {CharT(' '), CharT('\t'), CharT('\n')};
		return {w, 3};
	}

	template<class CharT>
	constexpr std::basic_string_view<CharT> ltrim(std::basic_string_view<CharT> s)
	{
		const auto start = s.find_first_not_of(trim_wsChars<CharT>());
		return start == std::basic_string_view<CharT>::npos ? std::basic_string_view<CharT>{} : s.substr(start);
	}

	template<class CharT>
	constexpr std::basic_string_view<CharT> rtrim(std::basic_string_view<CharT> s)
	{
		const auto start = s.find_last_not_of(trim_wsChars<CharT>());
		return start == std::basic_string_view<CharT>::npos ? std::basic_string_view<CharT>{} : s.substr(0, start + 1);
	}

	template<class CharT>
	constexpr std::basic_string_view<CharT> trim(std::basic_string_view<CharT> s)
	{
		return ltrim(rtrim(s));
	}
}

/**
 * Trim the left side of a string
 */
template<class CharT>
constexpr std::basic_string_view<CharT> ltrim(const CharT* s)
{
	return details::ltrim<CharT>(s);
}

/**
 * Trim the right side of a string
 */
template<class CharT>
constexpr std::basic_string_view<CharT> rtrim(const CharT* s)
{
	return details::rtrim<CharT>(s);
}

/**
 * Trim both sides of a string
 */
template<class CharT>
constexpr std::basic_string_view<CharT> trim(const CharT* s)
{
	return details::trim<CharT>(s);
}

/**
 * Trim the left side of a string
 */
template<class CharT>
constexpr std::basic_string_view<CharT> ltrim(std::basic_string_view<CharT> s)
{
	return details::ltrim<CharT>(s);
}

/**
 * Trim the right side of a string
 */
template<class CharT>
constexpr std::basic_string_view<CharT> rtrim(std::basic_string_view<CharT> s)
{
	return details::rtrim<CharT>(s);
}

/**
 * Trim both sides of a string
 */
template<class CharT>
constexpr std::basic_string_view<CharT> trim(std::basic_string_view<CharT> s)
{
	return details::trim<CharT>(s);
}

/**
 * Trim the left side of a string
 * Note that it returns a new string and NOT a string view, for safety reasons.
 * If you want a string view, use the overload that takes a literal or string_view as parameter.
 */
template<class CharT>
constexpr std::basic_string<CharT> ltrim(const std::basic_string<CharT>& s)
{
	return std::basic_string<CharT>(details::ltrim<CharT>(s));
}

/**
 * Trim the right side of a string
 * Note that it returns a new string and NOT a string view, for safety reasons.
 * If you want a string view, use the overload that takes a literal or string_view as parameter.
 */
template<class CharT>
constexpr std::basic_string<CharT> rtrim(const std::basic_string<CharT>& s)
{
	return std::basic_string<CharT>(details::rtrim<CharT>(s));
}

/**
 * Trim both sides of a string
 * Note that it returns a new string and NOT a string view, for safety reasons.
 * If you want a string view, use the overload that takes a literal or string_view as parameter.
 */
template<class CharT>
constexpr std::basic_string<CharT> trim(const std::basic_string<CharT>& s)
{
	return std::basic_string<CharT>(details::trim<CharT>(s));
}

/**
 * Compares two ASCII strings, ignoring case
 * Only the characters 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' are converted for comparison.
 */
bool asciiStrEqualsCi(std::string_view str1, std::string_view str2);

/**
 * Converts a string to lowercase.
 * Only ASCII characters are converted
 */
[[nodiscard]] std::string asciiToLower(std::string_view str);

/**
 * Converts the specified span into lowercase, in-place.
 * Only ASCII characters are converted
 */
void asciiToLowerInPlace(std::span<char> str);

/*!
 * Given a string_view as input, it replaces all occurences of `from` with `to`
 */
std::string replace(std::string_view input, std::string_view from, std::string_view to);

/*!
 * Given a string_view as input, it performs multiple replacements as specified in the replacements list
 */
std::string replace(std::string_view input, std::initializer_list<std::pair<std::string_view, std::string_view>>&& replacements);

enum class EOL
{
	Windows,
	Linux,
};

/*!
 * Changes the EOLs of the specified string
 */
std::string changeEOL(std::string_view str, EOL eol);

/**
 * Splits a string into lines and puts them into a vector
 * This allocates memory for each line, so consider using `StringLineSplit` instead.
 */
std::vector<std::string> stringSplitIntoLinesVector(std::string_view text, bool dropEmptyLines = true);

/**
 * Checks if string `what` is found in string `where`, in a case sensitive way.
 * NOTE: Only ASCII characters are considered for case insensivity.
 */
bool containsCi(const std::string_view where, const std::string_view what);

/*
 * Utility that given a std::string_view, it allows iterating through lines
 *
 * E.g:
 *
 *	for(auto l : StringLineSplit(mystr))
 *	{
 *		CZ_LOG(Main, Log, "{}", l);
 *	}
 *
 * NOTE: The constructor allows specifying if empty lines should be reported, or skipped
 * 
 */
class StringLineSplit
{
  public:
	class Iterator
	{
	  public:
		using value_type = std::string_view;
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::input_iterator_tag;
		using pointer = const std::string_view*;
		using reference = const std::string_view&;

		Iterator() = default;

		Iterator(std::string_view str, size_t pos, bool dropEmptyLines)
			: m_str(str)
			, m_pos(pos)
			, m_dropEmptyLines(dropEmptyLines)
		{
			advance();
		}

		reference operator*() const
		{
			return m_current;
		}
		pointer operator->() const
		{
			return &m_current;
		}

		Iterator& operator++()
		{
			advance();
			return *this;
		}

		Iterator operator++(int)
		{
			Iterator tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const Iterator& other) const
		{
			return m_pos == other.m_pos && m_str.data() == other.m_str.data();
		}

		bool operator!=(const Iterator& other) const
		{
			return !(*this == other);
		}

	  private:

		void advance()
		{
			advanceImpl();
			while (m_pos != std::string_view::npos && m_dropEmptyLines && m_current.size() == 0)
			{
				advanceImpl();
			}
		}
		
		void advanceImpl();

		std::string_view m_str;
		size_t m_pos = 0;
		std::string_view m_current;
		bool m_dropEmptyLines;
	};

	explicit StringLineSplit(std::string_view str, bool dropEmptyLines = true)
		: m_str(str)
		, m_dropEmptyLines(dropEmptyLines)
	{
	}

	Iterator begin() const
	{
		return Iterator(m_str, 0, m_dropEmptyLines);
	}
	Iterator end() const
	{
		return Iterator(m_str, std::string_view::npos, m_dropEmptyLines);
	}

  private:
	std::string_view m_str;
	bool m_dropEmptyLines;
};

/*!
 * Utility that given a std::string_view and a delimiter, it allows iterating through tokens
 *
 * E.g:
 *
 *	for(auto token : StringSplit(mystr, ','))
 *	{
 *		CZ_LOG(Main, Log, "{}", token);
 *	}
 */
class StringSplit
{
  public:
	class Iterator
	{
	  public:
		using value_type = std::string_view;
		using difference_type = std::ptrdiff_t;
		using pointer = const std::string_view*;
		using reference = const std::string_view&;
		using iterator_category = std::input_iterator_tag;

		Iterator(std::string_view str, char delim, size_t pos, bool skipEmpty)
			: m_str(str)
			, m_delim(delim)
			, m_pos(pos)
			, m_skipEmpty(skipEmpty)
		{
			advance();
		}

		std::string_view operator*() const noexcept
		{
			return m_current;
		}

		Iterator& operator++() noexcept
		{
			advance();
			return *this;
		}

		bool operator==(const Iterator& other) const noexcept
		{
			return m_pos == other.m_pos && m_done == other.m_done;
		}

	  private:
		void advanceImpl() noexcept
		{
			if (m_pos == std::string_view::npos)
			{
				m_current = {};
				m_done = true;
				return;
			}

			size_t next = m_str.find(m_delim, m_pos);
			if (next == std::string_view::npos)
			{
				m_current = m_str.substr(m_pos);
				m_pos = std::string_view::npos;
			}
			else
			{
				m_current = m_str.substr(m_pos, next - m_pos);
				m_pos = next + 1;
			}
		}

		void advance() noexcept
		{
			advanceImpl();
			while(!m_done && m_skipEmpty && m_current.size() == 0)
				advanceImpl();
		}

		std::string_view m_str;
		char m_delim;
		size_t m_pos;
		bool m_done = false;
		bool m_skipEmpty = false;
		std::string_view m_current;
	};

	StringSplit(std::string_view str, char delim, bool skipEmpty = false)
		: m_str(str)
		, m_delim(delim)
		, m_skipEmpty(skipEmpty)
	{
	}

	Iterator begin() const noexcept
	{
		return Iterator(m_str, m_delim, 0, m_skipEmpty);
	}

	Iterator end() const noexcept
	{
		return Iterator(m_str, m_delim, std::string_view::npos, m_skipEmpty);
	}

  private:
	std::string_view m_str;
	char m_delim;
	bool m_skipEmpty;
};


/*!
 * Calls visitor(key, value) for each entry like "key=value" separated by commas.
 * Invalid/empty entries are skipped; entries without '=' yield empty value.
 */
template <class Visitor>
void visitKeyValues(std::string_view input, Visitor&& visitor, char pairSep = ',', char kvSep = '=')
{
	while (!input.empty())
	{
		// Take next segment up to comma
		std::size_t cut = input.find(pairSep);
		std::string_view seg = (cut == std::string_view::npos) ? input : input.substr(0, cut);
		input = (cut == std::string_view::npos) ? std::string_view{} : input.substr(cut + 1);

		seg = trim(seg);
		if (seg.empty())
		{
			continue;
		}

		// Split on first '='
		std::size_t eq = seg.find(kvSep);
		std::string_view key = trim(eq == std::string_view::npos ? seg : seg.substr(0, eq));
		if (key.empty())
		{
			continue;  // no key, skip
		}

		std::string_view value = (eq == std::string_view::npos) ? std::string_view{} : trim(seg.substr(eq + 1));
		visitor(key, value);
	}
}


/*!
 * Overloads to convert from string
 */
template<typename T>
requires std::is_floating_point_v<T>
bool fromString(std::string_view str, T& dst)
{
	return std::from_chars(str.data(), str.data() + str.length(), dst).ec == std::errc();
}

/**
 * Converts a string to some integral T
 *
 * If `base` is left at 0, it will look for a prefix such as `0x` and use base 16, or 10 if the prefix doesn't exist.
 * If base is specified as e.g `16`, it will interpret the string as that.
 */
template<typename T>
requires std::is_integral_v<T>
bool fromString(std::string_view str, T& dst, int base = 0)
{
	if constexpr (std::is_same_v<T, bool>)
	{
		if (str == "0" || asciiStrEqualsCi(str, "false"))
		{
			dst = false;
			return true;
		}
		else if (str == "1" || asciiStrEqualsCi(str, "true"))
		{
			dst = true;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (base == 0)
		{
			if (str.size()>2 && (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')))
			{
				str = std::string_view(str.begin() + 2, str.end());
				base = 16;
			}
			else
			{
				base = 10;
			}
		}

		return std::from_chars(str.data(), str.data() + str.length(), dst, base).ec == std::errc();
	}
}

inline bool fromString(std::string_view str, std::string& dst)
{
	dst = str;
	return true;
}

inline bool fromString(std::string_view str, fs::path& dst)
{
	dst = str;
	return true;
}

template<typename T>
	requires std::is_integral_v<T>
std::string toString(const T& val)
{
	return std::format("{}", val);
}

/**
 * Converts an integral T to an hexadecimal string with leading zeros
 */
template<typename T>
	requires std::is_integral_v<T>
std::string toHexString(T val)
{
	constexpr std::size_t width = sizeof(T) * 2;
	return std::format("{:0{}X}", val, width);
}

/*!
 * Parses a delimited string into an array of values.
 * E.g: "10,20,30" into an array of 3 integers.
 * - T must have a `fromString(std::string_view, T&)` overload.
 * - Returns true on success, false on failure (e.g invalid format, wrong number of elements, etc)
 * - delim can't occur inside the values.
 */
template<typename T>
bool fromDelimitedString(std::string_view str, T* dst, int count, char delim = ',')
{
	int idx = 0;
	for(auto token : StringSplit(str, delim))
	{
		token = trim(token);
		if (idx>=count || !fromString(token, dst[idx]))
			return false;

		++idx;
	}

	return idx == count ? true : false;
}


/**
 * Calculates the levenshtein distance between two strings.
 */
int levenshtein_distance(std::string_view s1, std::string_view s2);

} // namespace cz


/*!
 * Formatter for fs::path , so it can be used with the std::format functions.
 * At the time of writing, no compiler implements this yet.
 * See https://en.cppreference.com/w/cpp/filesystem/path/formatter
 */
template<>
struct std::formatter<cz::fs::path> : std::formatter<string_view>
{
	auto format(const cz::fs::path& p, std::format_context& ctx) const
	{
		return std::formatter<std::string_view>::format(cz::narrow(p.native()), ctx);
	}
};

inline std::string toString(const cz::fs::path& p)
{
	return cz::narrow(p.native());
}

/**
 * This might look silly, but to have a true generic `toString` that can be used in any code
 * regardless of T, toString needs to accept a string too.
 */
inline const std::string& toString(const std::string& v)
{
	return v;
}

/**
 * Overload so that doing something like `toString(someFunc())` works, because if `someFunc` returns e.g `std::string`, then
 * we need to keep the string alive.
 */
inline std::string toString(std::string&& v)
{
	return v;
}

/**
 * Overload to avoid any heap allocation or copying
 */
inline std::string_view toString(std::string_view v)
{
	return v;
}

/*!
 * Given a codepoint, returns its UTF8 representation as a char array.
 * It doesn't check if the codepoint is valid.
 */
inline std::array<char, 5> codepointToUtf8(uint32_t cp)
{
	std::array<char, 5> result = {};
	utf8::unchecked::append(cp, result.begin());
	return result;
}


/**
 * Utility that allows iterating through the code points of a UTF8 string easily.
 * Use as:
 * 
 * ```
 * for(uint32_t cp : Utf8CodePoints(myUtf8String))
 * {
 *     // process code point
 * }
 * ```
 *
 * Invalid UTF8 doesn't break the iteration: Each invalid sequence yields one REPLACEMENT_CHARACTER (U+FFFD), the same way
 * `utf8::replace_invalid` handles it. This means it's safe to use with text from untrusted sources (e.g. data files, user
 * input or the network).
 * If you want to check if a string is valid UTF8, use `utf8::is_valid()`.
 *
 */
struct Utf8CodePoints
{
	static constexpr uint32_t REPLACEMENT_CHARACTER = 0xFFFD;

	std::string_view text;

	class Iterator
	{
	  public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = uint32_t;
		using difference_type = std::ptrdiff_t;
		using pointer = const uint32_t*;
		using reference = uint32_t;

		Iterator() = default;

		Iterator(const char* pos, const char* end)
			: m_pos(pos)
			, m_end(end)
		{
			decode();
		}

		uint32_t operator*() const
		{
			return m_cp;
		}

		Iterator& operator++()
		{
			m_pos = m_next;
			decode();
			return *this;
		}

		Iterator operator++(int)
		{
			Iterator tmp = *this;
			++*this;
			return tmp;
		}

		bool operator==(const Iterator& other) const
		{
			return m_pos == other.m_pos;
		}

	  private:
		// Decodes the code point at m_pos, and finds where the next one starts
		void decode()
		{
			if (m_pos == m_end)
			{
				m_next = m_end;
				return;
			}

			m_next = m_pos;
			utf8::utfchar32_t cp = 0;
			switch (utf8::internal::validate_next(m_next, m_end, cp))
			{
			case utf8::internal::UTF8_OK:
				m_cp = cp;
				return;

			case utf8::internal::NOT_ENOUGH_ROOM:
				// Truncated sequence at the end of the string
				m_next = m_end;
				break;

			case utf8::internal::INVALID_LEAD:
				// Stray byte (e.g. a lone trail byte). Skip just that byte.
				m_next = m_pos + 1;
				break;

			case utf8::internal::INCOMPLETE_SEQUENCE:
			case utf8::internal::OVERLONG_SEQUENCE:
			case utf8::internal::INVALID_CODE_POINT:
			default:
				// Skip the lead byte, and any trail bytes that follow it
				m_next = m_pos + 1;
				while (m_next != m_end && utf8::internal::is_trail(*m_next))
					++m_next;
				break;
			}

			m_cp = REPLACEMENT_CHARACTER;
		}

		const char* m_pos = nullptr;
		const char* m_next = nullptr;
		const char* m_end = nullptr;
		uint32_t m_cp = 0;
	};

	Iterator begin() const
	{
		return Iterator(text.data(), text.data() + text.size());
	}

	Iterator end() const
	{
		return Iterator(text.data() + text.size(), text.data() + text.size());
	}
};

/**
 * Given a type a T, it defines a std::formatter so that type can be directly used in std::format.
 * A `toString` function that returns a string like result (e.g std::string, std::string_view, const char*) is expected to exist.
 */
#define CZ_DEFINE_FORMATTER_FROM_TOSTRING(T)                     \
	template <>                                                  \
	struct std::formatter<T> : std::formatter<string_view>       \
	{                                                            \
		auto format(const T& v, std::format_context& ctx) const  \
		{                                                        \
			return std::format_to(ctx.out(), "{}", toString(v)); \
		}                                                        \
	};

