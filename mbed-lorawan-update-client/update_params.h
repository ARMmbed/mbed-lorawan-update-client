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

/**
 * This file is shared between bootloader and the target application
 */

#ifndef _MBED_FOTA_UPDATE_PARAMS
#define _MBED_FOTA_UPDATE_PARAMS

// These values need to be the same between target application and bootloader!
#define     FOTA_SIGNATURE_LENGTH  sizeof(UpdateSignature_t)    // Length of RSA signature + class UUIDs + diff struct (5 bytes) -> matches sizeof(UpdateSignature_t)

// This structure contains the update header (which is the first FOTA_SIGNATURE_LENGTH bytes of a package)
typedef struct __attribute__((__packed__)) {
    uint8_t signature_length;           // Length of the ECDSA/SHA256 signature
    unsigned char signature[72];        // ECDSA/SHA256 signature, signed with private key of the firmware (after applying patching), length is 70, 71 or 72
    uint8_t manufacturer_uuid[16];      // Manufacturer UUID
    uint8_t device_class_uuid[16];      // Device Class UUID

    uint32_t diff_info;                 // first byte indicates whether this is a diff, last three bytes are the size of the *old* file
} UpdateSignature_t;

#endif
