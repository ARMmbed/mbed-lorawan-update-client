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

using namespace utest::v1;

#include "update-client-common/arm_uc_common.h"

int a = 0;
int b = 0;
int c = 0;
int d = 0;

void notification(void)
{
    d++;
}

void setA(uint32_t parameter) { a = parameter + b + c; }
void setB(uint32_t parameter) { b = a + parameter + c; }
void setC(uint32_t parameter) { c = a + b + parameter; }

arm_uc_callback_t callA = { NULL, 0, NULL, 0 };
arm_uc_callback_t callB = { NULL, 0, NULL, 0 };
arm_uc_callback_t callC = { NULL, 0, NULL, 0 };

void test_scheduler()
{
    ARM_UC_AddNotificationHandler(notification);

    TEST_ASSERT_TRUE_MESSAGE(d == 0,
        "variable d not clear");

    ARM_UC_PostCallback(&callA, setA, 100);

    TEST_ASSERT_TRUE_MESSAGE(d == 1,
        "variable d not correct value");

    ARM_UC_PostCallback(&callB, setB, 200);

    TEST_ASSERT_TRUE_MESSAGE(d == 1,
        "variable d not correct value");

    ARM_UC_PostCallback(&callC, setC, 300);

    TEST_ASSERT_TRUE_MESSAGE(d == 1,
        "variable d not correct value");

    TEST_ASSERT_TRUE_MESSAGE(a == 0,
        "variable a not clear");

    TEST_ASSERT_TRUE_MESSAGE(b == 0,
        "variable b not clear");

    TEST_ASSERT_TRUE_MESSAGE(c == 0,
        "variable c not clear");

    ARM_UC_ProcessQueue();

    TEST_ASSERT_TRUE_MESSAGE(a == 100,
        "variable a not set");

    TEST_ASSERT_TRUE_MESSAGE(b == 300,
        "variable b not set");

    TEST_ASSERT_TRUE_MESSAGE(c == 700,
        "variable c not set");
}

Case cases[] = {
    Case("test_scheduler", test_scheduler)
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
