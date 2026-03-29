#pragma once

#include "Common_Macros.h"

#define CZ_SCOPE_EXIT \
	auto CZ_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) \
	= ::cz::detail::ScopeGuardOnExit() + [&]()

