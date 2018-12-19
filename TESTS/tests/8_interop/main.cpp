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

// Don't flash the actual firmware, because we're testing non-valid firmware
#undef  MBED_CONF_LORAWAN_UPDATE_CLIENT_INTEROP_TESTING
#define MBED_CONF_LORAWAN_UPDATE_CLIENT_INTEROP_TESTING 1

#include "mbed.h"
#include "packets.h"
#include "UpdateCerts.h"
#include "LoRaWANUpdateClient.h"
#include "FragmentationBlockDeviceWrapper.h"
#include "FragmentationCrc32.h"
#include "test_setup.h"
#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"

using namespace utest::v1;

static uint8_t INTEROP_PACKETS[][53] = {
    { 0x08, 0x01, 0x00, 0x6A, 0xB8, 0x00, 0x4D, 0x25, 0x18, 0x2F, 0x58, 0x65, 0x89, 0x6B, 0xAF, 0x34, 0xE0, 0x07, 0xAB, 0x6A, 0x8E, 0x24, 0x33, 0xCC, 0xF7, 0x50, 0xB1, 0xDF, 0xE4, 0x16, 0x0A, 0x2B, 0xE0, 0x19, 0x6B, 0xF4, 0x88, 0xB0, 0x50, 0xAF, 0xD5, 0x05, 0xBF, 0xFC, 0xBF, 0x48, 0xC9, 0x1A, 0x72, 0xE8, 0x4B, 0x49, 0x21 },
    { 0x08, 0x02, 0x00, 0x05, 0xAD, 0x36, 0x44, 0x7D, 0x0E, 0x92, 0x25, 0x96, 0xB2, 0x1A, 0x6A, 0xB1, 0x6A, 0x0D, 0x89, 0xA9, 0x83, 0xF1, 0x96, 0xE6, 0x23, 0x24, 0xCE, 0x65, 0x2A, 0xED, 0x59, 0xBF, 0xB9,0xE1, 0x9F, 0xBF, 0x59, 0x45, 0xE4, 0x6D, 0xF6, 0xA9, 0x9F, 0x1D, 0xF2, 0x73, 0x93, 0x68, 0x3C, 0xE6, 0x92, 0x01, 0x9D },
    { 0x08, 0x03, 0x00, 0x53, 0x86, 0xE2, 0x5B, 0xE8, 0x9F, 0x04, 0xED, 0xB0, 0xFE, 0x2C, 0x23, 0xEE, 0xB2, 0x11, 0xC1, 0xC0, 0xEB, 0xB5, 0x20, 0x05, 0x07, 0x07, 0x3F, 0xDB, 0x89, 0x8D, 0xD7, 0x20, 0x47, 0x95, 0xF7, 0x8F, 0x05, 0xCC, 0x3B, 0xCE, 0x63, 0xDC, 0xBF, 0x8E, 0x23, 0x0F, 0x1F, 0x0B, 0x1B, 0x3A, 0xB6, 0x8F, 0x03 },
    { 0x08, 0x04, 0x00, 0x12, 0xF7, 0x91, 0x34, 0x40, 0xBE, 0x32, 0x94, 0xF7, 0xD8, 0x3D, 0x7E, 0x9E, 0xD3, 0x28, 0x05, 0x12, 0x7C, 0x9B, 0x91, 0x51, 0xFC, 0x94, 0x61, 0x8C, 0xBE, 0xAB, 0x44, 0x11, 0x5E, 0xA1, 0x36, 0xC0, 0x11, 0x42, 0xCD, 0x31, 0xA3, 0x86, 0xEC, 0x43, 0x11, 0xBB, 0xC5, 0xE7, 0xEE, 0x04, 0x3C, 0x9D, 0xF2 },
    { 0x08, 0x05, 0x00, 0xF2, 0x8E, 0xE9, 0xA4, 0x63, 0x7C, 0x9A, 0x8C, 0xEC, 0xEA, 0x65, 0xF6, 0x2C, 0x20, 0x22, 0x81, 0x05, 0xF2, 0xD3, 0x04, 0x2D, 0x55, 0x21, 0xCE, 0x58, 0xF0, 0x94, 0xE0, 0xD7, 0xE7, 0x75, 0x8B, 0xCC, 0x49, 0x7D, 0x99, 0x04, 0x97, 0x6F, 0xCE, 0x50, 0xE4, 0x93, 0x2F, 0xC9, 0x9C, 0x0E, 0x6B, 0xAD, 0xEA },
    { 0x08, 0x06, 0x00, 0x00, 0xF9, 0x60, 0xF8, 0x9A, 0xD3, 0x93, 0xA0, 0x49, 0x96,0xBF, 0xDB, 0xC1, 0xB2, 0xDC, 0x52, 0xAB, 0x73, 0x61, 0x69, 0x66, 0x51, 0x9F, 0x6E, 0xF8, 0xAD, 0x33, 0x6D, 0x58, 0xCB, 0xE0, 0xE6, 0xA9, 0x45, 0x40, 0xDA, 0x87, 0xCD, 0x92, 0xBB, 0x84, 0xC5, 0x91, 0x77, 0x57, 0x11, 0x60, 0x14, 0xFB, 0x2E },
    { 0x08, 0x07, 0x00, 0xCF, 0xDF, 0xB0, 0x91, 0x29, 0x77, 0x58, 0x39, 0x97, 0x50, 0xEA, 0xE8, 0x42, 0x1C, 0x31, 0x7F, 0xBA, 0x35, 0x3F, 0xD9, 0x6A, 0x9D, 0x3C, 0x1A, 0x84, 0x7A, 0x27, 0x9F, 0x8B, 0xA7, 0x25, 0xC0, 0x39, 0x84, 0xC8, 0x06, 0x53, 0xDF, 0xD7, 0x89, 0xDD, 0xF2, 0xD3, 0xDA, 0x19, 0xA6, 0xB3, 0x9C, 0xCC, 0x09 },
    { 0x08, 0x08, 0x00, 0xC4, 0xBB, 0x42, 0x42, 0xA1, 0x58, 0xCB, 0x72, 0xC8, 0xFD, 0x4D, 0x24, 0xE6, 0x8A, 0xF9, 0xA2, 0xFD, 0x8B, 0x86, 0x23, 0x5B, 0x07, 0x29, 0xBE, 0x08, 0x5D, 0xDC, 0xB1, 0xB0, 0x30, 0x71,0x94, 0xFC, 0x34, 0x3F, 0x43, 0xBF, 0x75, 0x0F, 0x82, 0x36, 0xCC, 0x4C, 0x07, 0x97, 0xD7, 0x61, 0xBF, 0x82, 0x8A },
    { 0x08, 0x09, 0x00, 0xF5, 0xCD, 0x08, 0xB5, 0x77, 0xF2, 0x38, 0x44, 0x15, 0x6D, 0x1C, 0xA2, 0xCD, 0xB2, 0xC3, 0x57, 0xD8, 0x6D, 0xD2, 0xA0, 0x25, 0x14, 0x05, 0x11, 0x75, 0x1D, 0x07, 0xC0, 0x65, 0xBE, 0x73, 0x73, 0x7A, 0x79, 0xCD, 0x67, 0xE7, 0x09, 0xC5, 0x20, 0x9E, 0x03, 0x89, 0x01, 0xF3, 0xE7, 0xCB, 0xE9, 0x25, 0x28 },
    { 0x08, 0x0a, 0x00, 0x30, 0x9F, 0xE7, 0xFC, 0xB5, 0xBB, 0xE8, 0x66, 0x40, 0x2C, 0x1E, 0xCF, 0x25, 0x43, 0xD1, 0x4F, 0xFB, 0x44, 0x88, 0x50, 0xE8, 0x5D, 0x6F, 0x83, 0xEF, 0x08, 0xB7, 0xE3, 0x07, 0x85, 0x53, 0xDB, 0x8E, 0xB0, 0x73, 0xA0, 0x4A, 0x02, 0x93, 0x4F, 0x84, 0xEA, 0x6D, 0x3F, 0x5F, 0xEE, 0xEF, 0xD7, 0xEB, 0x3A },
    { 0x08, 0x0b, 0x00, 0x16, 0x3A, 0x50, 0x2D, 0x9B, 0x69, 0xD0, 0x2F, 0xB3, 0x3D, 0x92, 0x59, 0x0F, 0x3A, 0xA9, 0x7F, 0x84, 0x2D, 0x92, 0xFE, 0xD0, 0x98, 0xF9, 0xE6, 0x98, 0x08, 0x18, 0x11, 0x73, 0x60, 0xF9, 0x2B, 0xF8, 0xC4, 0xD2, 0xA1, 0xAB, 0x7A, 0x03, 0x5A, 0x7D, 0xBA, 0x78, 0x75, 0x23, 0x03, 0xC1, 0x52, 0xFB, 0x38 },
    { 0x08, 0x0c, 0x00, 0x56, 0x86, 0xC0, 0x76, 0x20, 0x50, 0x81, 0xAC, 0xC4, 0x21, 0x06,0x84, 0xCF, 0x03, 0xAB, 0xAF, 0x73, 0xE9, 0xA4, 0x01, 0x7C, 0xDB, 0xD4, 0xA6, 0xAC, 0x94, 0x46, 0x8F, 0xAB, 0x5A, 0xDA, 0x32, 0xBF, 0x4A, 0xC5, 0x6D, 0xCE, 0x5A, 0x36, 0xC4, 0x4F, 0xBB, 0xBE, 0x38, 0x37, 0x33, 0x24, 0x60, 0x07, 0x1C },
    { 0x08, 0x0d, 0x00, 0xAC, 0xCC, 0x15, 0x3B, 0x35, 0xEA, 0xB5, 0x8D, 0x4E, 0xD5, 0x6F, 0xEB, 0xB4, 0x7A, 0x20, 0xF9, 0x29, 0x34, 0x6E, 0x67, 0x25, 0xBA, 0x30, 0xA4, 0xC0, 0x36, 0x99, 0xBF, 0xA3, 0x98, 0x4B, 0xBB, 0xF1, 0x6D, 0xC7, 0x0E, 0xD5, 0x31, 0x65, 0x4D, 0x14, 0xE7, 0x5E, 0x87, 0x7E, 0x22, 0x35, 0x13, 0x82, 0x43 },
    { 0x08, 0x0e, 0x00, 0x5B, 0x1C, 0xC9, 0x1B, 0xFB, 0x2D, 0x92, 0x0B, 0xC9, 0x30, 0x87, 0xBD, 0x26, 0x8D, 0x37, 0xC2, 0xB8, 0x2D, 0xDC, 0x05, 0xDB, 0x8F, 0x67, 0xC1, 0xB7, 0xFC, 0x47, 0x01, 0xEE, 0xDB, 0xBA, 0x84,0xB4, 0xC7, 0x60, 0xC4, 0xBF, 0x9C, 0x66, 0xB2, 0x01, 0xC6, 0xE5, 0x3D, 0x1F, 0x38, 0x4D, 0xE1, 0x8B, 0x49 },
    { 0x08, 0x0f, 0x00, 0x23, 0x4A, 0x9D, 0x53, 0x75, 0x71, 0xD3, 0x6D, 0x58, 0xAC, 0x38, 0x77, 0x50, 0xA0, 0xE0, 0x72, 0xC8, 0x75, 0xA7, 0x22, 0x6E, 0xE8, 0x9A, 0xC4, 0x81, 0x7F, 0xD7, 0x11, 0x92, 0xF0, 0x84, 0x32, 0xD8, 0x40, 0xB3, 0x8A, 0xF2, 0x9F, 0xD6, 0x02, 0xFC, 0x14, 0x52, 0xF1, 0x02, 0xD2, 0xDC, 0x70, 0x41, 0xCD },
    { 0x08, 0x10, 0x00, 0x7A, 0x22, 0xED, 0xE4, 0x7D, 0xDA, 0x6B, 0xAE, 0x65, 0x81, 0x30, 0xF6, 0x4B, 0x1A, 0x25, 0x04, 0xB7, 0x90, 0xCB, 0x81, 0xCA, 0xB1, 0xC6, 0x68, 0xA5, 0x2E, 0x52, 0x2C, 0x68, 0x3E, 0x68, 0xF9, 0x52, 0xFB, 0xA2, 0x60, 0xDB, 0x9E, 0x40, 0xCA, 0x6E, 0x5B, 0x54, 0xB2, 0x45, 0xCE, 0x4B, 0x8B, 0x7C, 0xDA },
    { 0x08, 0x11, 0x00, 0xE3, 0x2F, 0x95, 0xE5, 0x72, 0xEB, 0x47, 0x9B, 0xAE, 0x3A, 0x04, 0x6A, 0xEF, 0x57, 0xC7, 0x2D, 0x57, 0x25, 0xB7, 0xB2, 0xB0, 0x41, 0xB1, 0x3A, 0x6C, 0x5F, 0x5B, 0x0F, 0xA1, 0xB4, 0x9C, 0xA5, 0x2B, 0x26, 0x83, 0xDF, 0x2F, 0x76, 0x6D, 0x7F, 0x29, 0x57, 0x43, 0xD7, 0xCC, 0x6D, 0x9B, 0x25, 0x82, 0x4C },
    { 0x08, 0x12, 0x00, 0xDB, 0xAB, 0xA2, 0x20, 0x78, 0xFC, 0xF2, 0xA4, 0x27, 0xA3, 0x90, 0x78,0x6D, 0x99, 0xD9, 0xC0, 0x94, 0xEC, 0x11, 0xFD, 0x0E, 0x33, 0x6C, 0x1B, 0x9F, 0x0C, 0x49, 0x10, 0xB3, 0xAA, 0x61, 0x30, 0xBE, 0x57, 0xCB, 0x7C, 0x86, 0x07, 0xA4, 0x59, 0x3A, 0x6F, 0x62, 0x78, 0xFA, 0x5D, 0xC5, 0x8D, 0xE3, 0x5B },
    { 0x08, 0x13, 0x00, 0x3F, 0xE8, 0x0B, 0xF2, 0x8E, 0x60, 0xFE, 0x0F, 0x84, 0x08, 0x92, 0x2E, 0xA1, 0xFA, 0xDF, 0x73, 0xB5, 0xC6, 0x7E, 0x87, 0x26, 0x5E, 0x24, 0xB9, 0x7A, 0x72, 0xE2, 0x87, 0x68, 0x45, 0x12, 0x6B, 0x07, 0x4A, 0x80, 0xF6, 0x1C, 0xAC, 0x7F, 0xC6, 0x25, 0x15, 0x66, 0xCB, 0x31, 0xC4, 0x4A, 0x37, 0x04, 0x66 },
    { 0x08, 0x14, 0x00, 0x61, 0xA8, 0x12, 0x27, 0x04, 0x1D, 0xA6, 0x67, 0x52, 0x8E, 0xFD, 0xD5, 0xB2, 0xEA, 0x0A, 0x12, 0x79, 0x59, 0xEF, 0x7D, 0x8A, 0xE4, 0x72, 0xE0, 0x41, 0x46, 0x54, 0x8C, 0x38, 0xAB, 0x24, 0x18, 0xDE,0x3C, 0x62, 0x92, 0x86, 0x13, 0xDF, 0xF3, 0xCF, 0x48, 0x87, 0x57, 0x8D, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x08, 0x15, 0x00, 0xEF, 0xAB, 0xB9, 0xC7, 0x19, 0x8E, 0xAE, 0x27, 0x53, 0xA4, 0x61, 0x4B, 0x68, 0x77, 0xA6, 0x02, 0x93, 0xE1, 0x78, 0x97, 0xBE, 0x65, 0xDD, 0x24, 0x8B, 0x39, 0x0E, 0xDE, 0x33, 0xA9, 0xDE, 0x51, 0xFE, 0xFA, 0x48, 0x6C, 0xC4, 0xAC, 0x0C, 0x6A, 0x8E, 0x08, 0x0B, 0xA7, 0x88, 0x74, 0xAA, 0x50, 0x10, 0x55 },
    { 0x08, 0x16, 0x00, 0xF3, 0x8C, 0xC7, 0x84, 0x90, 0xBD, 0x21, 0xDD, 0x3E, 0xBA, 0x88, 0x62, 0x16, 0x5B, 0xC5, 0xDC, 0x33, 0x84, 0x58, 0x78, 0x9E, 0x97, 0x73, 0xEA, 0x6F, 0x8C, 0xD4, 0x03, 0x48, 0x6E, 0x53, 0xE5, 0xBA, 0xD9, 0x97, 0xE9, 0x8E, 0x99, 0x37, 0x6F, 0x25, 0x2D, 0x2D, 0xC0, 0x27, 0x23, 0x2A, 0xB7, 0xCF, 0x0B },
    { 0x08, 0x17, 0x00, 0x76, 0xD6, 0xA0, 0x42, 0x9F, 0xFB, 0x60, 0x8F, 0x92, 0x51, 0xD1, 0x5E, 0x5F, 0x44, 0x51, 0xEC, 0x76, 0xB5, 0x99, 0xC2, 0x48, 0x3C, 0x9A, 0xAB, 0xD0, 0x81, 0x09, 0xDB, 0xA5, 0xB0, 0x83, 0x4A, 0xAC, 0x8C, 0x19, 0xD7, 0x69, 0x72, 0xB3, 0x04, 0xAA, 0xB0, 0xE4, 0x06, 0x13, 0xD9, 0x82, 0x90, 0xA9, 0x39 },
    { 0x08, 0x18, 0x00, 0x56, 0x18, 0xB7, 0xAD, 0x47, 0xB8, 0xC4, 0x23, 0xAC, 0xA2, 0x01, 0x11, 0x9C,0x82, 0xFE, 0x52, 0xE0, 0xE2, 0x4E, 0x35, 0xFC, 0xD1, 0xE4, 0x2D, 0xBC, 0x72, 0x52, 0xEF, 0xBF, 0x1F, 0x27, 0xFF, 0x17, 0xCC, 0x1F, 0xF6, 0xF7, 0xD1, 0x8A, 0xD6, 0x43, 0x79, 0xC8, 0xD9, 0x40, 0x97, 0x42, 0x9A, 0x9B, 0x7E },
    { 0x08, 0x19, 0x00, 0x42, 0x94, 0xFC, 0x49, 0x0E, 0x7B, 0x5F, 0xFE, 0xD7, 0x6A, 0x4F, 0xB0, 0xED, 0x2D, 0x15, 0x39, 0x20, 0xC5, 0x64, 0x97, 0x53, 0x95, 0xA6, 0x2D, 0xDE, 0x51, 0xD4, 0x7A, 0x74, 0xCB, 0x52, 0x69, 0xF0, 0xD3, 0xA9, 0x7A, 0x6E, 0x62, 0x81, 0x1B, 0xEE, 0x6C, 0xC8, 0x58, 0xA5, 0xE3, 0x07, 0x2D, 0x9D, 0x7E }
};

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

