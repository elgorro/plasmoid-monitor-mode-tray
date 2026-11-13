# SPDX-FileCopyrightText: 2026 elgorro
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)

project(pmmt VERSION 0.1.0 LANGUAGES CXX)

# Minimum versions: docs/development/prerequisites.md, "Target platform".
set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(KDEInstallDirs)
include(KDECMakeSettings) # also includes CTest, which provides BUILD_TESTING
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(ECMSetupVersion)
include(FeatureSummary)

ecm_setup_version(PROJECT
    VARIABLE_PREFIX PMMT
    VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/core/pmmt_version.h)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core)
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# `ninja clang-format` formats all C++ sources with ECM's KDE style.
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
