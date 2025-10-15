namespace cz
{

	/**
	 * Returns the lowest power of 2 greater than n
	 * IMPORTANT: Note the "greater than" detail. E.g, if you pass 8, it will return 16.
	 */
	std::size_t next_pow2(std::size_t n) noexcept;

	/**
	 * Returns the lowest power of 2 greater or equal to n
	 * IMPORTANT: 0 is not a power of two, so if `n` is 0, it will return 1 (which is the next power of two)
	 */
	std::size_t round_pow2(std::size_t n) noexcept;

	/**
	 * Checks if the given number is a power of 2
	 */
	template <typename T>
	static constexpr bool isPowerOf2(T x) noexcept
	{
		typedef typename std::make_unsigned<T>::type U;
		// NOTE: 0 is NOT a power of 2, so we are checking for that.
		return x && !(U(x) & (U(x) - U(1)));
	}

	/**
	 * Checks if `a` is a multiple of `b`
	 * Note that if `a` is zero, then it is considered as not a multiple
	 */
	template<typename T>
	static constexpr bool isMultipleOf(T a, T b)
	{
		// We don't want to consider 0 as a multiple of any other number
		return (a) &&  (a % b == 0);
	}

	/**
	 * Returns `a` rounded up to a multiple of `b`
	 */
	template<typename T>
	static constexpr T roundUpToMultipleOf(T a, T b)
	{
		// If `b` is 0, then we don't do any alignment
		if (b == 0)
		{
			return a;
		}

		// Integer division trick to round up `a` to a multiple of `b`
		//
		return ((a + b - 1) / b) * b;
	}

} // namespace cz

