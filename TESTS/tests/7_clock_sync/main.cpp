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
static void fake_send_method(LoRaWANUpdateClientSendParams_t&);

const uint8_t APP_KEY[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

LoRaWANUpdateClient uc(&bd, APP_KEY, fake_send_method);

typedef struct {
    uint8_t port;
    uint8_t data[255];
    size_t length;
} send_message_t;

static send_message_t last_message;
static bool in_class_c = false;

void switch_to_class_a() {
    in_class_c = false;
}

void switch_to_class_c(LoRaWANUpdateClientClassCSession_t*) {
    in_class_c = true;
}

static void fake_send_method(LoRaWANUpdateClientSendParams_t &params) {
    last_message.port = params.port;
    memcpy(last_message.data, params.data, params.length);
    last_message.length = params.length;
}

static uint64_t gpsTime = 1214658125; // Tue Jul 03 2018 21:02:35 GMT+0800
LW_UC_STATUS status;

void setup() {
    uc.outOfBandClockSync(gpsTime);
    // !!! THESE FUNCTIONS RUN IN AN ISR !!!
    // !!! DO NOT DO BLOCKING THINGS IN THEM !!!
    uc.callbacks.switchToClassA = switch_to_class_a;
    uc.callbacks.switchToClassC = switch_to_class_c;

    wait_ms(2000); // 2 seconds delay to make sure the clock is forward
}

static control_t manual_clock_sync(const size_t call_count) {
    setup();

    status = uc.requestClockSync(true);
    TEST_ASSERT_EQUAL(status, LW_UC_OK);
    TEST_ASSERT_EQUAL(last_message.port, 202);
    TEST_ASSERT_EQUAL(last_message.length, 6);
    TEST_ASSERT_EQUAL(last_message.data[0], 1);
    TEST_ASSERT_EQUAL(last_message.data[5], 0b10000);

    uint32_t curr_time = (last_message.data[4] << 24) + (last_message.data[3] << 16) + (last_message.data[2] << 8) + last_message.data[1];
    TEST_ASSERT(curr_time - gpsTime < 4);

    return CaseNext;
}

static control_t response_should_adjust_time(const size_t call_count) {
    // device clock is 2400 seconds too fast
    int32_t adjust = -2400;

    uint8_t header[] = { 1, adjust & 0xff, (adjust >> 8) & 0xff, (adjust >> 16) & 0xff, (adjust >> 24) & 0xff, 0b0000 /* tokenAns */ };
    status = uc.handleClockSyncCommand(header, sizeof(header));
    TEST_ASSERT_EQUAL(status, LW_UC_OK);

    uint64_t currTime = uc.getCurrentTime_s();
    TEST_ASSERT(currTime <= gpsTime);

    TEST_ASSERT(gpsTime - currTime >= 2395 && gpsTime - currTime <= 2405);

    return CaseNext;
}

static control_t response_should_up_tokenans(const size_t call_count) {
    status = uc.requestClockSync(false);
    TEST_ASSERT_EQUAL(status, LW_UC_OK);
    TEST_ASSERT_EQUAL(last_message.data[5], 0b00001);

    return CaseNext;
}

static control_t should_handle_forcedevicesyncreq(const size_t call_count) {

    uint8_t header[] = { 3, 0b001 /* nbTrans, not implemented yet */ };
    status = uc.handleClockSyncCommand(header, sizeof(header));
    TEST_ASSERT_EQUAL(status, LW_UC_OK);
    TEST_ASSERT_EQUAL(last_message.port, 202);
    TEST_ASSERT_EQUAL(last_message.length, 6);
    TEST_ASSERT_EQUAL(last_message.data[0], 1);
    TEST_ASSERT_EQUAL(last_message.data[5], 0b00001); // still the same as no reply was had yet

    uint32_t curr_time = (last_message.data[4] << 24) + (last_message.data[3] << 16) + (last_message.data[2] << 8) + last_message.data[1];
    TEST_ASSERT(curr_time - (gpsTime - 2400) < 5);

    return CaseNext;
}

static control_t should_update_mc_during_clocksync(const size_t call_count) {
    uc.outOfBandClockSync(gpsTime);

    // create new group and start a MC request
    const uint8_t setup_header[] = { 0x2, 0b00,
        0x3e, 0xaa, 0x24, 0x18, /* mcaddr */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* mcKey_Encrypted */
        0x3, 0x0, 0x0, 0x0, /* minFcCount */
        0x2, 0x10, 0x0, 0x0 /* maxFcCount */
    };
    uc.handleMulticastControlCommand((uint8_t*)setup_header, sizeof(setup_header));

    uint32_t timeToStart = static_cast<uint32_t>(gpsTime + 100);
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

    // after 2 seconds we should not be switched to class C...
    wait_ms(2100);

    TEST_ASSERT_EQUAL(in_class_c, false);

    // so now, send an adjustment of +96 seconds and the MC group should start 2 seconds later...
    int32_t adjust = 96;

    // device needs to trigger this to make sure the request is valid
    uc.requestClockSync(false);

    uint8_t adjustHeader[] = { 1, adjust & 0xff, (adjust >> 8) & 0xff, (adjust >> 16) & 0xff, (adjust >> 24) & 0xff, 0b0001 /* tokenAns */ };
    status = uc.handleClockSyncCommand(adjustHeader, sizeof(adjustHeader));

    wait_ms(2100);

    TEST_ASSERT_EQUAL(in_class_c, true);

    return CaseNext;
}


utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(5*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("manual_clock_sync", manual_clock_sync),
    Case("response_should_adjust_time", response_should_adjust_time),
    Case("response_should_up_tokenans", response_should_up_tokenans),
    Case("should_handle_forcedevicesyncreq", should_handle_forcedevicesyncreq),
    Case("should_update_mc_during_clocksync", should_update_mc_during_clocksync)
};

Specification specification(greentea_setup, cases);

void blink_led() {
    static DigitalOut led(LED1);
    led = !led;
}

int main() {
    Ticker t;
    t.attach(blink_led, 0.2);

    mbed_trace_init();

    return !Harness::run(specification);
}
