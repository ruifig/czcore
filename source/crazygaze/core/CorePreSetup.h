#pragma once

#if defined(_MSVC_LANG)
#define CZ_THIRD_PARTY_INCLUDES_START                                                                                                                                                                           \
		__pragma(warning(push))                                                                                                                                                                                 \
		__pragma(warning(disable: 4510))  /* '<class>': default constructor could not be generated. */                                                                                                          \
		__pragma(warning(disable: 5045))  /* Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified */                                                                           \
		__pragma(warning(disable: 4355))  /* 'this': used in base member initializer list */                                                                                                                    \
		__pragma(warning(disable: 4388))  /* ''token' : signed/unsigned mismatch*/                                                                                                                              \
		__pragma(warning(disable: 5204))  /* '<class>': class has virtual functions, but its trivial destructor is not virtual; instances of objects derived from this class may not be destructed correctly */ \
		__pragma(warning(disable: 4061))  /* enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label */                                                               \
		__pragma(warning(disable: 4702))  /* unreachable code */                                                                                                                                                \
		__pragma(warning(disable: 4868))  /* compiler may not enforce left-to-right evaluation order in braced initializer list */                                                                              \
		__pragma(warning(disable: 4820))  /* 'bytes' bytes padding added after construct 'member_name' */                                                                                                       \
		__pragma(warning(disable: 4365))  /* 'action' : conversion from 'type_1' to 'type_2', signed/unsigned mismatch */                                                                                       \
		__pragma(warning(disable: 4623))  /* 'derived class' : default constructor was implicitly defined as deleted */                                                                                         \
		__pragma(warning(disable: 5262))  /* implicit fall-through occurs here; are you missing a break statement? Use [[fallthrough]] when a break statement is intentionally omitted between cases */         \
		/* __pragma(warning(disable: 5267))  // definition of implicit copy constructor/assignment operator for 'type' is deprecated because it has a user-provided assignment operator/copy constructor */
		__pragma(warning(disable: 4625))  /* 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted */                           \
		__pragma(warning(disable: 4626))  /* 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted */                     \
		__pragma(warning(disable: 5026))  /* "'type': move constructor was implicitly defined as deleted */                                                                                                     \
		__pragma(warning(disable: 5027))  /* "'type': move assignment operator was implicitly defined as deleted */                                                                                             \
		__pragma(warning(disable: 5031))  /* #pragma warning(pop): likely mismatch, popping warning state pushed in different file */                                                                           \
		__pragma(warning(disable: 4582))  /* 'type': constructor is not implicitly called */                                                                                                                    \
		__pragma(warning(disable: 4583))  /* 'type': destructor is not implicitly called */                                                                                                                     \
		__pragma(warning(disable: 4668))  /* 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives' */                                                                            \
		__pragma(warning(disable: 4619))  /* #pragma warning : there is no warning number 'number' */                                                                                                           \
		__pragma(warning(disable: 4710))  /* 'function' : function not inlined */                                                                                                                               \
		__pragma(warning(disable: 4711))  /* "function 'function' selected for inline expansion */                                                                                                              \
		__pragma(warning(disable: 4201))  /* nonstandard extension used: nameless struct/union */                                                                                                               \
		__pragma(warning(disable: 5219))  /* "implicit conversion from 'type-1' to 'type-2', possible loss of data" */                                                                                          \
		__pragma(warning(disable: 4865))  /* '<ENUM>': the underlying type will change from 'int' to 'unsigned int' when '/Zc:enumTypes' is specified on the command line */

	#define CZ_THIRD_PARTY_INCLUDES_END \
		__pragma(warning(pop))
#else
	#define CZ_THIRD_PARTY_INCLUDES_START
	#define CZ_THIRD_PARTY_INCLUDES_END
#endif

