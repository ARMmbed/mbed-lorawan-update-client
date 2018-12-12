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

#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <string.h>

#include "update-client-common/arm_uc_crypto.h"
#include "../alice.h"

#define MIN_SIZE (16)

arm_uc_cipherHandle_t cipherHandle;
uint8_t output_ptr[1000];

void test_unit()
{
    uint8_t nc_copy[16];

    for (uint32_t block_size = MIN_SIZE + 1; block_size < 1000; block_size++)
    {
        printf("block size: %" PRIu32 "\r\n", block_size);

        arm_uc_buffer_t keyBuffer = {
            .size_max = 32,
            .size = 32,
            .ptr = (uint8_t*) key
        };

        /* mbedtls needs to modify the nonce, so copy its original value
           into a new buffer on each iteration */
        memcpy(nc_copy, nc, sizeof(nc));
        arm_uc_buffer_t ivBuffer = {
            .size_max = 16,
            .size = 16,
            .ptr = (uint8_t*) nc_copy
        };

        arm_uc_buffer_t input = {
            .size_max = sizeof(ecila),
            .size = sizeof(ecila),
            .ptr = (uint8_t*) ecila
        };

        arm_uc_buffer_t output = {
            .size_max = block_size,
            .size = 0,
            .ptr = output_ptr
        };

        arm_uc_error_t result = ARM_UC_cryptoDecryptSetup(&cipherHandle,
                                                          &keyBuffer,
                                                          &ivBuffer,
                                                          256);

        for (size_t index = 0; index < input.size; )
        {
            uint32_t size = block_size;

            if (index + block_size > input.size)
            {
                size = input.size - index;
            }

            result = ARM_UC_cryptoDecryptUpdate(&cipherHandle,
                                                &input.ptr[index],
                                                size,
                                                &output);

            TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(&alice[index],
                                                  output.ptr,
                                                  output.size,
                                                  "decryption failed");

            if (result.error == ERR_NONE)
            {
                index += output.size;
            }
            else
            {
                TEST_FAIL_MESSAGE("decryption failed");
                break;
            }
        }

        ARM_UC_cryptoDecryptFinish(&cipherHandle, &output);

        /* commented out for speed */
        /* for (size_t outex = 0; outex < output.size; outex++) */
        /* { */
        /*     printf("%c", output.ptr[outex]); */
        /* } */
    }

    printf("done\r\n");
}

Case cases[] = {
    Case("test_init", test_unit)
};

utest::v1::status_t greentea_setup(const size_t number_of_cases)
{
#if defined(TARGET_LIKE_MBED)
    GREENTEA_SETUP(10 * 60, "default_auto");
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
