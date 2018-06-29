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

#ifndef __PROTOCOLLAYER_H
#define __PROTOCOLLAYER_H

#include "mbed.h"
#include "FragmentationSession.h"

#define LORAWAN_APP_PROTOCOL_DATA_MAX_SIZE 100
#define LORAWAN_PROTOCOL_DEFAULT_DATARATE DR_5

#define FRAGSESSION_PORT 201
#define MCSETUP_PORT     202

#define PROTOCOL_VERSION_REQ 0x0
#define PROTOCOL_VERSION_ANS 0x0

#define MC_GROUP_STATUS_REQ 0x01
#define MC_GROUP_STATUS_ANS 0x01
#define MC_GROUP_SETUP_REQ  0x02
#define MC_GROUP_SETUP_ANS  0x02
#define MC_GROUP_DELETE_REQ 0x03
#define MC_GROUP_DELETE_ANS 0x03
#define MC_CLASSC_SESSION_REQ  0x04
#define MC_CLASSC_SESSION_ANS  0x04
#define MC_CLASSC_SESSION_REQ_LENGTH 0xa
#define MC_CLASSC_SESSION_ANS_LENGTH 0x5
#define FRAGMENTATION_ON_GOING 0xFFFFFFFF
#define FRAGMENTATION_NOT_STARTED 0xFFFFFFFE
#define FRAGMENTATION_FINISH 0x0
#define MAX_UPLINK_T0_UIFCNTREF 0x3

#define FRAG_SESSION_STATUS_REQ 0x01
#define FRAG_SESSION_STATUS_ANS 0x01
#define FRAG_SESSION_SETUP_REQ  0x02
#define FRAG_SESSION_SETUP_ANS  0x02
#define FRAG_SESSION_DELETE_REQ 0x03
#define FRAG_SESSION_DELETE_ANS 0x03
#define DATA_BLOCK_AUTH_REQ  0x05
#define DATA_BLOCK_AUTH_ANS  0x05
#define DATA_FRAGMENT  0x08

#define FRAG_SESSION_SETUP_REQ_LENGTH 0x0A
#define FRAG_SESSION_SETUP_ANS_LENGTH 0x2

#define DATA_BLOCK_AUTH_REQ_LENGTH 0xa
#define LORAWAN_APP_FTM_PACKAGE_DATA_MAX_SIZE 20

#define REDUNDANCYMAX 80

#define DELAY_BW2FCNT  10 // 5s
#define STATUS_ERROR 1
#define STATUS_OK 0

typedef struct sMcGroupSetParams {
    uint8_t McGroupIDHeader;

    uint32_t McAddr;

    uint8_t McKey[16];

    uint16_t McCountMSB;

    uint32_t Validity;
} McGroupSetParams_t;

/*!
 * Global  McClassCSession parameters
 */
typedef struct sMcClassCSessionParams
{
  	/*!
     * is the identifier of the multicast  group being used.
     */
    uint8_t McGroupIDHeader ;
    /*!
     * encodes the maximum length in seconds of the multicast fragmentation session
     */
    uint32_t TimeOut;

    /*!
     * encodes the maximum length in seconds of the multicast fragmentation session
     */
    uint32_t TimeToStart;

	  /*!
     * encodes the maximum length in seconds of the multicast fragmentation session ans
     */
     int32_t TimeToStartRec;

  	/*!
     * equal to the 8LSBs of the device�s uplink frame counter used by the network as the reference to provide the session timing information
     */
    uint8_t UlFCountRef;

	  /*!
     * reception frequency
     */
    uint32_t DLFrequencyClassCSession;

	   /*!
     * datarate of the current class c session
     */
    uint8_t DataRateClassCSession ;

		 /*!
     * bit signals the server that the timing information of the uplink
		 * specified by UlFCounter is no longer available
     */
    uint8_t UlFCounterError ;

}McClassCSessionParams_t;


/*!
 * Global  DataBlockTransport parameters
 */
typedef struct sDataBlockTransportParams
{
    /*!
     * Channels TX power
     */
    int8_t ChannelsTxPower;
    /*!
     * Channels data rate
     */
    int8_t ChannelsDatarate;

}DataBlockTransportParams_t;

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

enum FragmenationSessionAnswerErrors {
    FSAE_WrongDescriptor = 3,
    FSAE_IndexNotSupported = 2,
    FSAE_NotEnoughMemory = 1,
    FSAE_EncodingUnsupported = 0,
    FSAE_None = -1
};

#endif
