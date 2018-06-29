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

#ifndef _LORAWAN_UPDATE_CLIENT_H_
#define _LORAWAN_UPDATE_CLIENT_H_

#include "mbed.h"
#include "mbed_stats.h"
#include "update_types.h"
#include "FragmentationSha256.h"
#include "FragmentationEcdsaVerify.h"
#include "FragmentationBlockDeviceWrapper.h"

#ifndef NB_FRAG_GROUPS
#define NB_FRAG_GROUPS          1
#endif // NB_FRAG_GROUPS

#ifndef LW_UC_SHA256_BUFFER_SIZE
#define LW_UC_SHA256_BUFFER_SIZE       128
#endif

enum LW_UC_STATUS {
    LW_UC_OK = 0,
    LW_UC_INVALID_PACKET_LENGTH = 1,
    LW_UC_UNKNOWN_COMMAND = 2,
    LW_UC_FRAG_SESSION_NOT_ACTIVE = 3,
    LW_UC_PROCESS_FRAME_FAILED = 4,
    LW_UC_BD_READ_ERROR = 5,
    LW_UC_BD_WRITE_ERROR = 6,
    LW_UC_SIGNATURE_MANUFACTURER_UUID_MISMATCH = 7,
    LW_UC_SIGNATURE_DEVICECLASS_UUID_MISMATCH = 8,
    LW_UC_SIGNATURE_ECDSA_FAILED = 9,
    LW_UC_OUT_OF_MEMORY = 10,
    LW_UC_CREATE_BOOTLOADER_HEADER_FAILED = 11
};

enum LW_UC_EVENT {
    LW_UC_EVENT_FIRMWARE_READY = 0,
    LW_UC_EVENT_FRAGSESSION_COMPLETE = 1
};

class LoRaWANUpdateClient {
public:
    /**
     * Initialize a new LoRaWANUpdateClient
     *
     * @params bd A block device
     * @params send_fn A send function, invoked when we want to relay data back to the network
     */
    LoRaWANUpdateClient(BlockDevice *bd, Callback<void(uint8_t, uint8_t*, size_t)> send_fn)
        : _bd(bd), _send_fn(send_fn), _event_cb(NULL)
    {
        for (size_t ix = 0; ix < NB_FRAG_GROUPS; ix++) {
            frag_sessions[ix].active = false;
            frag_sessions[ix].session = NULL;
        }
    }

    /**
     * Sets the event callback (will let you know when firmware is complete and ready to flash)
     * @param fn Callback function
     */
    void setEventCallback(Callback<void(LW_UC_EVENT)> fn) {
        _event_cb = fn;
    }

    /**
     * Handle packets that came in on the fragmentation port (e.g. 201)
     */
    LW_UC_STATUS handleFragmentationCommand(uint8_t *buffer, size_t length) {
        if (length == 0) return LW_UC_INVALID_PACKET_LENGTH;

        switch (buffer[0]) {
            case FRAG_SESSION_SETUP_REQ:
                return handleFragmentationSetupReq(buffer + 1, length - 1);

            case DATA_FRAGMENT:
                return handleDataFragment(buffer + 1, length - 1);

            default:
               return LW_UC_UNKNOWN_COMMAND;
        }
    }

private:
    /**
     * Start a new fragmentation session
     */
    LW_UC_STATUS handleFragmentationSetupReq(uint8_t *buffer, size_t length) {
        if (length != FRAG_SESSION_SETUP_REQ_LENGTH) {
            // @todo, I assume we need to send a FRAG_SESSION_SETUP_ANS at this point... But not listed in the spec.
            return LW_UC_INVALID_PACKET_LENGTH;
        }

        uint8_t fragIx = (buffer[0] >> 4) & 0b11;

        if (fragIx > NB_FRAG_GROUPS - 1) {
            sendFragSessionAns(FSAE_IndexNotSupported);
            return LW_UC_OK;
        }

        if (frag_sessions[fragIx].active) {
            if (frag_sessions[fragIx].session) {
                // clear memory associated with the session - this should clear out the full context...
                delete frag_sessions[fragIx].session;
            }
        }

        frag_sessions[fragIx].mcGroupBitMask = buffer[0] & 0b1111;
        frag_sessions[fragIx].nbFrag = (buffer[2] << 8) + buffer[1];
        frag_sessions[fragIx].fragSize = buffer[3];
        frag_sessions[fragIx].fragAlgo = (buffer[4] >> 3) & 0b111;
        frag_sessions[fragIx].blockAckDelay = buffer[4] & 0b111;
        frag_sessions[fragIx].padding = buffer[5];
        frag_sessions[fragIx].descriptor = (buffer[9] << 24) + (buffer[8] << 16) + (buffer[7] << 8) + buffer[6];

        // create a fragmentation session which can handle all this...
        FragmentationSessionOpts_t opts;
        opts.NumberOfFragments = frag_sessions[fragIx].nbFrag;
        opts.FragmentSize = frag_sessions[fragIx].fragSize;
        opts.Padding = frag_sessions[fragIx].padding;
        opts.RedundancyPackets = MBED_CONF_LORAWAN_UPDATE_CLIENT_MAX_REDUNDANCY - 1;

        // @todo, make this dependent on the frag index...
        opts.FlashOffset = MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS;

        frag_sessions[fragIx].sessionOptions = opts;

        FragmentationSession *session = new FragmentationSession(&_bd, opts);
        FragResult init_res = session->initialize();
        if (init_res != FRAG_OK) {
            printf("Failed to initialize fragmentation session (out of memory?)\n");
            delete session;

            sendFragSessionAns(FSAE_NotEnoughMemory);
            return LW_UC_OK;
        }

        frag_sessions[fragIx].session = session;
        frag_sessions[fragIx].active = true;

        sendFragSessionAns(FSAE_None);
        return LW_UC_OK;
    }

