/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
	
*********************************************************************/

#include "UUID.h"
#include "Logging.h"
#include "Objbase.h"

namespace cz
{

UUID UUID::create()
{
	static_assert(sizeof(UUID) == 4*4);

	static_assert(sizeof(GUID)==sizeof(UUID), "Windows GUID and UUID sizes don't match");
	UUID guid(0,0,0,0);
	CZ_CHECK(CoCreateGuid((GUID*)&guid) == S_OK );
	return guid;
}

bool fromString(std::string_view str, UUID& dst)
{
	auto hexToU32 = [&str](size_t offset, size_t len, uint32_t& dst) -> bool
	{
		return std::from_chars(str.data() + offset, str.data() + offset + len, dst, 16).ec == std::errc();
	};

	uint32_t a, b, c, d;

	auto parseDBP = [&a, &b, &c, &d, &hexToU32]() -> bool
	{
		// 000000000011111111112222222222333333 \
		// 012345678901234567890123456789012345  |-> index
		// 00000000-0000-0000-0000-000000000000

		// Read the upper parts of 'b' and 'c'
		uint32_t b1, c1;
		if (!hexToU32(9, 4, b1) || !hexToU32(19, 4, c1))
			return false;

		uint32_t b2, c2;
		if (!hexToU32(14, 4, b2) || !hexToU32(24, 4, c2))
			return false;

		b = b1 << 16 | b2;
		c = c1 << 16 | c2;

		return hexToU32( 0, 8, a) && hexToU32(28, 8, d);
	};
	
	switch(str.length())
	{
		case 32: // formatType N
			if (!hexToU32( 0, 8, a) ||
				!hexToU32( 8, 8, b) ||
				!hexToU32(16, 8, c) ||
				!hexToU32(24, 8, d))
			{
				return false;
			}
			break;
		case 36: // formatType D
		{
			if (!parseDBP())
				return false;
			break;
		}
		case 38: // formatType B and P
		{
			// If we just skip the first character (the `(` or `{`) then we can parse as if it is of type D
			str = std::string_view(str.begin()+1, str.end());
			if (!parseDBP())
				return false;
			break;
		}

		default:
			return false;
	};

	dst = UUID(a,b,c,d);
	return true;
}

std::string toString(const UUID& v, UUID::FormatType formatType)
{
	switch(formatType)
	{
		case UUID::FormatType::N:
			return std::format("{:08X}{:08X}{:08X}{:08X}", v.values.a, v.values.b, v.values.c, v.values.d);
		case UUID::FormatType::D:
			return std::format("{:08X}-{:04X}-{:04X}-{:04X}-{:04X}{:08X}", v.values.a, v.values.b >> 16, v.values.b & 0xFFFF, v.values.c >> 16, v.values.c & 0xFFFF, v.values.d);
		case UUID::FormatType::B:
			return std::format("{{{:08X}-{:04X}-{:04X}-{:04X}-{:04X}{:08X}}}", v.values.a, v.values.b >> 16, v.values.b & 0xFFFF, v.values.c >> 16, v.values.c & 0xFFFF, v.values.d);

		case UUID::FormatType::P:
		default:
			return std::format("({:08X}-{:04X}-{:04X}-{:04X}-{:04X}{:08X})", v.values.a, v.values.b >> 16, v.values.b & 0xFFFF, v.values.c >> 16, v.values.c & 0xFFFF, v.values.d);
	};
}

bool UUID::operator<(const UUID& other) const
{
	if (values.a != other.values.a)
		return values.a < other.values.a;

	if (values.b != other.values.b)
		return values.b < other.values.b;

	if (values.c != other.values.c)
		return values.c < other.values.c;

	return values.d < other.values.d;
}

} // namespace cz

