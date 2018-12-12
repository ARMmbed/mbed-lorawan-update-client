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

static control_t invalid_length(const size_t call_count) {
    LW_UC_STATUS status;
    const uint8_t header[] = { 0x2, 0x0, 0x28 };
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_INVALID_PACKET_LENGTH, status);

    return CaseNext;
}

static control_t invalid_index(const size_t call_count) {
    LW_UC_STATUS status;

    // frag index 4 is invalid, can only have one...
    const uint8_t header[] = { 0x2, 0b00110000, 0x28, 0x0, 0xcc, 0x0, 0xa7, 0x0, 0x0, 0x0, 0x0 };
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(201, last_message.port);
    TEST_ASSERT_EQUAL(2, last_message.length);
    TEST_ASSERT_EQUAL(0x2, last_message.data[0]);
    TEST_ASSERT_EQUAL(0b0100, last_message.data[1]);

    return CaseNext;
}

static control_t create_session(const size_t call_count) {
    LW_UC_STATUS status;

    status = uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS_HEADER, sizeof(FAKE_PACKETS_HEADER));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(201, last_message.port);
    TEST_ASSERT_EQUAL(2, last_message.length);
    TEST_ASSERT_EQUAL(0x2, last_message.data[0]);
    TEST_ASSERT_EQUAL(0b0000, last_message.data[1]);

    return CaseNext;
}

static control_t get_status(const size_t call_count) {
    LW_UC_STATUS status;

    // alright now send 3 packets (2 missing, 0 and 3) and see what status we can read...
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS[1], sizeof(FAKE_PACKETS[0]));
    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS[2], sizeof(FAKE_PACKETS[0]));
    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    uc.handleFragmentationCommand(0x0, (uint8_t*)FAKE_PACKETS[4], sizeof(FAKE_PACKETS[0]));
    TEST_ASSERT_EQUAL(LW_UC_OK, status);

    // [7..3] = RFU, 00 = fragIx (which should be active), 1 = all participants
    const uint8_t header[] = { 0x1, 0b00000001 };
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(201, last_message.port);
    TEST_ASSERT_EQUAL(5, last_message.length);
    TEST_ASSERT_EQUAL(0x1, last_message.data[0]);
    // 1&2 is 2 byte field, upper 2 bits should be the ix; then 3 received messages in total
    TEST_ASSERT_EQUAL(0b00000000, last_message.data[1]);
    TEST_ASSERT_EQUAL(3, last_message.data[2]);
    // missing frag field
    TEST_ASSERT_EQUAL(2, last_message.data[3]);
    // status field... 7..1 RFU, 0 should be 1 only if out of memory
    TEST_ASSERT_EQUAL(0, last_message.data[4]);

    return CaseNext;
}

static control_t delete_session(const size_t call_count) {
    LW_UC_STATUS status;
    const uint8_t header[] = { 0x3, 0 };
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);

    TEST_ASSERT_EQUAL(201, last_message.port);
    TEST_ASSERT_EQUAL(2, last_message.length);
    TEST_ASSERT_EQUAL(3, last_message.data[0]);
    // bit 2 = sessionNotExists flag, bit [1..0] is fragIx
    TEST_ASSERT_EQUAL(0b00000000, last_message.data[1]);

    return CaseNext;
}

static control_t delete_invalid_session(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x3, 0b10 };
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(201, last_message.port);
    TEST_ASSERT_EQUAL(2, last_message.length);
    TEST_ASSERT_EQUAL(3, last_message.data[0]);
    // bit 2 = sessionNotExists flag, bit [1..0] is fragIx
    TEST_ASSERT_EQUAL(0b00000110, last_message.data[1]);

    return CaseNext;
}

static control_t get_package_version(const size_t call_count) {
    LW_UC_STATUS status;

    const uint8_t header[] = { 0x0 };
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(LW_UC_OK, status);
    TEST_ASSERT_EQUAL(201, last_message.port);
    TEST_ASSERT_EQUAL(3, last_message.length);
    TEST_ASSERT_EQUAL(0, last_message.data[0]);
    TEST_ASSERT_EQUAL(3, last_message.data[1]);
    TEST_ASSERT_EQUAL(1, last_message.data[2]);

    return CaseNext;
}

utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(5*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("invalid_length", invalid_length),
    Case("invalid_index", invalid_index),
    Case("create_session", create_session),
    Case("get_status", get_status),
    Case("delete_session", delete_session),
    Case("delete_invalid_session", delete_invalid_session),
    Case("get_package_version", get_package_version)
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
