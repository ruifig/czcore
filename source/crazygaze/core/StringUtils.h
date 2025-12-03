#pragma once

namespace cz
{

/**
 * Convert a utf16/32 to utf8
 */
std::string narrow(std::wstring_view str);
std::string narrow(std::u16string_view str);
std::string narrow(std::u32string_view str);

/**
 * Converts a utf8 string to a wide string (OS dependent).
 * std::wstring character width is OS dependent, so this fucntion is handy when interacting with the OS or some of the STL. E.g,
 * opening a file.
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

		Iterator(std::string_view str, char delim, size_t pos)
			: m_str(str)
			, m_delim(delim)
			, m_pos(pos)
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
		void advance() noexcept
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

		std::string_view m_str;
		char m_delim;
		size_t m_pos;
		bool m_done = false;
		std::string_view m_current;
	};

	StringSplit(std::string_view str, char delim)
		: m_str(str)
		, m_delim(delim)
	{
	}

	Iterator begin() const noexcept
	{
		return Iterator(m_str, m_delim, 0);
	}

	Iterator end() const noexcept
	{
		return Iterator(m_str, m_delim, std::string_view::npos);
	}

  private:
	std::string_view m_str;
	char m_delim;
};


/*!
 * Calls visitor(key, value) for each entry like "key=value" separated by commas.
 * Invalid/empty entries are skipped; entries without '=' yield empty value.
 */
template <class Visitor>
static void visitKeyValues(std::string_view input, Visitor&& visitor, char pairSep = ',', char kvSep = '=')
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

template<typename T>
requires std::is_integral_v<T>
bool fromString(std::string_view str, T& dst)
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
		return std::from_chars(str.data(), str.data() + str.length(), dst).ec == std::errc();
	}
}

inline bool fromString(std::string_view str, std::string& dst)
{
	dst = str;
	return true;
}

inline bool fromString(std::string_view str, std::filesystem::path& dst)
{
	dst = str;
	return true;
}


/*!
 * Convert any type to string.
 * The type must support std::format
 */
template<typename T>
std::string toString(const T& val)
{
	return std::format("{}", val);
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

} // namespace cz


/*!
 * Formatter for std::filesystem::path , so it can be used with the std::format functions.
 * At the time of writing, no compiler implements this yet.
 * See https://en.cppreference.com/w/cpp/filesystem/path/formatter
 */
template<>
struct std::formatter<std::filesystem::path> : std::formatter<string_view>
{
	auto format(const std::filesystem::path& p, std::format_context& ctx) const
	{
		return std::formatter<std::string_view>::format(cz::narrow(p.native()), ctx);
	}
};


