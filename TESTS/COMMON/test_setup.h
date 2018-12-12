#ifndef _TEST_STORAGE_HELPER_H_
#define _TEST_STORAGE_HELPER_H_

#include "mbed.h"

#if defined(TARGET_SIMULATOR)
// Initialize a persistent block device with 528 bytes block size, and 256 blocks (mimicks the at45, which also has 528 size blocks)
#include "SimulatorBlockDevice.h"
SimulatorBlockDevice bd("lorawan-frag-in-flash", 256 * 528, static_cast<uint64_t>(528));

#elif defined(TARGET_FF1705_L151CC)
// Flash interface on the L-TEK xDot shield
#include "AT45BlockDevice.h"
AT45BlockDevice bd(SPI_MOSI, SPI_MISO, SPI_SCK, SPI_NSS);

#else
#error "No storage selected in test_setup.h"

#endif


bool compare_buffers(uint8_t* buff1, const uint8_t* buff2, size_t size) {
    for (size_t ix = 0; ix < size; ix++) {
        if (buff1[ix] != buff2[ix]) return false;
    }
    return true;
}

#endif // _TEST_STORAGE_HELPER_H_