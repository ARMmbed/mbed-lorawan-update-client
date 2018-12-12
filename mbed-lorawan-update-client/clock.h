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

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_CLOCK
#define _MBED_LORAWAN_UPDATE_CLIENT_CLOCK

#if !MBED_CONF_RTOS_PRESENT && !defined(TARGET_SIMULATOR)

#include "mbed.h"

/**
 * We don't have access to the Kernel::get_ms_count() function when running in non-RTOS mode
 * This uses Timer interrupts underneath.
 *
 * NOTE: Need to check if we can use the Low-Power timer when it's available!
 *
 * Based on https://os.mbed.com/questions/61002/Equivalent-to-Arduino-millis/
 */
class Clock : public TimerEvent {
public:
	Clock() {
		// This also starts the us ticker.
		insert(0x40000000);
	}

	float read() {
		return read_us() / 1000000.0f;
	}

	uint64_t read_ms() {
		return read_us() / 1000;
	}

	uint64_t read_us() {
		return _triggers * 0x40000000ull + (ticker_read(_ticker_data) & 0x3FFFFFFF);
	}

private:
	void handler() override {
		++_triggers;
		// If this is the first time we've been called (at 0x4...)
		// then _triggers now equals 1 and we want to insert at 0x80000000.
		insert((_triggers+1) * 0x40000000);
	}

	// The number of times the us_ticker has rolled over.
	uint32_t _triggers = 0;
};

#endif // #if !MBED_CONF_RTOS_PRESENT && !defined(TARGET_SIMULATOR)

#endif // _MBED_LORAWAN_UPDATE_CLIENT_CLOCK
