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

typedef struct {
    uint8_t port;
    uint8_t data[255];
    size_t length;
} send_message_t;

static send_message_t last_message;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

// difference between UTC epoch and GPS epoch is 315964800 seconds
static uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800 - yes, it's 9PM in Bali and I'm writing code

static control_t get_package_version(const size_t call_count) {
    LW_UC_STATUS status;

    uc.outOfBandClockSync(gpsTime);

    const uint8_t header[] = { 0x0 };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(status, LW_UC_OK);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 3);
    TEST_ASSERT_EQUAL(last_message.data[0], 0);
    TEST_ASSERT_EQUAL(last_message.data[1], 2);
    TEST_ASSERT_EQUAL(last_message.data[2], 1);

    return CaseNext;
}

static control_t invalid_length(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
    };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(status, LW_UC_INVALID_PACKET_LENGTH);

    return CaseNext;
}

static control_t invalid_index(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x2, 0b01,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(status, LW_UC_OK);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 2);
    TEST_ASSERT_EQUAL(last_message.data[1], 0b101);

    return CaseNext;
}

static control_t create_mc_group(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 2);
    TEST_ASSERT_EQUAL(last_message.data[1], 0); // status should be 0b000 (first bit is error status, last is group)

    return CaseNext;
}

static control_t delete_invalid_group(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x3, 0b10 };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 3);
    TEST_ASSERT_EQUAL(last_message.data[1], 0b110);

    return CaseNext;
}

static control_t delete_valid_group(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x3, 0b00 };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 3);
    TEST_ASSERT_EQUAL(last_message.data[1], 0);

    return CaseNext;
}

static control_t delete_already_deleted_group(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x3, 0b00 };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 3);
    TEST_ASSERT_EQUAL(last_message.data[1], 0b100);

    return CaseNext;
}

static control_t get_status_no_groups_active(const size_t call_count) {
    LW_UC_STATUS status;

    // get the status for all mc groups
    const uint8_t header[] = { 0x1, 0b1111 };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 1);
    TEST_ASSERT_EQUAL(last_message.data[1], 0);

    return CaseNext;
}

static control_t get_status_active_group(const size_t call_count) {
    LW_UC_STATUS status;

    // create new group
    const uint8_t setup_header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    uc.handleMulticastControlCommand((uint8_t*)setup_header, sizeof(setup_header));

    // get the status for all mc groups
    const uint8_t header[] = { 0x1, 0b1111 };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 7);
    TEST_ASSERT_EQUAL(last_message.data[0], 1);
    TEST_ASSERT_EQUAL(last_message.data[1], 0b010001); // 01 = number of groups, 0001 is the group ma?
    TEST_ASSERT_EQUAL(last_message.data[2], 0);
    TEST_ASSERT_EQUAL(last_message.data[3] == 0x3e && last_message.data[4] == 0xaa && last_message.data[5] == 0x24 && last_message.data[6] == 0x18, 1);

    return CaseNext;
}

static control_t start_inactive_classc_request(const size_t call_count) {
    LW_UC_STATUS status;

    // start in ~5 seconds (actually a bit less because we ran the other tests before)
    uint32_t timeToStart = static_cast<uint32_t>((gpsTime + 5) % static_cast<uint64_t>(pow(2.0f, 32.0f)));
    uint32_t freq = 869525000 / 100;

    // start mc session
    const uint8_t header[] = { 0x4, 0b10,
        timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
        8 /* timeOut (2^8) */,
        freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff, 3 };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 4);
    TEST_ASSERT_EQUAL(last_message.data[1], 0b10010); // mc session undefined for 0b10

    return CaseNext;
}

static control_t start_active_classc_request(const size_t call_count) {
    LW_UC_STATUS status;

    // start in ~5 seconds (actually a bit less because we ran the other tests before)
    uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 5);
    uint32_t freq = 869525000 / 100;

    // start mc session
    const uint8_t header[] = { 0x4, 0b00,
        timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
        8 /* timeOut (2^8) */,
        freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
        3 /* data rate */
    };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 5);
    TEST_ASSERT_EQUAL(last_message.data[0], 4);
    TEST_ASSERT_EQUAL(last_message.data[1], 0); // no error

    // we set our tts to 5 seconds after program start, so this should always be under 5
    uint32_t timeToStartResp = (last_message.data[4] << 16) + (last_message.data[3] << 8) + (last_message.data[2]);
    TEST_ASSERT(timeToStartResp <= 5);
    TEST_ASSERT(timeToStartResp != 0);

    return CaseNext;
}


static control_t session_in_past_should_start_directly(const size_t call_count) {
    LW_UC_STATUS status;

    uint32_t timeToStart = static_cast<uint32_t>(gpsTime - 10);
    uint32_t freq = 869525000 / 100;

    // start mc session
    const uint8_t header[] = { 0x4, 0b00,
        timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
        8 /* timeOut (2^8) */,
        freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
        3 /* data rate */
    };
    status = uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 5);
    TEST_ASSERT_EQUAL(last_message.data[0], 4);
    TEST_ASSERT_EQUAL(last_message.data[1], 0); // no error

    uint32_t timeToStartResp = (last_message.data[4] << 16) + (last_message.data[3] << 8) + (last_message.data[2]);
    TEST_ASSERT_EQUAL(timeToStartResp, 0);

    return CaseNext;
}

utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(5*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("get_package_version", get_package_version),
    Case("invalid_length", invalid_length),
    Case("invalid_index", invalid_index),
    Case("create_mc_group", create_mc_group),
    Case("delete_invalid_group", delete_invalid_group),
    Case("delete_valid_group", delete_valid_group),
    Case("delete_already_deleted_group", delete_already_deleted_group),
    Case("get_status_no_groups_active", get_status_no_groups_active),
    Case("get_status_active_group", get_status_active_group),
    Case("start_inactive_classc_request", start_inactive_classc_request),
    Case("start_active_classc_request", start_active_classc_request),
    Case("session_in_past_should_start_directly", session_in_past_should_start_directly)
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