static void lorawan_uc_firmware_ready(uint32_t) {
    is_fw_ready = true;

    printf("Firmware is ready\n");
}

static void setup() {
    mbed_trace_init();

    // !!! THESE FUNCTIONS RUN IN AN ISR !!!
    // !!! DO NOT DO BLOCKING THINGS IN THEM !!!
    uc.callbacks.switchToClassA = switch_to_class_a;
    uc.callbacks.switchToClassC = switch_to_class_c;

    // These run in the context that calls the update client
    uc.callbacks.fragSessionComplete = lorawan_uc_fragsession_complete;
    uc.callbacks.firmwareReady = lorawan_uc_firmware_ready;
}

static control_t interop_test(const size_t call_count) {
    LW_UC_STATUS status;

    uc.outOfBandClockSync(10);

    // create new group and start a MC request
    const uint8_t mc_setup_header[] = {
        0x02, 0x00,
        0xFF, 0xFF, 0xFF, 0x01, // McAddr
        0x01, 0x5E, 0x85, 0xF4, 0xB9, 0x9D, 0xC0, 0xB9, 0x44, 0x06, 0x6C, 0xD0, 0x74, 0x98, 0x33, 0x0B, //McKey_encrypted
        0x0, 0x0, 0x0, 0x0, // minFCnt
        0xff, 0x0, 0x0, 0x0 // maxFCnt
    };
    status = uc.handleMulticastControlCommand((uint8_t*)mc_setup_header, sizeof(mc_setup_header));
    TEST_ASSERT_EQUAL(LW_UC_OK, status);

    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 0x02);
    TEST_ASSERT_EQUAL(last_message.data[1], 0x0);

    // create fragmentation group
    const uint8_t frag_setup_header[] = {
        0x2, 0x0, 20, 0x0, 50, 0x0, 0x10, 0x01, 0x02, 0x03, 0x04
    };
    status = uc.handleFragmentationCommand(0x0, (uint8_t*)frag_setup_header, sizeof(frag_setup_header));
    TEST_ASSERT_EQUAL(LW_UC_OK, status);

    TEST_ASSERT_EQUAL(last_message.port, 201);
    TEST_ASSERT_EQUAL(last_message.length, 2);
    TEST_ASSERT_EQUAL(last_message.data[0], 0x02);
    TEST_ASSERT_EQUAL(last_message.data[1], 0x0);

    const uint8_t mc_start_header[] = {
        0x4,
        0x0, // mcgroupidheader
        12 & 0xff, 0, 0, 0, // 2sec.
        0x07, // session timeout
        0xd2, 0xad, 0x84, // dlfreq
        0x5 // dr
    };
    status = uc.handleMulticastControlCommand((uint8_t*)mc_start_header, sizeof(mc_start_header));
    TEST_ASSERT_EQUAL(last_message.port, 200);
    TEST_ASSERT_EQUAL(last_message.length, 5);
    TEST_ASSERT_EQUAL(last_message.data[0], 0x04);
    TEST_ASSERT_EQUAL(last_message.data[1], 0x00);
    TEST_ASSERT_EQUAL(last_message.data[2], 0x02);
    TEST_ASSERT_EQUAL(last_message.data[3], 0x00);
    TEST_ASSERT_EQUAL(last_message.data[4], 0x00);

    wait_ms(2100);

    TEST_ASSERT_EQUAL(in_class_c, true);
    TEST_ASSERT_EQUAL(class_c.downlinkFreq, 869525000);
    TEST_ASSERT_EQUAL(class_c.datarate, 5);
    printf("DevAddr is %d\n", class_c.deviceAddr);
    TEST_ASSERT_EQUAL(class_c.deviceAddr, 0x01FFFFFF);

    const uint8_t expected_nwkskey[] = { 0xBB, 0x75, 0xC3, 0x62, 0x58, 0x8F, 0x5D, 0x65, 0xFC, 0xC6, 0x1C, 0x08, 0x0B, 0x76, 0xDB, 0xA3 };
    const uint8_t expected_appskey[] = { 0xC3, 0xF6, 0xC3, 0x9B, 0x6B, 0x64, 0x96, 0xC2, 0x96, 0x29, 0xF7, 0xE7, 0xE9, 0xB0, 0xCD, 0x29 };

    TEST_ASSERT_EQUAL(compare_buffers(class_c.nwkSKey, expected_nwkskey, 16), true);
    TEST_ASSERT_EQUAL(compare_buffers(class_c.appSKey, expected_appskey, 16), true);

    // OK, start sending packets, but with some packet loss
    // Process the frames in the INTEROP_PACKETS array
    for (size_t ix = 0; ix < sizeof(INTEROP_PACKETS) / sizeof(INTEROP_PACKETS[0]); ix++) {
        if (is_complete) break;

        TEST_ASSERT_EQUAL(in_class_c, true);

        bool lose_packet = false;
        if (ix == 2 || ix == 9) lose_packet = true;

        if (lose_packet) {
            printf("Lost frame %d\n", ix);
            continue;
        }

        status = uc.handleFragmentationCommand(0x01ffffff, (uint8_t*)INTEROP_PACKETS[ix], sizeof(INTEROP_PACKETS[0]));

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

static control_t verify_crc_in_slot0(const size_t call_count) {
    FragmentationBlockDeviceWrapper fbd(&bd);
    int r = fbd.init();
    TEST_ASSERT_EQUAL(r, 0);

    uint8_t crc_buffer[128];

    FragmentationCrc32 crc32(&fbd, crc_buffer, 128);
    uint32_t crc = crc32.calculate(MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS, 995);
    printf("CRC32 is %lu\n", crc);
    TEST_ASSERT_EQUAL(0xECB2A918, crc);

    return CaseNext;
}

static control_t verify_sha256_in_slot0(const size_t call_count) {
    FragmentationBlockDeviceWrapper fbd(&bd);
    fbd.init();

    // Calculate the SHA256 hash of the file, and then verify whether the signature was signed with a trusted private key
    unsigned char sha_out_buffer[32];
    // Internal buffer for reading from BD
    uint8_t sha_buffer[LW_UC_SHA256_BUFFER_SIZE];

    // SHA256 requires a large buffer, alloc on heap instead of stack
    FragmentationSha256* sha256 = new FragmentationSha256(&fbd, sha_buffer, sizeof(sha_buffer));

    uint32_t flashOffset = MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS;
    uint32_t flashLength = 995;

    sha256->calculate(flashOffset, flashLength, sha_out_buffer);

    delete sha256;

    // output from shasum -a 256 interop-test-file.bin
    const uint8_t expected[] = {
        0x79, 0x41, 0xc5, 0xe8, 0x85, 0x12, 0x84, 0x56, 0x7b, 0xca, 0xf9, 0x7a, 0x43, 0xdb, 0xc6, 0x3b,
        0x61, 0x7c, 0x9e, 0x55, 0x9b, 0x3b, 0x88, 0x97, 0xe8, 0xf8, 0x8f, 0xb9, 0xf3, 0x7d, 0x7d, 0xbc
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

    return CaseNext;
}

utest::v1::status_t greentea_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(5*60, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    Case("interop_test", interop_test),
    Case("verify_crc_in_slot0", verify_crc_in_slot0),
    Case("verify_sha256_in_slot0", verify_sha256_in_slot0)
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
