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

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_UPDATE_TYPES
#define _MBED_LORAWAN_UPDATE_CLIENT_UPDATE_TYPES

#include "mbed.h"
#include "FragmentationSession.h"

#ifndef FRAGSESSION_PORT
#define FRAGSESSION_PORT 201
#endif

#ifndef MCCONTROL_PORT
#define MCCONTROL_PORT   200
#endif

#ifndef CLOCKSYNC_PORT
#define CLOCKSYNC_PORT   202
#endif

#define PACKAGE_VERSION_REQ 0x0
#define PACKAGE_VERSION_ANS 0x0

#define MC_GROUP_STATUS_REQ 0x01
#define MC_GROUP_STATUS_ANS 0x01
#define MC_GROUP_SETUP_REQ  0x02
#define MC_GROUP_SETUP_ANS  0x02
#define MC_GROUP_DELETE_REQ 0x03
#define MC_GROUP_DELETE_ANS 0x03
#define MC_CLASSC_SESSION_REQ  0x04
#define MC_CLASSC_SESSION_ANS  0x04

#define MC_GROUP_SETUP_REQ_LENGTH 29
#define MC_GROUP_SETUP_ANS_LENGTH 2
#define MC_GROUP_DELETE_REQ_LENGTH 1
#define MC_GROUP_DELETE_ANS_LENGTH 2
#define MC_GROUP_STATUS_REQ_LENGTH 1
#define MC_CLASSC_SESSION_REQ_LENGTH 10
#define MC_CLASSC_SESSION_ANS_LENGTH 5

#define FRAG_SESSION_STATUS_REQ 0x01
#define FRAG_SESSION_STATUS_ANS 0x01
#define FRAG_SESSION_SETUP_REQ  0x02
#define FRAG_SESSION_SETUP_ANS  0x02
#define FRAG_SESSION_DELETE_REQ 0x03
#define FRAG_SESSION_DELETE_ANS 0x03
#define DATA_BLOCK_AUTH_REQ  0x05
#define DATA_BLOCK_AUTH_ANS  0x05
#define DATA_FRAGMENT  0x08

#define PACKAGE_VERSION_REQ_LENGTH 0
#define PACKAGE_VERSION_ANS_LENGTH 3
#define FRAG_SESSION_SETUP_REQ_LENGTH 10
#define FRAG_SESSION_SETUP_ANS_LENGTH 2
#define FRAG_SESSION_DELETE_REQ_LENGTH 1
#define FRAG_SESSION_DELETE_ANS_LENGTH 2
#define FRAG_SESSION_STATUS_REQ_LENGTH 1
#define FRAG_SESSION_STATUS_ANS_LENGTH 5

#define CLOCK_APP_TIME_REQ 0x1
#define CLOCK_APP_TIME_ANS 0x1
#define CLOCK_APP_TIME_PERIODICITY_REQ 0x2
#define CLOCK_APP_TIME_PERIODICITY_ANS 0x2
#define CLOCK_FORCE_RESYNC_REQ 0x3

#define CLOCK_APP_TIME_REQ_LENGTH 6
#define CLOCK_APP_TIME_ANS_LENGTH 5
#define CLOCK_APP_TIME_PERIODICITY_REQ_LENGTH 1
#define CLOCK_APP_TIME_PERIODICITY_ANS_LENGTH 6
#define CLOCK_FORCE_RESYNC_REQ_LENGTH 1

#define FRAGMENTATION_ON_GOING 0xFFFFFFFF
#define FRAGMENTATION_NOT_STARTED 0xFFFFFFFE
#define FRAGMENTATION_FINISH 0x0

typedef struct {
    /**
     * SessionTime is the start of the Class C window, and is expressed as the time in
     * seconds since 00:00:00, Sunday 6th of January 1980 (start of the GPS epoch) modulo 2^32.
     * Note that this is the same format as the Time field in the beacon frame.
     */
    uint32_t sessionTime;

    /**
     * TimeOut encodes the maximum length in seconds of the multicast session
     * (max time the end-device stays in class C before reverting to class A to save battery)
     * This is a maximum duration because the end-device’s application might decide to revert
     * to class A before the end of the session, this decision is application specific.
     */
    uint32_t timeOut;

    /**
     * Frequency used for the multicast in Hz.
     * Values representing frequencies below 100 MHz are reserved for future use.
     * This allows setting the frequency of a channel
     * anywhere between 100 MHz to 1.67 GHz in 100 Hz steps.
     */
	uint32_t dlFreq;

    /**
     * index of the data rate used for the multicast.
     * Uses the same look-up table than the one used by the LinkAdrReq MAC command of the LoRaWAN protocol.
     */
    uint8_t dr;

} McClassCSessionParams_t;

