#pragma once

namespace cz
{

/**
 * Helper that allows searching a container of smart pointers of T by a raw pointer
 *
 * See: https://stackoverflow.com/questions/18939882/raw-pointer-lookup-for-sets-of-unique-ptrs
 */
template<class T>
struct pointer_comp
{
	typedef std::true_type is_transparent;

	// helper does some magic in order to reduce the number of
	// pairs of types we need to know how to compare: it turns
	// everything into a pointer, and then uses `std::less<T*>`
	// to do the comparison:
	struct helper
	{
		T* ptr;
		helper()
			: ptr(nullptr)
		{
		}

		helper(helper const&) = default;

		helper(T* p)
			: ptr(p)
		{
		}

		template <class U>
		helper(std::shared_ptr<U> const& sp)
			: ptr(sp.get())
		{
		}

		template <class U>
		helper(ObjectPtr<U> const& sp)
			: ptr(sp.get())
		{
		}

		template <class U, class... Ts>
		helper(std::unique_ptr<U, Ts...> const& up)
			: ptr(up.get())
		{
		}

		// && optional: enforces rvalue use only
		bool operator<(helper o) const
		{
			return std::less<T*>()(ptr, o.ptr);
		}
	};

	// without helper, we would need 2^n different overloads, where
	// n is the number of types we want to support (so, 8 with
	// raw pointers, unique pointers, and shared pointers).  That
	// seems silly:
	// && helps enforce rvalue use only
	bool operator()(helper const&& lhs, helper const&& rhs) const
	{
		return lhs < rhs;
	}
};


/**
 * Wrapper for std::aligned_alloc, because MSVC doesn't support it
 * If the platform supports, it calls std::aligned_alloc, if not then calls whatever the platform provides
 *
 * WARNING: `alignedFree` MUST be used to free the memory. Don't try to deallocate with something else (e.g: free or delete)
 *
 * \param alignment
 *		What should be the alignment of the allocated memory
 * \param size
 *		Minimum size required.
 *		Note that depending on the platform, the actual allocated size can be larger.
 * \param adjustedSize
 *		If not nulled and `size` had to be adjusted to satisfy requirements (e.g, the `std::aligned_alloc` requirements),
 *		then on exit this will contain the adjusted size. This can be useful information for the caller, if it is allocating
 *		memory for a container and the adjusted size will be the actual new capacity of the container.
 */
void* alignedAlloc(size_t alignment, size_t size, size_t* adjustedSize = nullptr);

/**
 * Counterpart to alignedAlloc.
 */
void alignedFree(void* ptr);


}

