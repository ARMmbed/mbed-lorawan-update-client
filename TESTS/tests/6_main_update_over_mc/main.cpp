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

#undef MBED_CONF_LORAWAN_UPDATE_CLIENT_OVERWRITE_VERSION
#define MBED_CONF_LORAWAN_UPDATE_CLIENT_OVERWRITE_VERSION 0

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

typedef struct {
    uint8_t port;
    uint8_t data[255];
    size_t length;
} send_message_t;

static send_message_t last_message;
static LoRaWANUpdateClientClassCSession_t class_c;
static bool in_class_c = false;
static bool is_complete = false;
static bool is_fw_ready = false;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

static void switch_to_class_a() {
    in_class_c = false;
}

static void switch_to_class_c(LoRaWANUpdateClientClassCSession_t *session) {
    memcpy(&class_c, session, sizeof(LoRaWANUpdateClientClassCSession_t));

    in_class_c = true;
}

static void lorawan_uc_fragsession_complete() {
    is_complete = true;
}

static void lorawan_uc_firmware_ready() {
    is_fw_ready = true;

    printf("Firmware is ready\n");
}

// difference between UTC epoch and GPS epoch is 315964800 seconds
static uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800

static void setup() {
    mbed_trace_init();

    LW_UC_STATUS status;

#ifdef TARGET_SIMULATOR
    uint32_t curr_time = EM_ASM_INT({
        return Date.now();
    });
    srand(curr_time);
#endif

    uc.outOfBandClockSync(gpsTime);

    // !!! THESE FUNCTIONS RUN IN AN ISR !!!
    // !!! DO NOT DO BLOCKING THINGS IN THEM !!!
    uc.callbacks.switchToClassA = switch_to_class_a;
    uc.callbacks.switchToClassC = switch_to_class_c;

    // These run in the context that calls the update client
    uc.callbacks.fragSessionComplete = lorawan_uc_fragsession_complete;
    uc.callbacks.firmwareReady = lorawan_uc_firmware_ready;
}

static control_t full_update_over_mc(const size_t call_count) {
    LW_UC_STATUS status;

    // create new group and start a MC request
    const uint8_t mc_setup_header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    status = uc.handleMulticastControlCommand((uint8_t*)mc_setup_header, sizeof(mc_setup_header));
    TEST_ASSERT_EQUAL(LW_UC_OK, status);

    // create fragmentation group
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));

    // start MC session
    uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 2);
    uint32_t freq = 869525000 / 100;

    const uint8_t mc_start_header[] = { 0x4, 0b00,
        timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
        2 /* timeOut (2^8) */,
        freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
        3 /* data rate */
    };
    status = uc.handleMulticastControlCommand((uint8_t*)mc_start_header, sizeof(mc_start_header));

    wait_ms(2100);

    TEST_ASSERT_EQUAL(in_class_c, true);

    // OK, start sending packets with 12.5% packet loss
    // Process the frames in the FAKE_PACKETS array
    for (size_t ix = 0; ix < sizeof(FAKE_PACKETS) / sizeof(FAKE_PACKETS[0]); ix++) {
        if (is_complete) break;

        TEST_ASSERT_EQUAL(in_class_c, true);

        bool lose_packet = (rand() % 8) == 4;
        if (lose_packet) {
            printf("Lost frame %d\n", ix);
            continue;
        }

        status = uc.handleFragmentationCommand(0x1824aa3e, (uint8_t*)FAKE_PACKETS[ix], sizeof(FAKE_PACKETS[0]));

        TEST_ASSERT_EQUAL(LW_UC_OK, status);

        printf("Processed frame %d\n", ix);

        if (is_complete) {
            break;
        }

        wait_ms(50); // @todo: this is really weird, writing these in quick succession leads to corrupt image... need to investigate.
    }

    TEST_ASSERT_EQUAL(is_complete, true);
    TEST_ASSERT_EQUAL(is_fw_ready, true);
    TEST_ASSERT_EQUAL(in_class_c, false);

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
    Case("full_update_over_mc", full_update_over_mc),
    Case("verify_firmware_in_slot0", verify_firmware_in_slot0)
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
    setup();

    return !Harness::run(specification);
}
