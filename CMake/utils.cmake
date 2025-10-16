# ====================================================================
# czAddConfigMacro(<target> <partialName> <macroName> <macroValue>)
#
# Defines <macroName>=<macroValue> for configurations whose names
# contain <partialName>.
#
# If <macroValue> is an empty string (""), the macro is defined
# without a value (i.e., "#define MACRO").
#
# Examples:
#   czAddConfigMacro(myLib "Debug"       "CZ_DEBUG"       "1")
#   czAddConfigMacro(myLib "Release"     "CZ_RELEASE"     "1")
# ====================================================================
function(czAddConfigMacro target partialName macroName macroValue)
    # Determine if macroValue is empty
    set(has_value TRUE)
    if("${macroValue}" STREQUAL "")
        set(has_value FALSE)
    endif()

    # --- Multi-config generators (VS, Xcode, etc.)
    if(CMAKE_CONFIGURATION_TYPES)
        foreach(cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
            if(cfg MATCHES "${partialName}")
                if(has_value)
                    target_compile_definitions(${target} PUBLIC
                        $<$<CONFIG:${cfg}>:${macroName}=${macroValue}>
                    )
                else()
                    target_compile_definitions(${target} PUBLIC
                        $<$<CONFIG:${cfg}>:${macroName}>
                    )
                endif()
            endif()
        endforeach()

    # --- Single-config generators (Make, Ninja, etc.)
    else()
        if(CMAKE_BUILD_TYPE MATCHES "${partialName}")
            if(has_value)
                target_compile_definitions(${target} PUBLIC "${macroName}=${macroValue}")
            else()
                target_compile_definitions(${target} PUBLIC "${macroName}")
            endif()
        endif()
    endif()
endfunction()

