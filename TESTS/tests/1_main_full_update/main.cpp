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

#include "mbed.h"
#include "packets.h"
#include "UpdateCerts.h"
#include "LoRaWANUpdateClient.h"
#include "FragmentationBlockDeviceWrapper.h"
#include "test_setup.h"
#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"

using namespace utest::v1;

// fwd declaration
static void fake_send_method(LoRaWANUpdateClientSendParams_t &params);

const uint8_t APP_KEY[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

LoRaWANUpdateClient uc(&bd, APP_KEY, fake_send_method);

// Print heap statistics
static void print_heap_stats(uint8_t prefix = 0) {
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);

    if (prefix != 0) {
        printf("%d ", prefix);
    }
    printf("Heap stats: %d / %d (max=%d)\n", heap_stats.current_size, heap_stats.reserved_size, heap_stats.max_size);
}

static bool is_complete = false;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    printf("Sending %u bytes on port %u: ", params.length, params.port);
    for (size_t ix = 0; ix < params.length; ix++) {
        printf("%02x ", params.data[ix]);
    }
    printf("\n");
}

static void lorawan_uc_fragsession_complete() {
    is_complete = true;
}

static void lorawan_uc_firmware_ready() {
    printf("Firmware is ready - reset the device to flash new firmware...\n");
}

static control_t full_update(const size_t call_count) {
    LW_UC_STATUS status;

    uc.callbacks.fragSessionComplete = lorawan_uc_fragsession_complete;
    uc.callbacks.firmwareReady = lorawan_uc_firmware_ready;

    status = uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));
    TEST_ASSERT_EQUAL(LW_UC_OK, status);

    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        if (is_complete) break;

        status = uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS[ix], sizeof(FAKE_PACKETS[0]));

        TEST_ASSERT_EQUAL(LW_UC_OK, status);

        if (is_complete) {
            break;
        }

        printf("Processed frame %d\n", ix);

        wait_ms(20); // @todo: this is really weird, writing these in quick succession leads to corrupt image... need to investigate.
    }

    return CaseNext;
}

static control_t verify_firmware_in_slot0(const size_t call_count) {
    FragmentationBlockDeviceWrapper fbd(&bd);
    fbd.init();

    // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
    unsigned char sha_out_buffer[32];
    // Internal buffer for reading from BD
    uint8_t sha_buffer[LW_UC_SHA256_BUFFER_SIZE];

    // SHA256 requires a large buffer, alloc on heap instead of stack
    FragmentationSha256* sha256 = new FragmentationSha256(&fbd, sha_buffer, sizeof(sha_buffer));

    uint32_t flashOffset = MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS;
    uint32_t flashLength = 7884;

    sha256->calculate(flashOffset, flashLength, sha_out_buffer);

    delete sha256;

    // output from shasum -a 256 xdot-l151cc-blinky-application.bin
    const uint8_t expected[] = {
        0xc3, 0x02, 0x91, 0x1f, 0x65, 0x15, 0x8a, 0x3a, 0xfd, 0x33, 0x01, 0xa3, 0xa4, 0x22, 0xd5, 0x5c,
        0x58, 0x31, 0xf6, 0x96, 0x09, 0x2b, 0x24, 0x65, 0xea, 0x9b, 0xa6, 0x38, 0x3f, 0x50, 0xf7, 0x50
    };

    printf("Expected: ");
    for (uint8_t ix = 0; ix < 32; ix++) {
        printf("%02x ", expected[ix]);
    }
    printf("\n");

    printf("Actual: ");
    for (uint8_t ix = 0; ix < 32; ix++) {
        printf("%02x ", sha_out_buffer[ix]);
    }
    printf("\n");

    TEST_ASSERT_EQUAL(true, compare_buffers(sha_out_buffer, expected, 32));

    // @todo should also parse the FW header in slot 0

    return CaseNext;
}

utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(5*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("full update", full_update),
    Case("verify firmware", verify_firmware_in_slot0)
};

Specification specification(greentea_setup, cases);

void blink_led() {
    static DigitalOut led(LED1);
    led = !led;
}

int main() {
    Ticker t;
    t.attach(blink_led, 0.5);

    mbed_trace_init();

    return !Harness::run(specification);
}
