# CompilerWarnings.cmake
# Compiler warning configuration for void_engine

function(void_set_compiler_warnings target)
    set(MSVC_WARNINGS
        /W4                 # Baseline reasonable warnings
        /w14242             # 'identifier': conversion, possible loss of data
        /w14254             # 'operator': conversion, possible loss of data
        /w14263             # function does not override any base class virtual function
        /w14265             # class has virtual functions, but destructor is not virtual
        /w14287             # unsigned/negative constant mismatch
        /we4289             # nonstandard extension: 'variable' uses loop control variable
        /w14296             # 'operator': expression is always 'boolean_value'
        /w14311             # pointer truncation from 'type1' to 'type2'
        /w14545             # expression before comma evaluates to a function
        /w14546             # function call before comma missing argument list
        /w14547             # operator before comma has no effect
        /w14549             # operator before comma has no effect
        /w14555             # expression has no effect
        /w14619             # pragma warning: there is no warning number 'number'
        /w14640             # thread un-safe static member initialization
        /w14826             # conversion from 'type1' to 'type_2' is sign-extended
        /w14905             # wide string literal cast to 'LPSTR'
        /w14906             # string literal cast to 'LPWSTR'
        /w14928             # illegal copy-initialization
        /permissive-        # standards conformance mode
    )

    set(CLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow                    # warn if variable shadows one from parent context
        -Wnon-virtual-dtor          # warn if class has virtual functions but no virtual destructor
        -Wold-style-cast            # warn for c-style casts
        -Wcast-align                # warn for potential performance problem casts
        -Wunused                    # warn on anything being unused
        -Woverloaded-virtual        # warn if you overload (not override) a virtual function
        -Wconversion                # warn on type conversions that may lose data
        -Wsign-conversion           # warn on sign conversions
        -Wnull-dereference          # warn if a null dereference is detected
        -Wdouble-promotion          # warn if float is implicit promoted to double
        -Wformat=2                  # warn on security issues around functions that format output
        -Wimplicit-fallthrough      # warn on statements that fallthrough without an explicit annotation
    )

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation    # warn if indentation implies blocks where blocks do not exist
        -Wduplicated-cond           # warn if if/else chain has duplicated conditions
        -Wduplicated-branches       # warn if if/else branches have duplicated code
        -Wlogical-op                # warn about logical operations being used where bitwise were probably wanted
        -Wuseless-cast              # warn if you perform a cast to the same type
    )

    if(MSVC)
        set(PROJECT_WARNINGS_CXX ${MSVC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(PROJECT_WARNINGS_CXX ${CLANG_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PROJECT_WARNINGS_CXX ${GCC_WARNINGS})
    else()
        message(WARNING "No compiler warnings set for CXX compiler: '${CMAKE_CXX_COMPILER_ID}'")
    endif()

    target_compile_options(${target} PRIVATE ${PROJECT_WARNINGS_CXX})
endfunction()
