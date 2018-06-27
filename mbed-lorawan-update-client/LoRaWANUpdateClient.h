#ifndef _LORAWAN_UPDATE_CLIENT_H_
#define _LORAWAN_UPDATE_CLIENT_H_

#include "mbed.h"
#include "update_types.h"
#include "FragmentationCrc64.h"
#include "FragmentationBlockDeviceWrapper.h"

#ifndef NB_FRAG_GROUPS
#define NB_FRAG_GROUPS          4
#endif // NB_FRAG_GROUPS

#define LW_UC_CRC_BUFFER_SIZE       128

enum LW_UC_STATUS {
    LW_UC_OK = 0,
    LW_UC_INVALID_PACKET_LENGTH,
    LW_UC_UNKNOWN_COMMAND,
    LW_UC_FRAG_SESSION_NOT_ACTIVE,
    LW_UC_PROCESS_FRAME_FAILED
};

class LoRaWANUpdateClient {
public:
    /**
     * Initialize a new LoRaWANUpdateClient
     *
     * @params bd A block device
     * @params send_fn A send function, invoked when we want to relay data back to the network
     */
    LoRaWANUpdateClient(BlockDevice *bd, Callback<void(uint8_t, uint8_t*, size_t)> send_fn)
        : _bd(bd), _send_fn(send_fn)
    {
        for (size_t ix = 0; ix < NB_FRAG_GROUPS - 1; ix++) {
            frag_sessions[ix].active = false;
            frag_sessions[ix].session = NULL;
        }
    }

    LW_UC_STATUS handleFragmentationCommand(uint8_t *buffer, size_t length) {
        if (length == 0) return LW_UC_INVALID_PACKET_LENGTH;

        switch (buffer[0]) {
            case FRAG_SESSION_SETUP_REQ:
                return handleFragmentationSetupReq(buffer + 1, length - 1);

            case DATA_FRAGMENT:
                return handleDataFragment(buffer + 1, length - 1);

            default:
               return LW_UC_UNKNOWN_COMMAND;
        }
    }

private:
    LW_UC_STATUS handleFragmentationSetupReq(uint8_t *buffer, size_t length) {
        if (length != FRAG_SESSION_SETUP_REQ_LENGTH) {
            // @todo, I assume we need to send a FRAG_SESSION_SETUP_ANS at this point... But not listed in the spec.
            return LW_UC_INVALID_PACKET_LENGTH;
        }

        uint8_t fragIx = (buffer[0] >> 4) & 0b11;

        if (fragIx > NB_FRAG_GROUPS - 1) {
            sendFragSessionAns(FSAE_IndexNotSupported);
            return LW_UC_OK;
        }

        if (frag_sessions[fragIx].active) {
            // clear memory associated with the session - this should clear out the full context...
            delete frag_sessions[fragIx].session;
        }

        frag_sessions[fragIx].mcGroupBitMask = buffer[0] & 0b1111;
        frag_sessions[fragIx].nbFrag = (buffer[2] << 8) + buffer[1];
        frag_sessions[fragIx].fragSize = buffer[3];
        frag_sessions[fragIx].fragAlgo = (buffer[4] >> 3) & 0b111;
        frag_sessions[fragIx].blockAckDelay = buffer[4] & 0b111;
        frag_sessions[fragIx].padding = buffer[5];
        frag_sessions[fragIx].descriptor = (buffer[9] << 24) + (buffer[8] << 16) + (buffer[7] << 8) + buffer[6];

        // create a fragmentation session which can handle all this...
        FragmentationSessionOpts_t opts;
        opts.NumberOfFragments = frag_sessions[fragIx].nbFrag;
        opts.FragmentSize = frag_sessions[fragIx].fragSize;
        opts.Padding = frag_sessions[fragIx].padding;
        opts.RedundancyPackets = MBED_CONF_LORAWAN_UPDATE_CLIENT_MAX_REDUNDANCY - 1;
        opts.FlashOffset = MBED_CONF_LORAWAN_UPDATE_CLIENT_SLOT0_FW_ADDRESS;

        FragmentationSession *session = new FragmentationSession(&_bd, opts);
        FragResult init_res = session->initialize();
        if (init_res != FRAG_OK) {
            printf("Failed to initialize fragmentation session (out of memory?)\n");
            delete session;

            sendFragSessionAns(FSAE_NotEnoughMemory);
            return LW_UC_OK;
        }

        frag_sessions[fragIx].session = session;
        frag_sessions[fragIx].active = true;

        sendFragSessionAns(FSAE_None);
        return LW_UC_OK;
    }