    /**
     * Send FRAG_SESSION_ANS to network server with bits set depending on the error indicator
     */
    void sendFragSessionAns(FragmenationSessionAnswerErrors error) {
        uint8_t response = 0b0000;

        switch (error) {
            case FSAE_WrongDescriptor: response = 0b1000;
            case FSAE_IndexNotSupported: response = 0b0100;
            case FSAE_NotEnoughMemory: response = 0b0010;
            case FSAE_EncodingUnsupported: response = 0b0001;
            case FSAE_None: response = 0b0000;
        }

        uint8_t buffer[1];
        buffer[0] = response;
        send(FRAGSESSION_PORT, buffer, 1);
    }

    /**
     * Handle a data fragment packet
     * @param buffer
     * @param length
     */
    LW_UC_STATUS handleDataFragment(uint8_t *buffer, size_t length) {
        // top 2 bits are the fragSessionIx, other 16 bits are the pkgIndex
        uint16_t indexAndN = (buffer[1] << 8) + buffer[0];

        uint8_t fragIx = indexAndN >> 14;
        uint16_t frameCounter = indexAndN & 16383;

        if (!frag_sessions[fragIx].active) return LW_UC_FRAG_SESSION_NOT_ACTIVE;
        if (!frag_sessions[fragIx].session) return LW_UC_FRAG_SESSION_NOT_ACTIVE;

        FragResult result = frag_sessions[fragIx].session->process_frame(frameCounter, buffer + 2, length - 2);

        if (result == FRAG_OK) {
            return LW_UC_OK;
        }

        if (result == FRAG_COMPLETE) {
            printf("FragSession complete\n");
            if (_event_cb) {
                _event_cb(LW_UC_EVENT_FRAGSESSION_COMPLETE);
            }

            // clear the session to re-claim memory
            if (frag_sessions[fragIx].session) {
                delete frag_sessions[fragIx].session;
            }

            LW_UC_STATUS authStatus = verifyAuthenticity(fragIx);

            // set back to inactive
            frag_sessions[fragIx].active = false;

            if (authStatus != LW_UC_OK) {
                return authStatus;
            }
            else {
                if (_event_cb) {
                    _event_cb(LW_UC_EVENT_FIRMWARE_READY);
                }

                return LW_UC_OK;
            }
        }

        printf("process_frame failed (%d)\n", result);
        return LW_UC_PROCESS_FRAME_FAILED;
    }

