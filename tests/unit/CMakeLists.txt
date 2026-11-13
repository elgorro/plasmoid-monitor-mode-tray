# SPDX-FileCopyrightText: 2026 elgorro
# SPDX-License-Identifier: MIT

# Unit tests: docs/development/unit-tests.md. One executable per module.
include(ECMAddTests)

ecm_add_test(test_version.cpp
    TEST_NAME test_version
    LINK_LIBRARIES pmmt-core Qt6::Test)
