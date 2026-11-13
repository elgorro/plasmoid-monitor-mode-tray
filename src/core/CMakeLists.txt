# SPDX-FileCopyrightText: 2026 elgorro
# SPDX-License-Identifier: MIT

# libpmmt-core: mode model, config, diffing, apply logic.
add_library(pmmt-core STATIC
    configerror.h
    mode.h
    modestore.cpp
    modestore.h
    vcp.cpp
    vcp.h
    version.cpp
    version.h)

target_include_directories(pmmt-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}) # generated pmmt_version.h

target_link_libraries(pmmt-core
    PUBLIC Qt6::Core
    PRIVATE tomlplusplus::tomlplusplus)

# toml++ reports parse errors as exceptions (TOML_EXCEPTIONS=1 in the distro
# builds); KDECompilerSettings turns exceptions off by default. They are
# caught inside ModeStore and never cross the library's API.
kde_target_enable_exceptions(pmmt-core PRIVATE)
