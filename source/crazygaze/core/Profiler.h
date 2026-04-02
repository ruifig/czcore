#pragma once

#ifdef TRACY_ENABLE
	#include "tracy/Tracy.hpp"
#else
	#define TracyLockable( type, varname ) type varname
	#define LockableBase( type ) type
	#define TracyIsConnected false
#endif

