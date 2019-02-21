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

#include "FragmentationSession.h"

#include "mbed_trace.h"
#define TRACE_GROUP "FSES"

FragmentationSession::FragmentationSession(FragBDWrapper* flash, FragmentationSessionOpts_t opts)
    : _flash(flash), _opts(opts),
        _math(flash, opts.NumberOfFragments, opts.FragmentSize, opts.RedundancyPackets, opts.FlashOffset),
        _frames_received(0)
{
    tr_debug("FragmentationSession starting:");
    tr_debug("\tNumberOfFragments:   %d", opts.NumberOfFragments);
    tr_debug("\tFragmentSize:        %d", opts.FragmentSize);
    tr_debug("\tPadding:             %d", opts.Padding);
    tr_debug("\tMaxRedundancy:       %d", opts.RedundancyPackets);
    tr_debug("\tFlashOffset:         0x%x", opts.FlashOffset);
}

FragmentationSession::~FragmentationSession()
{
    tr_debug("dtor");
}

FragResult FragmentationSession::initialize() {
    if (_flash->init() != BD_ERROR_OK) {
        tr_warn("Could not initialize FragBDWrapper");
        return FRAG_NO_MEMORY;
    }

    // initialize the memory required for the Math module
    if (!_math.initialize()) {
        tr_warn("Could not initialize FragmentationMath");
        return FRAG_NO_MEMORY;
    }

    return FRAG_OK;
}

FragResult FragmentationSession::process_frame(uint16_t index, uint8_t* buffer, size_t size) {
    if (size != _opts.FragmentSize) return FRAG_SIZE_INCORRECT;
    if (index == 0) return FRAG_INDEX_INCORRECT;

    _frames_received++;

    // the first X packets contain the binary as-is... If that is the case, just store it in flash.
    // index is 1-based
    if (index <= _opts.NumberOfFragments) {
        int r = _flash->program(buffer, _opts.FlashOffset + ((index - 1) * size), size);
        if (r != 0) {
            return FRAG_FLASH_WRITE_ERROR;
        }

        _math.set_frame_found(index);

        if (index == _opts.NumberOfFragments && _math.get_lost_frame_count() == 0) {
            return FRAG_COMPLETE;
        }

        return FRAG_OK;
    }

    // redundancy packet coming in
    FragmentationMathSessionParams_t params;
    params.NbOfFrag = _opts.NumberOfFragments;
    params.Redundancy = _opts.RedundancyPackets;
    params.DataSize = _opts.FragmentSize;
    int r = _math.process_redundant_frame(index, buffer, params);
    if (r != FRAG_SESSION_ONGOING) {
        return FRAG_COMPLETE;
    }

    return FRAG_OK;
}

int FragmentationSession::get_lost_frame_count() {
    return _math.get_lost_frame_count();
}

uint16_t FragmentationSession::get_received_frame_count() {
    return _frames_received;
}

FragmentationSessionOpts_t FragmentationSession::get_options() {
    return _opts;
}