    void sendFragSessionAns(FragmenationSessionAnswerErrors error) {
        uint8_t response = 0b0000;

        switch (error) {
            case FSAE_WrongDescriptor: response = 0b1000;
            case FSAE_IndexNotSupported: response = 0b0100;
            case FSAE_NotEnoughMemory: response = 0b0010;
            case FSAE_EncodingUnsupported: response = 0b0001;
            case FSAE_None: response = 0b0000;
        }

        uint8_t buffer[1];
        buffer[0] = response;
        send(FRAGSESSION_PORT, buffer, 1);
    }

    LW_UC_STATUS handleDataFragment(uint8_t *buffer, size_t length) {
        // top 2 bits are the fragSessionIx, other 16 bits are the pkgIndex
        uint16_t indexAndN = (buffer[1] << 8) + buffer[0];

        uint8_t fragIx = indexAndN >> 14;
        uint16_t frameCounter = indexAndN & 16383;

        if (!frag_sessions[fragIx].active) return LW_UC_FRAG_SESSION_NOT_ACTIVE;
        if (!frag_sessions[fragIx].session) return LW_UC_FRAG_SESSION_NOT_ACTIVE; // should never happen but ok

        FragResult result = frag_sessions[fragIx].session->process_frame(frameCounter, buffer + 2, length - 2);

        if (result == FRAG_OK) {
            return LW_UC_OK;
        }

        if (result == FRAG_COMPLETE) {
            printf("FragSession is complete\n");

            // calculate CRC hash and send to network...
            FragmentationSessionOpts_t opts = frag_sessions[fragIx].session->get_options();

            FragmentationCrc64 crc64(&_bd, crc_buffer, LW_UC_CRC_BUFFER_SIZE);
            uint64_t crc_res = crc64.calculate(opts.FlashOffset, (opts.NumberOfFragments * opts.FragmentSize) - opts.Padding);

            uint8_t* crc_buff = (uint8_t*)&crc_res;

            printf("CRC64 hash is: ");
            for (size_t ix = 0; ix < 8; ix++) {
                printf("%02x ", crc_buff[ix]);
            }
            printf("\n");

            uint8_t buffer[DATA_BLOCK_AUTH_REQ_LENGTH];
            buffer[0] = DATA_BLOCK_AUTH_REQ;
            buffer[1] = fragIx;
            memcpy(buffer + 2, crc_buff, 8);

            send(FRAGSESSION_PORT, buffer, DATA_BLOCK_AUTH_REQ_LENGTH);

            return LW_UC_OK;
        }

        printf("process_frame failed (%d)\n", result);
        return LW_UC_PROCESS_FRAME_FAILED;
    }

    void send(uint8_t port, uint8_t *data, size_t length) {
        _send_fn(port, data, length);
    }

    // store fragmentation groups here...
    FragmentationSessionParams_t frag_sessions[NB_FRAG_GROUPS - 1];
    uint8_t crc_buffer[LW_UC_CRC_BUFFER_SIZE];

    // external storage
    FragmentationBlockDeviceWrapper _bd;
    Callback<void(uint8_t, uint8_t*, size_t)> _send_fn;
};

#endif // _LORAWAN_UPDATE_CLIENT_H_
