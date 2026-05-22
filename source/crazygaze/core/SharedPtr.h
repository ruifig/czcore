#pragma once

#include "details/BasicSharedPtr.h"

/**
 *
 * This provides the thread safe shared pointer types.
 * If you don't need thread safety, consider using LocalSharedPtr and LocalWeakPtr instead, which are more efficient.
 *
 * Available classes:
 * 
 * - SharedPtr
 *		Equivalent to std::shared_ptr.
 * - WeakPtr
 *		Equivalent to std::weak_ptr.
 * - SharedRef
 *		A non-nullable version of SharedPtr. Similar to std::shared_ptr but doesn't allow null values. This is handy for APIs that
 *		want to enforce that the pointer is not null.
 * - ObserverPtr
 *		A special kind of WeakPtr that doesn't allow promoting to SharedPtr. The purpose is just to check if a pointer is still valid.
 *		It has very little use, and it's unsafe if used with multi-threading.
 * 
 */

namespace cz
{

template <typename T>
using SharedPtr = details::BasicSharedPtr<T, true>;

template <typename T>
using WeakPtr = details::BasicWeakPtr<T, true, false>;

template <typename T>
using ObserverPtr = details::BasicWeakPtr<T, true, true>;

template<typename T>
using SharedRef = details::BasicSharedRef<T, true>;

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&& ... args)
{
	return details::basicMakeShared<T, true>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
SharedRef<T> makeSharedRef(Args&& ... args)
{
	return details::basicMakeSharedRef<T, true>(std::forward<Args>(args)...);
}

template <typename T>
using EnableSharedFromThis = details::BasicEnableSharedFromThis<T, true>;

/**
 * Helper to use in lambdas that capture a "weakThis".
 * function.
 */
#define lockWeakThis()                 \
	auto strongThis = weakThis.lock(); \
	if (!strongThis)                   \
		return;


} // namespace cz

