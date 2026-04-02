#pragma once

#include "Common_Macros.h"

CZ_THIRD_PARTY_INCLUDES_START

__pragma(warning(push))
__pragma(warning(disable: 5267))  /* definition of implicit copy constructor/assignment operator for 'type' is deprecated because it has a user-provided assignment operator/copy constructor */
#include <catch2/catch_all.hpp>
__pragma(warning(pop))

//#include <catch2/catch_session.hpp>
//#include <catch2/reporters/catch_reporter_registrars.hpp>
CZ_THIRD_PARTY_INCLUDES_END


