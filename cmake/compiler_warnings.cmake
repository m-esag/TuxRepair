# Shared compiler warning / sanitizer configuration for TuxRepair.
#
# Warnings are turned ON but -Werror is intentionally NOT used yet: the
# existing codebase has a non-trivial warning backlog (Qt moc macros, sign
# conversions, etc.). Clear the backlog first, then flip on -Werror.
#
# Phase 1 goal: make warnings *visible* so regressions are caught in review.
#
# Sanitizers (ASan + UBSan) are opt-in via -DTUXREPAIR_ENABLE_SANITIZERS=ON,
# because they require libasan/libubsan to be installed on the host and add
# ~2x runtime overhead. The `debug` CMakePreset enables them by default; if
# your system lacks libasan, drop the flag and the build still works:
#   cmake --preset debug -DTUXREPAIR_ENABLE_SANITIZERS=OFF

option(TUXREPAIR_ENABLE_SANITIZERS "Enable AddressSanitizer + UBSan in this config" OFF)

# Use an INTERFACE library so every target (core, ui, tests) gets the same
# flags by simply linking against it.
add_library(tuxrepair_warnings INTERFACE)
add_library(TuxRepair::Warnings ALIAS tuxrepair_warnings)

if(MSVC)
    target_compile_options(tuxrepair_warnings INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
else()
    target_compile_options(tuxrepair_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        # Silence the most noisy categories that fire on Qt macros / generated
        # moc code so the signal-to-noise ratio stays useful.
        -Wno-unused-parameter
    )

    if(TUXREPAIR_ENABLE_SANITIZERS)
        target_compile_options(tuxrepair_warnings INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -O1
        )
        target_link_options(tuxrepair_warnings INTERFACE
            -fsanitize=address,undefined
        )
    endif()
endif()
