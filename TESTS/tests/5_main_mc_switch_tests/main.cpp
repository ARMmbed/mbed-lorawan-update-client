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
static LoRaWANUpdateClientClassCSession_t class_c;
static bool in_class_c = false;

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

static void switch_to_class_a() {
    in_class_c = false;
}

static void switch_to_class_c(LoRaWANUpdateClientClassCSession_t* session) {
    memcpy(&class_c, session, sizeof(LoRaWANUpdateClientClassCSession_t));

    in_class_c = true;
}

// difference between UTC epoch and GPS epoch is 315964800 seconds
static uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800 - yes, it's 9PM in Bali and I'm writing code

static void setup() {
    uc.outOfBandClockSync(gpsTime);

    // !!! THESE FUNCTIONS RUN IN AN ISR !!!
    // !!! DO NOT DO BLOCKING THINGS IN THEM !!!
    uc.callbacks.switchToClassA = switch_to_class_a;
    uc.callbacks.switchToClassC = switch_to_class_c;

    // create new group and start a MC request
    const uint8_t setup_header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    uc.handleMulticastControlCommand((uint8_t*)setup_header, sizeof(setup_header));
}

static control_t start_active_classc_session_request(const size_t call_count) {
    uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 2);
    uint32_t freq = 869525000 / 100;

    // start mc session
    const uint8_t header[] = { 0x4, 0b00,
        timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
        2 /* timeOut (2^8) */,
        freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
        3 /* data rate */
    };
    uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    TEST_ASSERT_EQUAL(in_class_c, false);

    // after 2 seconds we should be switched to class C...
    wait_ms(2100);

    // @todo: i just hard coded these based on the output so it doesn't actually test anything
    uint8_t expectedNwkSKey[] = { 0xa8, 0x9a, 0xab, 0xa7, 0xc8, 0x0b, 0x9b, 0xc4, 0x53, 0xae, 0x1e, 0x6f, 0x03, 0x7b, 0x8c, 0x9a };
    uint8_t expectedAppSKey[] = { 0xad, 0x1a, 0x3d, 0x18, 0xe3, 0x0e, 0x0b, 0xb3, 0x66, 0x68, 0x68, 0xb0, 0x6c, 0x8a, 0x52, 0x4d };

    TEST_ASSERT_EQUAL(in_class_c, true);
    TEST_ASSERT_EQUAL(class_c.deviceAddr, 0x1824aa3e);
    TEST_ASSERT_EQUAL(class_c.downlinkFreq, freq * 100);
    TEST_ASSERT_EQUAL(class_c.datarate, 3);
    TEST_ASSERT_EQUAL(compare_buffers(class_c.nwkSKey, expectedNwkSKey, 16), true);
    TEST_ASSERT_EQUAL(compare_buffers(class_c.appSKey, expectedAppSKey, 16), true);

    // wait another 2 seconds, and it should not have timed out
    wait_ms(2000);

    TEST_ASSERT_EQUAL(in_class_c, true);

    // wait another 2 seconds, now it should have timed out
    wait_ms(2100);

    TEST_ASSERT_EQUAL(in_class_c, false);

    return CaseNext;
}

static control_t getting_datafragments_should_reset(const size_t call_count) {
    // now we're 6 seconds into the future
    gpsTime += 6;

    uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 2);
    uint32_t freq = 869525000 / 100;

    // start mc session
    const uint8_t header[] = { 0x4, 0b00,
        timeToStart & 0xff, (timeToStart >> 8) & 0xff, (timeToStart >> 16) & 0xff, (timeToStart >> 24) & 0xff,
        2 /* timeOut (2^8) */,
        freq & 0xff, (freq >> 8) & 0xff, (freq >> 16) & 0xff,
        3 /* data rate */
    };
    uc.handleMulticastControlCommand((uint8_t*)header, sizeof(header));

    // after 2 seconds we should be switched to class C...
    wait_ms(2100);

    TEST_ASSERT_EQUAL(in_class_c, true);

    // now handle a data fragment (the actual value can be bogus)
    uint8_t dataFragPacket[] = { 0x8, 0x3, 0x3, 0x3 };
    LW_UC_STATUS status = uc.handleFragmentationCommand(0x1824aa3e, dataFragPacket, sizeof(dataFragPacket));

    TEST_ASSERT_EQUAL(status, LW_UC_FRAG_SESSION_NOT_ACTIVE);

    // wait another 3.5 seconds, and it should not have timed out
    wait_ms(3500);

    TEST_ASSERT_EQUAL(in_class_c, true);

    // wait another second, now it should have timed out
    wait_ms(1000);

    TEST_ASSERT_EQUAL(in_class_c, false);

    return CaseNext;
}

utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(5*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("start_active_classc_session_request", start_active_classc_session_request),
    Case("getting_datafragments_should_reset", getting_datafragments_should_reset)
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
