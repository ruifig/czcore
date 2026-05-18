#pragma once

#include "details/BasicSharedPtr.h"

/**
 *
 * This provides the non-thread safe shared pointer types.
 * If you need thread safety, consider using SharedPtr and WeakPtr instead, which are more expensive.
 *
 * Available classes:
 * 
 * - LocalSharedPtr
 *		Equivalent to std::shared_ptr.
 * - LocalWeakPtr
 *		Equivalent to std::weak_ptr.
 * - LocalSharedRef
 *		A non-nullable version of LocalSharedPtr. Similar to std::shared_ptr but doesn't allow null values. This is handy for APIs that
 *		want to enforce that the pointer is not null.
 * - LocalObserverPtr
 *		A special kind of LocalWeakPtr that doesn't allow promoting to LocalSharedPtr. The purpose is just to check if a pointer is still valid.
 *		It has very little use, and it's unsafe if used with multi-threading.
 * 
 */

namespace cz
{

template<typename T>
using LocalSharedPtr = details::BasicSharedPtr<T, false>;

template<typename T>
using LocalWeakPtr = details::BasicWeakPtrImpl<T, false, false>;

template<typename T>
using LocalObserverPtr = details::BasicWeakPtrImpl<T, false, true>;


template<typename T>
using LocalSharedRef = details::BasicSharedRef<T, false>;

template <typename T, typename... Args>
LocalSharedPtr<T> makeLocalShared(Args&& ... args)
{
	return details::basicMakeShared<T, false>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
LocalSharedRef<T> makeLocalSharedRef(Args&& ... args)
{
	return details::basicMakeSharedRef<T, false>(std::forward<Args>(args)...);
}

template <typename T>
using EnableLocalSharedFromThis = details::BasicEnableSharedFromThis<T, false>;

} // namespace cz

