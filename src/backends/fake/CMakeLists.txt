# SPDX-FileCopyrightText: 2026 elgorro
# SPDX-License-Identifier: MIT

# FakeBackend and FakeBrightness: in-memory monitors for unit and smoke tests.
add_library(pmmt-fake STATIC
    fakebackend.cpp
    fakebackend.h)

target_include_directories(pmmt-fake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pmmt-fake
    PUBLIC pmmt-core
    PRIVATE tomlplusplus::tomlplusplus)

# Fixture parse errors come from toml++ as exceptions; see src/core.
kde_target_enable_exceptions(pmmt-fake PRIVATE)
