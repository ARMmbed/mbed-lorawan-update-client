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

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_BDFILE
#define _MBED_LORAWAN_UPDATE_CLIENT_BDFILE

#include "FragBDWrapper.h"

// So, janpatch uses POSIX FS calls, let's emulate them, but backed by BlockDevice driver

class BDFILE {
public:
    /**
     * Creates a new BDFILE
     * @param _bd Instance of a BlockDevice
     * @param _offset Offset of the file in flash
     * @param _size Size of the file in flash
     */
    BDFILE(FragBDWrapper* _bd, size_t _offset, size_t _size) :
        bd(_bd), offset(_offset), size(_size), current_pos(0)
    {

    }

    /**
     * Sets position in the file
     * @param pos New position
     * @param origin Seek position
     */
    int fseek(long int pos, int origin) {
        switch (origin) {
            case SEEK_SET: { // from beginning
                current_pos = pos;
                break;
            }
            case SEEK_CUR: {
                current_pos += pos;
                break;
            }
            case SEEK_END: {
                current_pos = size + pos;
                break;
            }
            default: return -1;
        }

        if (current_pos < 0) return -1;
        if (static_cast<size_t>(current_pos) > size) return -1;
        return 0;
    }

    size_t fread(void *buffer, size_t elements, size_t element_size) {
        int r = bd->read(buffer, offset + current_pos, elements * element_size);
        if (r != 0) return 0;

        int new_pos = current_pos + (elements * element_size);
        if (new_pos < 0) {
            return -1;
        }
        if (static_cast<size_t>(new_pos) > size) {
            int r = size - current_pos;
            current_pos = size;
            return r;
        }

        current_pos = new_pos;

        return elements * element_size;
    }

    size_t fwrite(const void *buffer, size_t elements, size_t size) {
        int r = bd->program(buffer, offset + current_pos, elements * size);
        if (r != 0) return 0;

        current_pos += (elements * size);

        return elements * size;
    }

    long int ftell() {
        return current_pos;
    }

private:
    FragBDWrapper* bd;
    size_t offset;
    size_t size;
    int current_pos;
};

// Functions similar to the POSIX functions
int bd_fseek(BDFILE *file, long int pos, int origin) {
    return file->fseek(pos, origin);
}

long int bd_ftell(BDFILE *file) {
    return file->ftell();
}

size_t bd_fread(void *buffer, size_t elements, size_t size, BDFILE *file) {
    return file->fread(buffer, elements, size);
}

size_t bd_fwrite(const void *buffer, size_t elements, size_t size, BDFILE *file) {
    return file->fwrite(buffer, elements, size);
}

#endif // _MBED_LORAWAN_UPDATE_CLIENT_BDFILE
