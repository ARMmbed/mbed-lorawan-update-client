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

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_FRAGMENTATION_SESSION
#define _MBED_LORAWAN_UPDATE_CLIENT_FRAGMENTATION_SESSION

#include "mbed.h"
#include "FragBDWrapper.h"
#include "FragmentationMath.h"
#include "mbed_debug.h"

/**
 * The binary is laid out like this:
 * First, the original binary, split by FragmentSize. Padding should indicate when the binary stops (offset from end of last fragment).
 * Then, redundency packets
 */
typedef struct {
    uint16_t NumberOfFragments; // Number of fragments required for the *initial* binary, not counting the redundancy packets
    uint8_t  FragmentSize;      // Size of each fragment in bytes, **without the fragindex**
    uint8_t  Padding;           // Bytes of padding after the last original fragment
    uint16_t RedundancyPackets; // Max. number of redundancy packets we'll receive
    size_t   FlashOffset;       // Place in flash where the final binary needs to be placed
} FragmentationSessionOpts_t;

enum FragResult {
    FRAG_OK,
    FRAG_SIZE_INCORRECT,
    FRAG_FLASH_WRITE_ERROR,
    FRAG_NO_MEMORY,
    FRAG_INDEX_INCORRECT,
    FRAG_COMPLETE
};

/**
 * Sets up a fragmentation session
 */
class FragmentationSession {
public:
    /**
     * Start a fragmentation session
     * @param flash A block device that is wrapped for unaligned operations
     * @param opts  List of options for this session
     */
    FragmentationSession(FragBDWrapper* flash, FragmentationSessionOpts_t opts);

    ~FragmentationSession();

    /**
     * Allocate the required buffers for the fragmentation session, and clears the flash pages required for the binary file.
     *
     * @returns FRAG_OK if succeeded,
     *          FRAG_NO_MEMORY if allocations failed,
     *          FRAG_FLASH_WRITE_ERROR if clearing the flash failed.
    */
    FragResult initialize();

    /**
     * Process a fragmentation frame. Do **not** include the fragindex bytes
     * @param index The index of the frame
     * @param buffer The contents of the frame (without the fragindex bytes)
     * @param size The size of the buffer
     *
     * @returns FRAG_COMPLETE if the binary was reconstructed,
     *          FRAG_OK if the packet was processed, but the binary was not reconstructed,
     *          FRAG_FLASH_WRITE_ERROR if the packet could not be written to flash
     */
    FragResult process_frame(uint16_t index, uint8_t* buffer, size_t size);

    /**
     * Convert a FragResult to a string
     */
    static const char* frag_result_string(FragResult result) {
        switch (result) {
            case FRAG_SIZE_INCORRECT: return "Fragment size incorrect";
            case FRAG_FLASH_WRITE_ERROR: return "Writing to flash failed";
            case FRAG_INDEX_INCORRECT: return "Index is not correct";
            case FRAG_NO_MEMORY: return "Not enough space on the heap";
            case FRAG_COMPLETE: return "Complete";

            case FRAG_OK: return "OK";
            default: return "Unkown FragResult";
        }
    }

    /**
     * Get the number of lost fragments
     */
    int get_lost_frame_count();

    /**
     * Get number of frames received (in total)
     */
    uint16_t get_received_frame_count();

    FragmentationSessionOpts_t get_options();

private:
    FragBDWrapper* _flash;
    FragmentationSessionOpts_t _opts;
    FragmentationMath _math;

    uint16_t _frames_received;
};

#endif // _MBED_LORAWAN_UPDATE_CLIENT_FRAGMENTATION_SESSION
