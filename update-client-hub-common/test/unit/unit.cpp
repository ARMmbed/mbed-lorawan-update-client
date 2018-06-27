// ----------------------------------------------------------------------------
// Copyright 2016-2017 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#include <greentea-client/test_env.h>
#include <utest/utest.h>
#include <unity/unity.h>

extern "C" {
#include "update-client-common/arm_uc_common.h"
}

using namespace utest::v1;

const uint8_t test_str[] = "http://circleci.com/gh/ARMmbed";

void test_str_to_uri()
{
    uint8_t buffer_big[256];
    uint8_t buffer_small[10];

    arm_uc_uri_t test_uri = {
        .size_max = sizeof(buffer_big),
        .size     = 0,
        .ptr      = buffer_big,
        .scheme   = URI_SCHEME_NONE,
        .port     = 0,
        .host     = NULL,
        .path     = NULL,
    };

    arm_uc_error_t retval;
    retval = arm_uc_str2uri(test_str, sizeof(test_str), &test_uri);

    TEST_ASSERT_EQUAL_HEX_MESSAGE(ERR_NONE, retval.error,
        "str to arm_uc_uri_t conversion failed");

    TEST_ASSERT_EQUAL(URI_SCHEME_HTTP, test_uri.scheme);
    TEST_ASSERT_EQUAL(80, test_uri.port);
    TEST_ASSERT_EQUAL_STRING("circleci.com", test_uri.host);
    TEST_ASSERT_EQUAL_STRING("/gh/ARMmbed", test_uri.path);
}

void test_strnstren()
{
    const uint8_t input[] = "_________1__________2_________3_________4_________5";

    uint32_t index = UINT32_MAX;

    index = arm_uc_strnstrn(input, sizeof(input), (const uint8_t*) "9", 1);
    TEST_ASSERT_EQUAL(index, UINT32_MAX);

    index = arm_uc_strnstrn(input, sizeof(input), (const uint8_t*) "_", 1);
    TEST_ASSERT_EQUAL(0, index);
    TEST_ASSERT_EQUAL('_', input[index]);

    index = arm_uc_strnstrn(input, sizeof(input), (const uint8_t*) "1", 1);
    TEST_ASSERT_EQUAL(9, index);
    TEST_ASSERT_EQUAL('1', input[index]);

    index = arm_uc_strnstrn(input, sizeof(input), (const uint8_t*) "2", 1);
    TEST_ASSERT_EQUAL(20, index);
    TEST_ASSERT_EQUAL('2', input[index]);

    index = arm_uc_strnstrn(input, sizeof(input), (const uint8_t*) "_________4", 10);
    TEST_ASSERT_EQUAL(31, index);
    TEST_ASSERT_EQUAL('_', input[index]);

    index = arm_uc_strnstrn(input, sizeof(input), (const uint8_t*) "5", 1);
    TEST_ASSERT_EQUAL(50, index);
    TEST_ASSERT_EQUAL('5', input[index]);
}

Case cases[] = {
    Case("test_str_to_uri", test_str_to_uri),
    Case("test_strnstren", test_strnstren),
};

Specification specification(cases, verbose_continue_handlers);

#if defined(TARGET_LIKE_MBED)
int main()
#elif defined(TARGET_LIKE_POSIX)
void app_start(int argc __unused, char** argv __unused)
#endif
{
    // Run the test specification
    Harness::run(specification);
}
