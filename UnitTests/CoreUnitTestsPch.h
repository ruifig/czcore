#pragma once

#include "crazygaze/core/CorePch.h" 

#include "crazygaze/core/SharedPtr.h"
#include "crazygaze/core/FNVHash.h"
#include "crazygaze/core/VSOVector.h"
#include "crazygaze/core/TaggedPtr.h"
#include "crazygaze/core/FixedHeapArray.h"

#include <print>
#include <future>
#include <chrono>
#include <type_traits>

CZ_THIRD_PARTY_INCLUDES_START

__pragma(warning(push))
__pragma(warning(disable: 5267))  /* definition of implicit copy constructor/assignment operator for 'type' is deprecated because it has a user-provided assignment operator/copy constructor */
#include <catch2/catch_all.hpp>
//#include <catch2/catch_session.hpp>
//#include <catch2/reporters/catch_reporter_registrars.hpp>
__pragma(warning(pop))

CZ_THIRD_PARTY_INCLUDES_END


