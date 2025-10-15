// Based on https://gist.github.com/filsinger/1255697 , with some changes to work with VS 2015 CTP 6
#pragma once

#include "Common.h"

//
// Compile time hashing
// C++11 32bit FNV-1 and FNV-1a string hashing (Fowler–Noll–Vo hash)
//

namespace cz
{


namespace hash
{

	template <typename S> struct fnv_internal;
	template <typename S> struct fnv1a;
 
	template <> struct fnv_internal<uint32_t>
	{
		constexpr static uint32_t default_offset_basis = 0x811C9DC5;
		constexpr static uint32_t prime				   = 0x01000193;
	};

	template <> struct fnv_internal<uint64_t>
	{
		constexpr static uint64_t default_offset_basis = 0xCBF29CE484222325;
		constexpr static uint64_t prime				   = 0x100000001B3;
	};
 
	template <> struct fnv1a<uint32_t> : public fnv_internal<uint32_t>
	{
		constexpr static inline uint32_t hash(char const* const str, const uint32_t val = default_offset_basis) noexcept
		{
			return (str[0] == '\0') ? val : hash(&str[1], (val ^ uint32_t((uint8_t)str[0])) * prime);
		}

		constexpr static inline uint32_t hash(char const* const str, const size_t len, const uint32_t val)
		{
			return (len == 0) ? val : hash( str + 1, len - 1, (val ^ uint32_t((uint8_t)str[0])) * prime);
		}
	};

	template <> struct fnv1a<uint64_t> : public fnv_internal<uint64_t>
	{
		constexpr static inline uint64_t hash(char const* const str, const uint64_t val = default_offset_basis) noexcept
		{
			return (str[0] == '\0') ? val : hash(&str[1], (val ^ uint64_t((uint8_t)str[0])) * prime);
		}

		constexpr static inline uint64_t hash(char const* const str, const size_t len, const uint64_t val)
		{
			return (len == 0) ? val : hash( str + 1, len - 1, (val ^ uint64_t((uint8_t)str[0])) * prime);
		}
	};

	inline constexpr uint32_t operator "" _fnv1a_32 (const char* aString, const size_t aStrlen)
	{
		using hash_type = hash::fnv1a<uint32_t>;
		return hash_type::hash(aString, aStrlen, hash_type::default_offset_basis);
	}

	inline constexpr uint64_t operator "" _fnv1a_64 (const char* aString, const size_t aStrlen)
	{
		using hash_type = hash::fnv1a<uint64_t>;
		return hash_type::hash(aString, aStrlen, hash_type::default_offset_basis);
	}

} // namespace hash
 

//
// Runtime only hashing
// From http://www.isthe.com/chongo/src/fnv/hash_64a.c and http://www.isthe.com/chongo/src/fnv/hash_32a.c
struct Hash
{
	static constexpr uint32_t FNV1_32A_INIT = 0x811c9dc5;
	static constexpr uint32_t FNV_32_PRIME = 0x01000193;

	/*
	 * fnv_32a_buf - perform a 32 bit Fowler/Noll/Vo FNV-1a hash on a buffer
	 *
	 * input:
	 *	buf	- start of buffer to hash
	 *	len	- length of buffer in octets
	 *	hval	- previous hash value or 0 if first call
	 *
	 * returns:
	 *	32 bit hash as a static hash type
	 *
	 * NOTE: To use the recommended 32 bit FNV-1a hash, use FNV1_32A_INIT as the
	 * 	 hval arg on the first call to either fnv_32a_buf() or fnv_32a_str().
	 */

	static uint32_t fnv_32a_buf(const void* buf, size_t len, uint32_t hval = FNV1_32A_INIT)
	{
		const unsigned char *bp = (const unsigned char *)buf; /* start of buffer */
		const unsigned char *be = bp + len;             /* beyond end of buffer */

		/*
		 * FNV-1a hash each octet in the buffer
		 */
		while (bp < be)
		{
			/* xor the bottom with the current octet */
			hval ^= (uint32_t)*bp++;

			/* multiply by the 32 bit FNV magic prime mod 2^32 */
			hval *= FNV_32_PRIME;
		}

		/* return our new hash value */
		return hval;
	}