#if defined(_MSVC_LANG)

	// On Windows/Visual Studio, we enable these warnings and treat them as errors. Any of these are VERY likely to be bugs in the
	// code if they show up.
	#pragma warning(error: 4003) /* not enough actual parameters for macro 'identifier' */
	#pragma warning(error: 4061) /* enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label */
	#pragma warning(error: 4062) /* enumerator 'identifier' in switch of enum 'enumeration' is not handled */
	#pragma warning(error: 4701) /* Potentially uninitialized local variable 'name' used */
	#pragma warning(error: 4715) /* 'function' : not all control paths return a value */
	#pragma warning(error: 4946) /* reinterpret_cast used between related classes: 'class1' and 'class2' */
	#pragma warning(error: 4265) /* 'class' : class has virtual functions, but destructor is not virtual */

	// The following we disable
	__pragma(warning(disable: 4514)) /* 'function' : unreferenced inline function has been removed */
	__pragma(warning(disable: 4820)) /* 'bytes' bytes padding added after construct 'member_name' */ 
	__pragma(warning(disable: 4623))  /* 'derived class' : default constructor was implicitly defined as deleted */
	__pragma(warning(disable: 4625)) /* 'derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted */
	__pragma(warning(disable: 4626)) /* 'derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted */
	__pragma(warning(disable: 5026)) /* "'type': move constructor was implicitly defined as deleted */
	__pragma(warning(disable: 5027)) /* "'type': move assignment operator was implicitly defined as deleted */
	__pragma(warning(disable: 5045)) /* Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified */
	__pragma(warning(disable: 4371)) /* 'classname': layout of class may have changed from a previous version of the compiler due to better packing of member 'member' */
	__pragma(warning(disable: 4866)) /* 'file(line_number)' compiler may not enforce left-to-right evaluation order for call to operator_name */
	__pragma(warning(disable: 4464)) /* "relative include path contains '..' */
	__pragma(warning(disable: 4710)) /* 'function' : function not inlined */
	__pragma(warning(disable: 4711)) /* "function 'function' selected for inline expansion */
	__pragma(warning(disable: 5039)) /* "function': pointer or reference to potentially throwing function passed to extern C function under -EHc. Undefined behavior may occur if this function throws an exception. */
	__pragma(warning(disable: 4548)) /* "expression before comma has no effect; expected expression with side-effect */
	__pragma(warning(disable: 4996)) /* 'function': This function or variable may be unsafe. Consider using sprintf_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details. */

#else

#endif


// Set the CZ_<PLATFORM_NAME> macro
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#define CZ_WINDOWS 1
#elif __APPLE__
	#include "TargetConditionals.h"
	#if TARGET_OS_IPHONE && TARGET_OS_SIMULATOR
		// define something for simulator
		// (although, checking for TARGET_OS_IPHONE should not be required).
		#error Unknown/unsupported platform
	#elif TARGET_OS_IPHONE && TARGET_OS_MACCATALYST
		// define something for Mac's Catalyst
		#error Unknown/unsupported platform
	#elif TARGET_OS_IPHONE
		// define something for iphone  
		#error Unknown/unsupported platform
	#else
		#define CZ_MACOSX 1
	#endif
#elif __linux__
	#define CZ_LINUX 1
#else
	#error Unknown/unsupported platform
#endif

#ifndef CZ_WINDOWS
	#define CZ_WINDOWS 0
#endif

#ifndef CZ_MACOSX
	#define CZ_MACOSX 0
#endif

#ifndef CZ_LINUX
	#define CZ_LINUX 0
#endif

#if defined(_WIN32) || defined(_WIN64)
	#if defined(_WIN64)
		#define CZ_64BITS 1
		#define CZ_32BITS 0
	#else
		#define CZ_64BITS 0
		#define CZ_32BITS 1
	#endif
#elif defined __GNUC__
	#if defined(__x86_64__) || defined(__ppc64__) || defined(__arm64__)
		#define CZ_64BITS 1
		#define CZ_32BITS 0
	#else
		#define CZ_64BITS 0
		#define CZ_32BITS 1
	#endif
#else
	#error Unknown compiler
#endif

#ifndef CZ_DEBUG
	#define CZ_DEBUG 0
#endif

#ifndef CZ_DEVELOPMENT
	#define CZ_DEVELOPMENT 0
#endif

#ifndef CZ_RELEASE
	#define CZ_RELEASE 0
#endif

namespace cz
{
	namespace details
	{
	}
}

