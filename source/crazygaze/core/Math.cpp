#include "Math.h"

namespace cz
{

std::size_t next_pow2(std::size_t n) noexcept
{
	std::size_t result = 1;
	while (result <= n)
	{
		result *= 2;
	}
	return result;
}

std::size_t round_pow2(std::size_t n) noexcept
{
	if ((n == 0) || (n & (n - 1)))
		return next_pow2(n);
	return n;
}

} // namespace cz

