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

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_UPDATE_CLIENT
#define _MBED_LORAWAN_UPDATE_CLIENT_UPDATE_CLIENT

#include "mbed.h"
#include "mbed_delta_update.h"
#include "mbed_stats.h"
#include "BDFile.h"
#include "FragmentationSha256.h"
#include "FragmentationEcdsaVerify.h"
#include "FragBDWrapper.h"
#include "FragmentationCrc32.h"
#include "arm_uc_metadata_header_v2.h"
#include "update_signature.h"
#include "update_types.h"
#include "tiny-aes.h"   // @todo: replace by Mbed TLS / hw crypto?

#if !MBED_CONF_RTOS_PRESENT && !defined(TARGET_SIMULATOR)
#include "clock.h"
#endif

#include "mbed_trace.h"
#define TRACE_GROUP "LWUC"

#ifndef NB_FRAG_GROUPS
#define NB_FRAG_GROUPS          1
#endif // NB_FRAG_GROUPS

#ifndef NB_MC_GROUPS
#define NB_MC_GROUPS          1
#endif // NB_MC_GROUPS

#ifndef LW_UC_SHA256_BUFFER_SIZE
#define LW_UC_SHA256_BUFFER_SIZE       128
#endif // LW_UC_SHA256_BUFFER_SIZE

