# SPDX-FileCopyrightText: 2026 elgorro
# SPDX-License-Identifier: MIT

# Unit tests: docs/development/unit-tests.md. One executable per module.
include(ECMAddTests)

ecm_add_test(test_version.cpp
    TEST_NAME test_version
    LINK_LIBRARIES pmmt-core Qt6::Test)

ecm_add_test(test_modestore.cpp
    TEST_NAME test_modestore
    LINK_LIBRARIES pmmt-core Qt6::Test)
target_compile_definitions(test_modestore PRIVATE
    PMMT_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")

ecm_add_test(test_modediff.cpp
    TEST_NAME test_modediff
    LINK_LIBRARIES pmmt-core Qt6::Test)

ecm_add_test(test_fakebackend.cpp
    TEST_NAME test_fakebackend
    LINK_LIBRARIES pmmt-fake Qt6::Test)
target_compile_definitions(test_fakebackend PRIVATE
    PMMT_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")

ecm_add_test(test_modeapplier.cpp
    TEST_NAME test_modeapplier
    LINK_LIBRARIES pmmt-fake Qt6::Test)
target_compile_definitions(test_modeapplier PRIVATE
    PMMT_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
