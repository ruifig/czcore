#pragma once

#include "Common_Macros.h"

/**
 * Defines the bitwise operators for an enum class that is used as a flags enum.
 */
#define CZ_DEFINE_ENUM_FLAGS_OPERATORS(Enum)                             \
	constexpr Enum operator|(Enum a, Enum b) noexcept                    \
	{                                                                    \
		using U = std::underlying_type_t<Enum>;                          \
		return static_cast<Enum>(static_cast<U>(a) | static_cast<U>(b)); \
	}                                                                    \
                                                                         \
	constexpr Enum operator&(Enum a, Enum b) noexcept                    \
	{                                                                    \
		using U = std::underlying_type_t<Enum>;                          \
		return static_cast<Enum>(static_cast<U>(a) & static_cast<U>(b)); \
	}                                                                    \
                                                                         \
	constexpr Enum& operator|=(Enum& a, Enum b) noexcept                 \
	{                                                                    \
		a = a | b;                                                       \
		return a;                                                        \
	}                                                                    \
                                                                         \
	constexpr Enum& operator&=(Enum& a, Enum b) noexcept                 \
	{                                                                    \
		a = a & b;                                                       \
		return a;                                                        \
	}                                                                    \
                                                                         \
	constexpr bool any(Enum mask) noexcept                               \
	{                                                                    \
		using U = std::underlying_type_t<Enum>;                          \
		return static_cast<U>(mask) != 0;                                \
	}


