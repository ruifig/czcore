/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com

	purpose:

*********************************************************************/

#pragma once

#include "Common.h"

namespace cz
{

/**
 * Creates a 128-bit UUID (Universally unique identifier)
 */
class UUID
{
public:

	/**
	 * Controls what string format to use.
	 * See https://learn.microsoft.com/en-us/dotnet/api/system.guid.tostring?view=net-10.0
	 */
	enum FormatType
	{
		// 32 digits: 
		// 00000000000000000000000000000000
		N,
		// 32 (8+4+4+12) digits separated by hyphens:
		// 00000000-0000-0000-0000-000000000000
		D,
		// 32 digits separated by hyphens, enclosed in braces
		// {00000000-0000-0000-0000-000000000000}
		B,
		// 32 digits separated by hyphens, enclosed in parentheses
		// (00000000-0000-0000-0000-000000000000)
		P
	};

	UUID() = default;
	UUID(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
		: values{a,b,c,d}
	{
	}

	static UUID create();

	bool operator==(const UUID& other) const
	{
		return values64[0] == other.values64[0] && values64[1] == other.values64[1];
	}

	bool operator!=(const UUID& other) const
	{
		return !(operator==(other));
	}

	bool isValid() const
	{
		return (values64[0] | values64[1]) != 0;
	}

	// So it can be used as a key in maps
	bool operator<(const UUID& other) const;

	union
	{
		struct
		{
			uint32_t a;
			uint32_t b;
			uint32_t c;
			uint32_t d;
		} values;

		uint8_t raw[16];
		uint64_t values64[2] = {0,0}; // These are used to make comparisons.
	};
};

bool fromString(std::string_view str, UUID& dst);
std::string toString(const UUID& v, UUID::FormatType formatType = UUID::FormatType::N);

} // namespace cz


// Define std::hash<UUID>, so it can be used as a key for std::unordered_map
namespace std
{
	template<>
	struct hash<cz::UUID>
	{
		std::size_t operator()(const cz::UUID& guid) const
		{
			using guidbitset = std::bitset<128>;
			static_assert(sizeof(guidbitset) == sizeof(cz::UUID), "bitset doesn't match UUID size");
			const guidbitset& bits = *reinterpret_cast<const guidbitset*>(&guid);
			return std::hash<guidbitset>()(bits);
		}
	};
}

template<>
struct std::formatter<cz::UUID> : public std::formatter<std::string_view>
{
	auto format(const cz::UUID& v, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "{}", toString(v));
	}
};