	/*
	 * fnv_32a_str - perform a 32 bit Fowler/Noll/Vo FNV-1a hash on a string
	 *
	 * input:
	 *	str	- string to hash
	 *	hval	- previous hash value or 0 if first call
	 *
	 * returns:
	 *	32 bit hash as a static hash type
	 *
	 * NOTE: To use the recommended 32 bit FNV-1a hash, use FNV1_32A_INIT as the
	 *  	 hval arg on the first call to either fnv_32a_buf() or fnv_32a_str().
	 */
	static uint32_t fnv_32a_str(const char *str, uint32_t hval = FNV1_32A_INIT)
	{
		const unsigned char *s = (const unsigned char *)str;	/* unsigned string */

		/*
		 * FNV-1a hash each octet in the buffer
		 */
		while (*s)
		{
			/* xor the bottom with the current octet */
			hval ^= (uint32_t)*s++;

			/* multiply by the 32 bit FNV magic prime mod 2^32 */
			hval *= FNV_32_PRIME;
		}

		/* return our new hash value */
		return hval;
	}

	static constexpr uint64_t FNV1A_64_INIT = (uint64_t)0xcbf29ce484222325ULL;
	static constexpr uint64_t FNV_64_PRIME = (uint64_t)0x100000001b3ULL;

	/*
	 * fnv_64a_buf - perform a 64 bit Fowler/Noll/Vo FNV-1a hash on a buffer
	 *
	 * input:
	 *	buf	- start of buffer to hash
	 *	len	- length of buffer in octets
	 *	hval	- previous hash value or 0 if first call
	 *
	 * returns:
	 *	64 bit hash as a static hash type
	 *
	 * NOTE: To use the recommended 64 bit FNV-1a hash, use FNV1A_64_INIT as the
	 * 	 hval arg on the first call to either fnv_64a_buf() or fnv_64a_str().
	 */
	static uint64_t fnv_64a_buf(const void *buf, size_t len, uint64_t hval = FNV1A_64_INIT)
	{
		const unsigned char *bp = (const unsigned char *)buf; /* start of buffer */
		const unsigned char *be = bp + len;             /* beyond end of buffer */

		/*
		 * FNV-1a hash each octet of the buffer
		 */
		while (bp < be)
		{
			/* xor the bottom with the current octet */
			hval ^= (uint64_t)*bp++;

			/* multiply by the 64 bit FNV magic prime mod 2^64 */
			hval *= FNV_64_PRIME;
		}

		/* return our new hash value */
		return hval;
	}

	/*
	 * fnv_64a_str - perform a 64 bit Fowler/Noll/Vo FNV-1a hash on a buffer
	 *
	 * input:
	 *	buf	- start of buffer to hash
	 *	hval	- previous hash value or 0 if first call
	 *
	 * returns:
	 *	64 bit hash as a static hash type
	 *
	 * NOTE: To use the recommended 64 bit FNV-1a hash, use FNV1A_64_INIT as the
	 * 	 hval arg on the first call to either fnv_64a_buf() or fnv_64a_str().
	 */
	static uint64_t fnv_64a_str(const char *str, uint64_t hval = FNV1A_64_INIT)
	{
		const unsigned char *s = (const unsigned char *)str; /* unsigned string */

		/*
		 * FNV-1a hash each octet of the string
		 */
		while (*s)
		{
			/* xor the bottom with the current octet */
			hval ^= (uint64_t)*s++;

			/* multiply by the 64 bit FNV magic prime mod 2^64 */
			hval *= FNV_64_PRIME;
		}

		/* return our new hash value */
		return hval;
	}

};

} // namespace cz