#ifndef LW_UC_JANPATCH_BUFFER_SIZE
#define LW_UC_JANPATCH_BUFFER_SIZE     528
#endif // LW_UC_JANPATCH_BUFFER_SIZE

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
    LW_UC_CREATE_BOOTLOADER_HEADER_FAILED = 11,
    LW_UC_INVALID_SLOT = 12,
    LW_UC_DIFF_SIZE_MISMATCH = 13,
    LW_UC_DIFF_INCORRECT_SLOT2_HASH = 14,
    LW_UC_DIFF_DELTA_UPDATE_FAILED = 15,
    LW_UC_INVALID_CLOCK_SYNC_TOKEN = 16,
    LW_UC_INTERNALFLASH_INIT_ERROR = 17,
    LW_UC_INTERNALFLASH_READ_ERROR = 18,
    LW_UC_INTERNALFLASH_DEINIT_ERROR = 19,
    LW_UC_INTERNALFLASH_SECTOR_SIZE_SMALLER = 20,
    LW_UC_INTERNALFLASH_HEADER_PARSE_FAILED = 21,
    LW_UC_NOT_CLASS_C_SESSION_ANS = 22
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
     * @param bd A block device
     * @param genAppKey Application Key, used to derive session keys from multicast keys
     * @param send_fn A send function, invoked when we want to relay data back to the network.
     *                Messages through this function should be sent as CONFIRMED uplinks.
     *                This is deliberatly not part of the callbacks structure, because it's a *required*
     *                part of the update client.
     */
    LoRaWANUpdateClient(BlockDevice *bd, const uint8_t genAppKey[16], Callback<void(LoRaWANUpdateClientSendParams_t&)> send_fn)
        : _bd(bd), _send_fn(send_fn), _lorawan(NULL)
    {
        // @todo: what if genAppKey is in secure element?
        memcpy(_genAppKey, genAppKey, 16);

        callbacks.fragSessionComplete = NULL;
        callbacks.firmwareReady = NULL;
        callbacks.switchToClassA = NULL;
        callbacks.switchToClassC = NULL;
    }

    /**
     * Initialize the update client
     *
     * @param lorawan The LoRaWANInterface before initialization
     * @returns LW_UC_OK when OK
     */
    LW_UC_STATUS initialize(LoRaWANInterface *lorawan) {
        _lorawan = lorawan;

        // Activate clock sync plugin
        _clk_sync_plugin.activate_clock_sync_package(callback(lorawan, &LoRaWANInterface::get_current_gps_time),
            callback(lorawan, &LoRaWANInterface::set_current_gps_time));

        // Activate multicast plugin with the GenAppKey
        _mcast_plugin.activate_multicast_control_package(_genAppKey, 16);

        // Multicast control package callbacks
        _mcast_cbs.switch_class = callback(this, &LoRaWANUpdateClient::switch_class);
        _mcast_cbs.check_params_validity = callback(lorawan, &LoRaWANInterface::verify_multicast_freq_and_dr);
        _mcast_cbs.get_gps_time = callback(lorawan, &LoRaWANInterface::get_current_gps_time);

        return LW_UC_OK;
    }

    /**
     * Handle packets that came in on the fragmentation port (e.g. 201)
     *
     * @param devAddr The device address that received this message (or 0x0 in unicast)
     * @param buffer Data buffer
     * @param length Length of the data buffer
     */
    LW_UC_STATUS handleFragmentationCommand(uint32_t devAddr, uint8_t *buffer, size_t length) {
        if (length == 0) return LW_UC_INVALID_PACKET_LENGTH;

        lorawan_rx_metadata md;
        _lorawan->get_rx_metadata(md);

        frag_ctrl_response_t *frag_resp = frag_plugin.parse(rx_buffer, retcode, flags, md.dev_addr,
                                            callback(this, &LoRaWANUpdateClient::bd_cb_handler),
                                            _lorawan->get_multicast_addr_register());

        uc.printHeapStats("Frag ");

        if (frag_resp != NULL && frag_resp->type == FRAG_CMD_RESP) {
            lora_uc_send(&frag_resp->cmd_ans);
        }

        if (frag_resp != NULL && frag_resp->type == FRAG_SESSION_STATUS && frag_resp->status == FRAG_SESSION_COMPLETE) {
            lorawan_uc_fragsession_complete(frag_resp->index);
        }
    }

    /**
     * Handle packets that came in on the multicast control port (e.g. 200)
     */
    LW_UC_STATUS handleMulticastControlCommand(uint8_t *buffer, size_t length) {
        if (length == 0) return LW_UC_INVALID_PACKET_LENGTH;

        mcast_ctrl_response_t *mcast_resp = mcast_plugin.parse(rx_buffer, retcode,
                                                               _lorawan->get_multicast_addr_register(),
                                                               &mcast_cbs,
                                                               true);

        if (mcast_resp) {
            lora_uc_send(mcast_resp);
        }

        return LW_UC_OK;
    }

    /**
     * Handle packets that came in on the multicast control port (e.g. 202)
     */
    LW_UC_STATUS handleClockSyncCommand(uint8_t *buffer, size_t length) {
        if (length == 0) return LW_UC_INVALID_PACKET_LENGTH;

        clk_sync_response_t *sync_resp = clk_sync_plugin.parse(rx_buffer, retcode);

        if (sync_resp) {
            lora_uc_send(sync_resp);
        }

        return LW_UC_OK;
    }

    /**
     * Helper function to print memory usage statistics
     */
    void printHeapStats(const char *prefix = "") {
        mbed_stats_heap_t heap_stats;
        mbed_stats_heap_get(&heap_stats);

        tr_info("%sHeap stats: %lu / %lu (max=%lu)", prefix, heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
    }

    /**
     * If the Class C Session Answer is sent later (e.g. due to duty cycle limitations)
     * call this function to update the timeToStart value
     * this is a hack, will be fixed properly when migrating multicast
     */
    LW_UC_STATUS updateClassCSessionAns(LoRaWANUpdateClientSendParams_t *queued_message) {
        if (queued_message->port != MCCONTROL_PORT || queued_message->length != MC_CLASSC_SESSION_ANS_LENGTH
                || queued_message->data[0] != MC_CLASSC_SESSION_ANS) {
            return LW_UC_NOT_CLASS_C_SESSION_ANS;
        }

        uint32_t originalTimeToStart = queued_message->data[2] + (queued_message->data[3] << 8) + (queued_message->data[4] << 16);

        // calculate delta between original send time and now
        uint32_t timeDelta = get_rtc_time_s() - queued_message->createdTimestamp;

        uint32_t timeToStart;
        if (timeDelta > originalTimeToStart) { // should already have started, send 0 back
            timeToStart = 0;
        }
        else {
            timeToStart = originalTimeToStart - timeDelta;
        }

        tr_debug("updateClassCSessionAns, originalTimeToStart=%lu, delta=%lu, newTimeToStart=%lu",
            originalTimeToStart, timeDelta, timeToStart);

        // update buffer
        queued_message->data[2] = timeToStart & 0xff;
        queued_message->data[3] = timeToStart >> 8 & 0xff;
        queued_message->data[4] = timeToStart >> 16 & 0xff;

        return LW_UC_OK;
    }

    /**
     * Callbacks to set that get invoked when state changes internally.
     *
     * This allows us to be independent of the underlying LoRaWAN stack.
     */
    LoRaWANUpdateClientCallbacks_t callbacks;

private:

    /**
     * Verify the authenticity (SHA hash and ECDSA hash) of a firmware package,
     * and after passing verification write the bootloader header
     *
     * @param addr Address of firmware slot (MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS or MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT1_HEADER_ADDRESS)
     * @param header Firmware manifest
     * @param flashOffset Offset in flash of the firmware
     * @param flashLength Length in flash of the firmware
     */
    LW_UC_STATUS verifyAuthenticityAndWriteBootloader(uint32_t addr, UpdateSignature_t *header, size_t flashOffset, size_t flashLength) {

        if (!compare_buffers(header->manufacturer_uuid, UPDATE_CERT_MANUFACTURER_UUID, 16)) {
            return LW_UC_SIGNATURE_MANUFACTURER_UUID_MISMATCH;
        }

        if (!compare_buffers(header->device_class_uuid, UPDATE_CERT_DEVICE_CLASS_UUID, 16)) {
            return LW_UC_SIGNATURE_DEVICECLASS_UUID_MISMATCH;
        }

        if (callbacks.verificationStarting) {
            callbacks.verificationStarting();
        }

        // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
        unsigned char sha_out_buffer[32];
        // Internal buffer for reading from BD
        uint8_t sha_buffer[LW_UC_SHA256_BUFFER_SIZE];

        // SHA256 requires a large buffer, alloc on heap instead of stack
        FragmentationSha256* sha256 = new FragmentationSha256(&_bd, sha_buffer, sizeof(sha_buffer));

        sha256->calculate(flashOffset, flashLength, sha_out_buffer);

        delete sha256;

        tr_debug("New firmware SHA256 hash is: ");
        for (size_t ix = 0; ix < 32; ix++) {
            printf("%02x", sha_out_buffer[ix]);
        }
        printf("\n");

        // now check that the signature is correct...
        {
            tr_debug("ECDSA signature is: ");
            for (size_t ix = 0; ix < header->signature_length; ix++) {
                printf("%02x", header->signature[ix]);
            }
            printf("\n");
            tr_debug("Verifying signature...");

            // ECDSA requires a large buffer, alloc on heap instead of stack
            FragmentationEcdsaVerify* ecdsa = new FragmentationEcdsaVerify(UPDATE_CERT_PUBKEY, UPDATE_CERT_LENGTH);
            bool valid = ecdsa->verify(sha_out_buffer, header->signature, header->signature_length);

            delete ecdsa;

            if (callbacks.verificationFinished) {
                callbacks.verificationFinished();
            }

            if (!valid) {
                tr_warn("New firmware signature verification failed");
                return LW_UC_SIGNATURE_ECDSA_FAILED;
            }
            else {
                tr_debug("New firmware signature verification passed");
            }
        }

        return writeBootloaderHeader(addr, header->version, flashLength, sha_out_buffer);
    }

    /**
     * Write the bootloader header so the firmware can be flashed
     *
     * @param addr Beginning of the firmware slot (e.g. MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS)
     * @param version Build timestamp of the firmware
     * @param fwSize Size of the firmware in bytes
     * @param sha_hash SHA256 hash of the firmware
     *
     * @returns LW_UC_OK if all went well, or non-0 status when something went wrong
     */
    LW_UC_STATUS writeBootloaderHeader(uint32_t addr, uint32_t version, size_t fwSize, unsigned char sha_hash[32]) {
        if (addr != MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS && addr != MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT1_HEADER_ADDRESS) {
            return LW_UC_INVALID_SLOT;
        }

        arm_uc_firmware_details_t details;

        // this is useful for tests, when the firmware is always older
#if MBED_CONF_LORAWAN_UPDATE_CLIENT_OVERWRITE_VERSION == 1
        // read internal flash page to see what version we're at
        uint64_t currVersion;
        LW_UC_STATUS status = getCurrentVersion(&currVersion);
        if (status != LW_UC_OK) {
            // fallback
            currVersion = (uint64_t)MBED_BUILD_TIMESTAMP;
        }
        details.version = currVersion + 1;
#else
        details.version = static_cast<uint64_t>(version);
#endif

        details.size = fwSize;
        memcpy(details.hash, sha_hash, 32); // SHA256 hash of the firmware
        memset(details.campaign, 0, ARM_UC_GUID_SIZE); // todo, add campaign info
        details.signatureSize = 0; // not sure what this is used for

        tr_debug("writeBootloaderHeader:\n\taddr: %lu\n\tversion: %llu\n\tsize: %llu", addr, details.version, details.size);

        uint8_t *fw_header_buff = (uint8_t*)malloc(ARM_UC_EXTERNAL_HEADER_SIZE_V2);
        if (!fw_header_buff) {
            tr_error("Could not allocate %d bytes for header", ARM_UC_EXTERNAL_HEADER_SIZE_V2);
            return LW_UC_OUT_OF_MEMORY;
        }

        arm_uc_buffer_t buff = { ARM_UC_EXTERNAL_HEADER_SIZE_V2, ARM_UC_EXTERNAL_HEADER_SIZE_V2, fw_header_buff };

        arm_uc_error_t err = arm_uc_create_external_header_v2(&details, &buff);

        if (err.error != ERR_NONE) {
            tr_error("Failed to create external header (%d)", err.error);
            free(fw_header_buff);
            return LW_UC_CREATE_BOOTLOADER_HEADER_FAILED;
        }

        int r = _bd.program(buff.ptr, addr, buff.size);
        if (r != BD_ERROR_OK) {
            tr_error("Failed to program firmware header: %lu bytes at address 0x%lx", buff.size, addr);
            free(fw_header_buff);
            return LW_UC_BD_WRITE_ERROR;
        }

        tr_debug("Stored the update parameters in flash on 0x%lx. Reset the board to apply update.", addr);

        free(fw_header_buff);

        return LW_UC_OK;
    }

#if MBED_CONF_LORAWAN_UPDATE_CLIENT_OVERWRITE_VERSION == 1
    /**
     * Get the current version number of the application from internal flash
     */
    LW_UC_STATUS getCurrentVersion(uint64_t* version) {
#if DEVICE_FLASH
        int r;
        if ((r = _internalFlash.init()) != 0) {
            tr_warn("Could not initialize internal flash (%d)", r);
            return LW_UC_INTERNALFLASH_INIT_ERROR;
        }

        uint32_t sectorSize = _internalFlash.get_sector_size(MBED_CONF_LORAWAN_UPDATE_CLIENT_INTERNAL_FLASH_HEADER);
        tr_debug("Internal flash sectorSize is %lu", sectorSize);

        if (sectorSize < ARM_UC_INTERNAL_HEADER_SIZE_V2) {
            tr_warn("SectorSize is smaller than ARM_UC_INTERNAL_HEADER_SIZE_V2 (%lu), cannot handle this", sectorSize);
            return LW_UC_INTERNALFLASH_SECTOR_SIZE_SMALLER;
        }

        uint8_t *buffer = (uint8_t*)malloc(sectorSize);
        if (!buffer) {
            tr_warn("getCurrentVersion() - Could not allocate %lu bytes", sectorSize);
            return LW_UC_OUT_OF_MEMORY;
        }

        if ((r = _internalFlash.read(buffer,  MBED_CONF_LORAWAN_UPDATE_CLIENT_INTERNAL_FLASH_HEADER, sectorSize)) != 0) {
            tr_warn("Read on internal flash failed (%d)", r);
            free(buffer);
            return LW_UC_INTERNALFLASH_READ_ERROR;
        }

        if ((r = _internalFlash.deinit()) != 0) {
            tr_warn("Could not de-initialize internal flash (%d)", r);
            free(buffer);
            return LW_UC_INTERNALFLASH_DEINIT_ERROR;
        }

        arm_uc_firmware_details_t details;

        arm_uc_error_t err = arm_uc_parse_internal_header_v2(const_cast<uint8_t*>(buffer), &details);
        if (err.error != ERR_NONE) {
            tr_warn("Internal header parsing failed (%d)", err.error);
            free(buffer);
            return LW_UC_INTERNALFLASH_HEADER_PARSE_FAILED;
        }

        *version = details.version;
        tr_debug("Version (from internal flash) is %llu", details.version);
        free(buffer);
        return LW_UC_OK;
#else
        *version = (uint64_t)MBED_BUILD_TIMESTAMP;
        return LW_UC_OK;
#endif
    }
#endif

    /**
     * Apply a delta update between slot 2 (source file) and slot 0 (diff file) and place in slot 1
     *
     * @param sizeOfFwInSlot0 Size of the diff image that we just received
     * @param sizeOfFwInSlot2 Expected size of firmware in slot 2 (will do sanity check)
     * @param sizeOfFwInSlot1 Out parameter which will be set to the size of the new firmware in slot 1
     */
    LW_UC_STATUS applySlot0Slot2DeltaUpdate(size_t sizeOfFwInSlot0, size_t sizeOfFwInSlot2, uint32_t *sizeOfFwInSlot1) {
        // read details about the current firmware, it's in the slot2 header
        arm_uc_firmware_details_t curr_details;
        int bd_status = _bd.read(&curr_details, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT2_HEADER_ADDRESS, sizeof(arm_uc_firmware_details_t));
        if (bd_status != BD_ERROR_OK) {
            return LW_UC_BD_READ_ERROR;
        }

        // so... sanity check, do we have the same size in both places
        if (sizeOfFwInSlot2 != curr_details.size) {
            tr_warn("Diff size mismatch, expecting %u (manifest) but got %llu (slot 2 content)", sizeOfFwInSlot2, curr_details.size);
            return LW_UC_DIFF_SIZE_MISMATCH;
        }

        // calculate sha256 hash for current fw & diff file (for debug purposes)
        {
            unsigned char sha_out_buffer[32];
            uint8_t sha_buffer[LW_UC_SHA256_BUFFER_SIZE];
            FragmentationSha256* sha256 = new FragmentationSha256(&_bd, sha_buffer, sizeof(sha_buffer));

            tr_debug("Firmware hash in slot 2 (current firmware): ");
            sha256->calculate(MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT2_FW_ADDRESS, sizeOfFwInSlot2, sha_out_buffer);
            print_buffer(sha_out_buffer, 32, false);
            printf("\n");

            tr_debug("Firmware hash in slot 2 (expected): ");
            print_buffer(curr_details.hash, 32, false);
            printf("\n");

            if (!compare_buffers(curr_details.hash, sha_out_buffer, 32)) {
                tr_info("Firmware in slot 2 hash incorrect hash");
                delete sha256;
                return LW_UC_DIFF_INCORRECT_SLOT2_HASH;
            }

            tr_debug("Firmware hash in slot 0 (diff file): ");
            sha256->calculate(MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS, sizeOfFwInSlot0, sha_out_buffer);
            print_buffer(sha_out_buffer, 32, false);
            printf("\n");

            delete sha256;
        }

        // now run the diff...
        BDFILE source(&_bd, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT2_FW_ADDRESS, sizeOfFwInSlot2);
        BDFILE diff(&_bd, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS, sizeOfFwInSlot0);
        BDFILE target(&_bd, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT1_FW_ADDRESS, 0);

        int v = apply_delta_update(&_bd, LW_UC_JANPATCH_BUFFER_SIZE, &source, &diff, &target);

        if (v != MBED_DELTA_UPDATE_OK) {
            tr_warn("apply_delta_update failed %d", v);
            return LW_UC_DIFF_DELTA_UPDATE_FAILED;
        }

        tr_debug("Patched firmware length is %ld", target.ftell());

        *sizeOfFwInSlot1 = target.ftell();

        return LW_UC_OK;
    }

    frag_bd_opts_t *bd_cb_handler(uint8_t frag_index, uint32_t desc) {
        // done in stack now, comment out for now
        // // clear out slot 0 and slot 1
        // int r = bd.init();
        // if (r != 0) {
        //     printf("Initializing block device failed (%d)\n", r);
        //     return NULL;
        // }
        // // if there's erase size misalignment between slot 0 / slot 1 / slot size this will fail
        // r = bd.erase(MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS, MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT_SIZE * 2);
        // if (r != 0) {
        //     printf("Erasing block device failed (%d), addr: %llu, size: %llu\n", r,
        //         static_cast<bd_size_t>(MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_HEADER_ADDRESS),
        //         static_cast<bd_size_t>(MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT_SIZE) * 2);
        //     return NULL;
        // }
        // printf("Erased slot 0 and slot 1\n");

        tr_debug("Creating storage layer for session %d with desc: %lu", frag_index, desc);

        bd_opts.redundancy_max = MBED_CONF_LORAWAN_UPDATE_CLIENT_MAX_REDUNDANCY;
        bd_opts.offset = MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS;
        bd_opts.fasm = &_assembler;
        bd_opts.bd = &_bd;

        return &bd_opts;
    }

    /**
     * Switch class function, called from the lorawan stack
     * @param device_class CLASS_A or CLASS_C
     * @param time_to_switch Time to switch in seconds
     * @param life_time Only valid for Class C, life time of the session
     * @param dr Only valid for Class C, data rate
     * @param dl_freq Only valid for Class C, downlink frequency
     */
    void switch_class(uint8_t device_class, uint32_t time_to_switch, uint32_t life_time, uint8_t dr, uint32_t dl_freq) {

    }

public:
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
     * Print the content of a buffer
     * @params buff Buffer
     * @params size Size of buffer
     * @params withSpace Whether to separate bytes by spaces
     */
    void print_buffer(void* buff, size_t size, bool withSpace = true) {
        for (size_t ix = 0; ix < size; ix++) {
            printf("%02x", ((uint8_t*)buff)[ix]);
            if (withSpace) {
                printf(" ");
            }
        }
    }

private:

#if DEVICE_FLASH
    FlashIAP _internalFlash;
#endif

    // external storage
    FragBDWrapper _bd;
    uint8_t _genAppKey[16];
    Callback<void(LoRaWANUpdateClientSendParams_t&)> _send_fn;

    ClockSyncControlPackage _clk_sync_plugin;
    MulticastControlPackage _mcast_plugin;
    FragmentationControlPackage _frag_plugin;
    mcast_controller_cbs_t _mcast_cbs;

    frag_bd_opts_t _bd_opts;
    FragAssembler _assembler;

    LoRaWANInterface *_lorawan;
};

#endif // _MBED_LORAWAN_UPDATE_CLIENT_UPDATE_CLIENT
