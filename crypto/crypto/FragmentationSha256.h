/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAG_SHA256
#define _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAG_SHA256

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SHA256_C)

#include "mbed.h"
#include "FragmentationBlockDeviceWrapper.h"
#include "sha256.h"

class FragmentationSha256 {
public:
    /**
     * Calculate the SHA256 hash of a file in flash
     *
     * @param flash         Instance of BlockDevice
     * @param buffer        A buffer to be used to read into
     * @param buffer_size   The size of the buffer
     */
    FragmentationSha256(FragmentationBlockDeviceWrapper* flash, uint8_t* buffer, size_t buffer_size);

    /**
     * Calculate the SHA256 hash of the file
     *
     * @param address   Offset of the file in flash
     * @param size      Size of the file in flash
     *
     * @returns SHA256 hash of the file
     */
    void calculate(uint32_t address, size_t size, unsigned char output[32]);

private:
    FragmentationBlockDeviceWrapper* _flash;
    uint8_t* _buffer;
    size_t _buffer_size;
    mbedtls_sha256_context _sha256_ctx;
};

#endif

#endif // _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAG_SHA256
