# SPDX-FileCopyrightText: 2026 elgorro
# SPDX-License-Identifier: MIT

# libpmmt-core: mode model, config, diffing, apply logic.
# Placeholder until the first modules land (milestone v0.1.0).
add_library(pmmt-core STATIC
    version.cpp
    version.h)

target_include_directories(pmmt-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}) # generated pmmt_version.h

target_link_libraries(pmmt-core PUBLIC Qt6::Core)
