#include "Logging.h"
#include "Math.h"

namespace cz
{

void* alignedAlloc(size_t alignment, size_t size, size_t* adjustedSize)
{
	CZ_CHECK(isPowerOf2(alignment));

	void* ptr;
#if CZ_WINDOWS
	// Validate according to https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc?view=msvc-170
	CZ_CHECK(size != 0);
	ptr = _aligned_malloc(size, alignment);
#else
	// Validate according to https://en.cppreference.com/w/cpp/memory/c/aligned_alloc
	CZ_CHECK(isMultipleOf(alignment, sizeof(void*)));
	// std::aligned_alloc requires `size` to be a multiple of `alignment`. That's ok. We can adjust the size and return that
	// adjusted value so the caller can use that info if useful.
	size = roundUpToMultipleOf(size, alignment);
	ptr = std::aligned_alloc(alignment, size);
#endif

	// std::aligned_alloc has some requirements that can end up failing depending on the alignment and size. So, an assert makes
	// sure we catch something we might miss.
	CZ_CHECK(ptr);

	if (adjustedSize)
	{
		*adjustedSize = size;
	}

	return ptr;
}

void alignedFree(void* ptr)
{
#if CZ_WINDOWS
	return _aligned_free(ptr);
#else
	return std::free(ptr);
#endif
}

} // namespace cz

