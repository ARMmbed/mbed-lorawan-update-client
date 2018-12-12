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

#include "FragmentationCrc32.h"

FragmentationCrc32::FragmentationCrc32(FragmentationBlockDeviceWrapper* flash, uint8_t* buffer, size_t buffer_size)
    : _flash(flash), _buffer(buffer), _buffer_size(buffer_size)
{

}

uint32_t FragmentationCrc32::calculate(uint32_t address, size_t size) {
    size_t offset = address;
    size_t bytes_left = size;

    uint64_t crc = 0;

    while (bytes_left > 0) {
        size_t length = _buffer_size;
        if (length > bytes_left) length = bytes_left;

        _flash->read(_buffer, offset, length);

        crc = crc32(crc, _buffer, length);

        offset += length;
        bytes_left -= length;
    }

    return crc;
}