    /**
     * Verify the authenticity (SHA hash and ECDSA hash) of a firmware package
     *
     * @param fragIx Fragmentation slot index
     */
    LW_UC_STATUS verifyAuthenticity(uint8_t fragIx) {
        if (!frag_sessions[fragIx].active) return LW_UC_FRAG_SESSION_NOT_ACTIVE;

        // Options contain info on where the manifest is placed
        FragmentationSessionOpts_t opts = frag_sessions[fragIx].sessionOptions;

        // the signature is the last FOTA_SIGNATURE_LENGTH bytes of the package
        size_t signatureOffset = opts.FlashOffset + ((opts.NumberOfFragments * opts.FragmentSize) - opts.Padding) - FOTA_SIGNATURE_LENGTH;

        // Manifest to read in
        UpdateSignature_t header;
        if (_bd.read(&header, signatureOffset, FOTA_SIGNATURE_LENGTH) != BD_ERROR_OK) {
            return LW_UC_BD_READ_ERROR;
        }

        if (!compare_buffers(header.manufacturer_uuid, UPDATE_CERT_MANUFACTURER_UUID, 16)) {
            return LW_UC_SIGNATURE_MANUFACTURER_UUID_MISMATCH;
        }

        if (!compare_buffers(header.device_class_uuid, UPDATE_CERT_DEVICE_CLASS_UUID, 16)) {
            return LW_UC_SIGNATURE_DEVICECLASS_UUID_MISMATCH;
        }

        // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
        unsigned char sha_out_buffer[32];
        // Internal buffer for reading from BD
        uint8_t sha_buffer[LW_UC_SHA256_BUFFER_SIZE];

        // SHA256 requires a large buffer, alloc on heap instead of stack
        FragmentationSha256* sha256 = new FragmentationSha256(&_bd, sha_buffer, sizeof(sha_buffer));

        // The last FOTA_SIGNATURE_LENGTH bytes are reserved for the sig, so don't use it for calculating the SHA256 hash
        sha256->calculate(
            opts.FlashOffset,
            (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding - FOTA_SIGNATURE_LENGTH,
            sha_out_buffer);

        delete sha256;

        printf("SHA256 hash is: ");
        for (size_t ix = 0; ix < 32; ix++) {
            printf("%02x", sha_out_buffer[ix]);
        }
        printf("\n");

        // now check that the signature is correct...
        {
            printf("ECDSA signature is: ");
            for (size_t ix = 0; ix < header.signature_length; ix++) {
                printf("%02x", header.signature[ix]);
            }
            printf("\n");
            printf("Verifying signature...\n");

            // ECDSA requires a large buffer, alloc on heap instead of stack
            FragmentationEcdsaVerify* ecdsa = new FragmentationEcdsaVerify(UPDATE_CERT_PUBKEY, UPDATE_CERT_LENGTH);
            bool valid = ecdsa->verify(sha_out_buffer, header.signature, header.signature_length);
            if (!valid) {
                printf("Signature verification failed\n");
                return LW_UC_SIGNATURE_ECDSA_FAILED;
            }
            else {
                printf("Signature verification passed\n");
            }

            delete ecdsa;
        }

        return LW_UC_OK;
    }

    LW_UC_STATUS writeBootloaderHeader(FragmentationSessionOpts_t opts, unsigned char sha_hash[32]) {
        arm_uc_firmware_details_t details;

        // @todo: replace by real version
        details.version = static_cast<uint64_t>(MBED_BUILD_TIMESTAMP) + 1; // should be timestamp that the fw was built, this is to get around this
        details.size = (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding - FOTA_SIGNATURE_LENGTH;
        memcpy(details.hash, sha_hash, 32); // SHA256 hash of the firmware
        memset(details.campaign, 0, ARM_UC_GUID_SIZE); // todo, add campaign info
        details.signatureSize = 0; // not sure what this is used for

        uint8_t *fw_header_buff = (uint8_t*)malloc(ARM_UC_EXTERNAL_HEADER_SIZE_V2);
        if (!fw_header_buff) {
            printf("Could not allocate %d bytes for header\n", ARM_UC_EXTERNAL_HEADER_SIZE_V2);
            return LW_UC_OUT_OF_MEMORY;
        }

        arm_uc_buffer_t buff = { ARM_UC_EXTERNAL_HEADER_SIZE_V2, ARM_UC_EXTERNAL_HEADER_SIZE_V2, fw_header_buff };

        arm_uc_error_t err = arm_uc_create_external_header_v2(&details, &buff);

        if (err.error != ERR_NONE) {
            printf("Failed to create external header (%d)\n", err.error);
            return LW_UC_CREATE_BOOTLOADER_HEADER_FAILED;
        }

        int r = _bd.program(buff.ptr, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS, buff.size);
        if (r != BD_ERROR_OK) {
            printf("Failed to program firmware header: %d bytes at address 0x%x\n", buff.size, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS);
            return LW_UC_BD_WRITE_ERROR;
        }

        printf("Stored the update parameters in flash on 0x%x. Reset the board to apply update.\n", MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS);

        return LW_UC_OK;
    }

    /**
     * Relay message back to network server - to be provided by the caller of this client
     */
    void send(uint8_t port, uint8_t *data, size_t length) {
        _send_fn(port, data, length);
    }

    /**
     * Compare whether two buffers contain the same content
     */
    bool compare_buffers(uint8_t* buff1, const uint8_t* buff2, size_t size) {
        for (size_t ix = 0; ix < size; ix++) {
            if (buff1[ix] != buff2[ix]) return false;
        }
        return true;
    }

    /**
     * Helper function to print memory usage statistics
     */
    void print_heap_stats(uint8_t prefix = 0) {
        mbed_stats_heap_t heap_stats;
        mbed_stats_heap_get(&heap_stats);

        if (prefix != 0) {
            printf("%d ", prefix);
        }
        printf("Heap stats: %d / %d (max=%d)\n", heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
    }

    // store fragmentation groups here...
    FragmentationSessionParams_t frag_sessions[NB_FRAG_GROUPS];

    // external storage
    FragmentationBlockDeviceWrapper _bd;
    Callback<void(uint8_t, uint8_t*, size_t)> _send_fn;
    Callback<void(LW_UC_EVENT)> _event_cb;
};

#endif // _LORAWAN_UPDATE_CLIENT_H_
