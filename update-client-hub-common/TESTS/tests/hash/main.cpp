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

#include <iostream>
#include <fstream>
#include <inttypes.h>

#include "update-client-common/arm_uc_crypto.h"
#include "../alice.h"

void test_unit(void)
{
    uint8_t output_ptr[100];
    arm_uc_buffer_t output = {
        .size_max = sizeof(output_ptr),
        .size = 0,
        .ptr = output_ptr
    };

    for (uint32_t block_size = 1; block_size < 1000; block_size++)
    {
        printf("block size: %" PRIu32 "\r\n", block_size);

        arm_uc_mdHandle_t mdHandle;
        ARM_UC_cryptoHashSetup(&mdHandle, ARM_UC_CU_SHA256);

        for (size_t offset = 0; offset < sizeof(alice); offset += block_size)
        {
            arm_uc_buffer_t input = {
                .size_max = block_size,
                .size = block_size,
                .ptr = (uint8_t*) &alice[offset]
            };

            if ((offset + block_size) > sizeof(alice))
            {
                input.size = sizeof(alice) - offset;
            }

            ARM_UC_cryptoHashUpdate(&mdHandle, &input);
        }

        ARM_UC_cryptoHashFinish(&mdHandle, &output);

        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(hash, output.ptr, output.size, "hash");
    }
}

Case cases[] = {
    Case("test_unit", test_unit)
};

utest::v1::status_t greentea_setup(const size_t number_of_cases)
{
#if defined(TARGET_LIKE_MBED)
    GREENTEA_SETUP(4 * 60, "default_auto");
#endif
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_setup, cases);

#if defined(TARGET_LIKE_MBED)
int main()
#elif defined(TARGET_LIKE_POSIX)
void app_start(int argc __unused, char** argv __unused)
#endif
{
    // Run the test specification
    Harness::run(specification);
}
