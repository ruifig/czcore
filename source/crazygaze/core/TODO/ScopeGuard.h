/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:

	Based on ScopedGuard presented at
	http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
*********************************************************************/

#pragma once

#include "Common.h"

namespace cz
{

template<class Func>
class ScopeGuard
{
public:
	ScopeGuard(Func f)
		: m_fun(std::move(f))
		, m_active(true)
	{
	}

	~ScopeGuard()
	{
		if (m_active)
		{
			m_fun();
		}
	}

	void dismiss()
	{
		m_active = false;
	}

	ScopeGuard() = delete;
	ScopeGuard(const ScopeGuard&) = delete;
	ScopeGuard& operator=(const ScopeGuard&) = delete;
	ScopeGuard(ScopeGuard&& rhs)
		: m_fun(std::move(rhs.m_fun))
		, m_active(rhs.active)
	{
		rhs.dismiss();
	}

private:
	Func m_fun;
	bool m_active;
};


/**
	Using a template function to create guards, since template functions can do type deduction,
	meaning shorter code.

	auto g1 = scopeGuard( [&] { cleanup(); } );
*/
template< class Func>
ScopeGuard<Func> scopeGuard(Func f)
{
	return ScopeGuard<Func>(std::move(f));
}


/**
	By putting a ScopeGuard in a shared_ptr, we can have arbitrary code execute once all strong references
	to the shared pointer are destroyed.
	This is useful for example when we want to queue asynchronous work, and would like for some code to execute
	once all work is done. By passing the std::shared_ptr to the work lambdas, the arbitrary code will automatically execute
	once all work lambdas are finished.
*/
using LifetimeGuard = ScopeGuard<std::function<void()>>;
template<class Func>
std::shared_ptr<LifetimeGuard> lifetimeGuard(Func f)
{
	return std::make_shared<LifetimeGuard>(std::move(f));
}

/**
	Some macro magic so it's easy to set anonymous scope guards. e.g:

	// some code ...
	CZ_SCOPE_EXIT { some cleanup code };
	// more code ...
	CZ_SCOPE_EXIT { more cleanup code };
	// more code ...
 */
namespace detail
{
	enum class ScopeGuardOnExit {};
	template <typename Func>
	__forceinline ::cz::ScopeGuard<Func> operator+(ScopeGuardOnExit, Func&& fn)
	{
		return ::cz::ScopeGuard<Func>(std::forward<Func>(fn));
	}
}

} // namespace cz


#define CZ_SCOPE_EXIT \
	auto CZ_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) \
	= ::cz::detail::ScopeGuardOnExit() + [&]()

