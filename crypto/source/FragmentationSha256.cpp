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

#include "crypto/FragmentationSha256.h"

FragmentationSha256::FragmentationSha256(FragBDWrapper* flash, uint8_t* buffer, size_t buffer_size)
    : _flash(flash), _buffer(buffer), _buffer_size(buffer_size)
{
}

void FragmentationSha256::calculate(uint32_t address, size_t size, unsigned char output[32]) {
    mbedtls_sha256_init(&_sha256_ctx);
    mbedtls_sha256_starts(&_sha256_ctx, false /* is224 */);

    size_t offset = address;
    size_t bytes_left = size;

    while (bytes_left > 0) {
        size_t length = _buffer_size;
        if (length > bytes_left) length = bytes_left;

        _flash->read(_buffer, offset, length);

        mbedtls_sha256_update(&_sha256_ctx, _buffer, length);

        offset += length;
        bytes_left -= length;
    }

    mbedtls_sha256_finish(&_sha256_ctx, output);
    mbedtls_sha256_free(&_sha256_ctx);
}
