
# ====================================================================
# czAddConfigMacro(<target> <partialName> <macroName> <macroValue>)
#
# For every build configuration that contains <partialName> in its name,
# defines the given preprocessor macro.
#
# Works for both single- and multi-config generators.
#
# Example:
#   czAddConfigMacro(czcore "Debug"       "CZ_DEBUG"       "1")
#   czAddConfigMacro(czcore "Development" "CZ_DEVELOPMENT" "1")
#   czAddConfigMacro(czcore "Release"     "CZ_RELEASE"     "1")
# ====================================================================
function(czAddConfigMacro target partialName macroName macroValue)
    if(CMAKE_CONFIGURATION_TYPES)
        # ---- Multi-config generators (VS, Xcode, etc.)
        foreach(cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
            if(cfg MATCHES "${partialName}")
                target_compile_definitions(${target} PUBLIC
                    $<$<CONFIG:${cfg}>:${macroName}=${macroValue}>
                )
            endif()
        endforeach()

    else()
        # ---- Single-config generators (Ninja, Makefiles, etc.)
        if(CMAKE_BUILD_TYPE MATCHES "${partialName}")
            target_compile_definitions(${target} PUBLIC
                "${macroName}=${macroValue}"
            )
        endif()
    endif()
endfunction()