typedef struct {

    /**
     * Whether the group is active
     */
    bool active;

    /**
     * McAddr is the multicast group network address.
     * McAddr is negotiated off-band by the application server with the network server.
     */
    uint32_t mcAddr;

    /**
     * McKey_encrypted is the encrypted multicast group key from which McAppSKey and McNetSKey will be derived.
     * The McKey_encrypted key can be decrypted using the following operation to give the multicast group’s McKey.
     * McKey = aes128_encrypt(McKEKey, McKey_encrypted)
     */
    uint8_t mcKey_Encrypted[16];

    /**
     * Network session key (derived from mcKey_Encrypted)
     */
    uint8_t nwkSKey[16];

    /**
     * Application session key (derived from mcKey_Encrypted)
     */
    uint8_t appSKey[16];

    /**
     * The minMcFCount field is the next frame counter value of the multicast downlink to be sent by the server
     * for this group. This information is required in case an end-device is added to a group that already exists.
     * The end-device MUST reject any downlink multicast frame using this group multicast address if the frame
     * counter is < minMcFCount.
     */
    uint32_t minFcFCount;

    /**
     * maxMcFCount specifies the life time of this multicast group expressed as a maximum number of frames.
     * The end-device will only accept a multicast downlink frame if the 32bits frame counter value
     * minMcFCount ≤ McFCount < maxMcFCount.
     */
    uint32_t maxFcFCount;

    /**
     * Class C session parameters
     */
    McClassCSessionParams_t params;

    /**
     * Timeout to start the multicast session
     */
#if defined (DEVICE_LPTICKER)
    LowPowerTimeout startTimeout;
#else
    Timeout startTimeout;
#endif

    /**
     * Ticker for the timeout, will switch out of the multicast session
     */
#if defined (DEVICE_LPTICKER)
    LowPowerTimeout timeoutTimeout;
#else
    Timeout timeoutTimeout;
#endif

} MulticastGroupParams_t;

typedef struct {
    /**
     * Whether the session is active
     */
    bool active;

    /**
     * McGroupBitMask specifies which multicast group addresses are allowed as input to this
     * defragmentation session. Bit number X indicates if multicast group with McGroupID=X
     * is allowed to feed fragments to the defragmentation session.
     * Unicast can always be used as a source for the defragmentation session and cannot be disabled.
     * For example, 4’b0000 means that only Unicast can be used with this fragmentation session.
     * 4’b0001 means the defragmentation layer MAY receive packets from the multicast group with
     * McGroupID=0 and the unicast address. 4’b1111 means that any of the 4 multicast groups or unicast
     * may be used. If the end-device does not support multicast, this field SHALL be ignored.
     */
    uint8_t mcGroupBitMask;

    /**
     * specifies the total number of fragments of the data block to be transported during the
     * coming multicast fragmentation session.
     */
    uint16_t nbFrag;

    /**
     * is the size in byte of each fragment.
     */
    uint8_t fragSize;

    /**
     * encodes the type of fragmentation algorithm used.
     * This parameter is simply passed to the fragmentation algorithm.
     */
    uint8_t fragAlgo;

    /**
     * encodes the amplitude of the random delay that end-devices have to wait
     * between the reception of a downlink command sent using multicast and the
     * transmission of their answer.
     * This parameter is a function of the group size and the geographic spread
     * and is used to avoid too many collisions on the uplink due to many end-devices
     * simultaneously answering the same command.
     * The actual delay SHALL be rand().2^(BlockAckDelay+4) seconds where rand() is a
     * random number in the [0:1] interval.
     */
    uint8_t blockAckDelay;

    /**
     * The descriptor field is a freely allocated 4 bytes field describing the file that
     * is going to be transported through the fragmentation session.
     * For example, this field MAY be used by the end-device to decide where to store the
     * defragmented file, how to treat it once received, etc...
     * If the file transported is a FUOTA binary image, this field might encode the version
     * of the firmware transported to allow end-device side compatibility verifications.
     * The encoding of this field is application specific.
     */
    uint32_t descriptor;

    /**
     * The maximum number of redundancy packets that can be expected
     * (not part of the incoming protocol, but will derive from a macro)
     */
    uint16_t redundancy;

    /**
     * The binary data block size may not be a multiple of FragSize.
     * Therefore, some padding bytes MUST be added to fill the last fragment.
     * This field encodes the number of padding byte used. Once the data block has been reconstructed
     * by the receiver, it SHALL remove the last "padding" bytes in order to get the original binary file.
     */
    uint8_t padding;

    /**
     * Options for the fragmentation session, such as where in flash to store the data
     */
    FragmentationSessionOpts_t sessionOptions;

    /**
     * Actual fragmentation session, which manages all the memory for this session
     */
    FragmentationSession* session;

} FragmentationSessionParams_t;

