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

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAG_CRC32
#define _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAG_CRC32

#include "mbed.h"
#include "FragBDWrapper.h"
#include "crc32.h"

class FragmentationCrc32 {
public:
    /**
     * Calculate the CRC32 hash of a file in flash
     *
     * @param flash         Instance of FragBDWrapper
     * @param buffer        A buffer to be used to read into
     * @param buffer_size   The size of the buffer
     */
    FragmentationCrc32(FragBDWrapper* flash, uint8_t* buffer, size_t buffer_size);

    /**
     * Calculate the CRC32 hash of the file
     *
     * @param address   Offset of the file in flash
     * @param size      Size of the file in flash
     *
     * @returns CRC32 hash of the file
     */
    uint32_t calculate(uint32_t address, size_t size);

private:
    FragBDWrapper* _flash;
    uint8_t* _buffer;
    size_t _buffer_size;
};

#endif // _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAG_CRC32