typedef struct {
    /**
     * The correction that needs to be applied to the RTC value
     */
    int32_t correction;

    /**
     * The current time during the last clock sync request
     */
    uint64_t rtcValueAtLastRequest;
} ClockSync_t;

enum FragmenationSessionAnswerErrors {
    FSAE_WrongDescriptor = 3,
    FSAE_IndexNotSupported = 2,
    FSAE_NotEnoughMemory = 1,
    FSAE_EncodingUnsupported = 0,
    FSAE_None = -1
};

// Parameters for the send structure, this is sent to user application
typedef struct {

    /**
     * The port to send the message on
     */
    uint8_t port;

    /**
     * Message buffer, note that this buffer is only valid during the initial callback.
     * If you need the buffer afterwards, make your own copy.
     */
    uint8_t *data;

    /**
     * Length of the message buffer
     */
    size_t length;

    /**
     * Whether the message needs to be sent as a confirmed message
     */
    bool confirmed;

    /**
     * Whether this message may be retried, and if so, how many times.
     * If this value is set to FALSE, the user application SHALL first temporarily disable
     * ADR and set NbTrans=1 before transmitting this message, then revert
     * the MAC layer to the previous state.
     * If this value is set to TRUE, and confirmed=True, use the number of confirmed retries
     * defined in the user application.
     */
    bool retriesAllowed;

    /**
     * Timestamp when this message was created.
     * If for some reason the application layer cannot send immediately it could re-calculate
     * package content (e.g. timeToStart for Class C group).
     * Not a perfect solution, as this requires the application layer to know about the message format,
     * but will be fixed properly when DevTimeReq and multicast are implemented in the MAC layer.
     */
    uint32_t createdTimestamp;

} LoRaWANUpdateClientSendParams_t;

// Parameters for the Class C session, to be handled by the user application when the
// multicast session starts
typedef struct {

    /**
     * Multicast group network address.
     */
    uint32_t deviceAddr;

    /**
     * Network session key.
     */
    uint8_t nwkSKey[16];

    /**
     * Application session key.
     */
    uint8_t appSKey[16];

    /**
     * The minMcFCount field is the next frame counter value of the multicast downlink to be sent by the server
     * for this group. This information is required in case an end-device is added to a group that already exists.
     * The end-device MUST reject any downlink multicast frame using this group multicast address if the frame
     * counter is < minMcFCount.
     */
    uint32_t minFcFCount;

    /**
     * maxMcFCount specifies the life time of this multicast group expressed as a maximum number of frames.
     * The end-device will only accept a multicast downlink frame if the 32bits frame counter value
     * minMcFCount ≤ McFCount < maxMcFCount.
     */
    uint32_t maxFcFCount;

    /**
     * Frequency used for the multicast in Hz.
     */
	uint32_t downlinkFreq;

    /**
     * Index of the data rate used for the multicast.
     * Uses the same look-up table than the one used by the LinkAdrReq MAC command of the LoRaWAN protocol.
     */
    uint8_t datarate;

} LoRaWANUpdateClientClassCSession_t;

typedef struct {

    /**
     * Callback when a fragmentation session is complete
     */
    Callback<void()> fragSessionComplete;

    /**
     * Firmware has been written and is ready, callback to restart the device to apply the update
     */
#if MBED_CONF_LORAWAN_UPDATE_CLIENT_INTEROP_TESTING == 1
    // during interop tests we need the crc32
    Callback<void(uint32_t)> firmwareReady;
#else
    Callback<void()> firmwareReady;
#endif

    /**
     * Switch to Class C callback.
     * **Note: This runs in an ISR!**
     */
    Callback<void(LoRaWANUpdateClientClassCSession_t*)> switchToClassC;

    /**
     * Switch to Class A callback
     * **Note: This runs in an ISR!**
     */
    Callback<void()> switchToClassA;

    /**
     * Callback fired when verification of the firmware is starting.
     * Use this as a memory pressure event, as it requires 5.5K of heap space.
     */
    Callback<void()> verificationStarting;

    /**
     * Callback fired when verification of the firmware is finished.
     * Use this as a reverse memory pressure event, as it freed 5.5K of heap space.
     */
    Callback<void()> verificationFinished;

} LoRaWANUpdateClientCallbacks_t;

#endif // _MBED_LORAWAN_UPDATE_CLIENT_UPDATE_TYPES
