// ----------------------------------------------------------------------------
// Copyright 2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------
#include "update-client-common/arm_uc_scheduler.h"
#include "update-client-common/arm_uc_config.h"

#include <stdio.h>

bool test_success = true;

#define TEST_INIT()\
    do {test_success = 1;} while (0)

#define TEST_RESULT()\
    (test_success)

#define TEST_FAIL()\
    ((test_success) = false)

#define CHECK_EQ(EXP,ACT)\
    do { if ((EXP) != (ACT)) { TEST_FAIL(); printf("[FAIL] %s:%d ( " #EXP " != " #ACT " )\n", __FILE__, __LINE__ ); } } while (0)
#define CHECK_NEQ(EXP,ACT)\
    do { if ((EXP) == (ACT)) { TEST_FAIL(); printf("[FAIL] %s:%d ( " #EXP " == " #ACT " )\n", __FILE__, __LINE__ ); } } while (0)


bool cb1_called;
void emptycb(uint32_t arg)
{
    cb1_called = true;
}

bool cb2_called;
void emptycb2(uint32_t arg)
{
    cb2_called = true;
}

static bool handlerCalled = false;
void errorHandler(uint32_t event)
{
    handlerCalled = true;
}

static bool notifyCalled = false;
void notifyHandler(void)
{
    notifyCalled = true;
}

void PrintTestResult(const char* test, bool success) {
    printf("%s %s\n", success?"[PASS]":"[FAIL]", test);
}

bool SchedulerTest_PostCallback()
{
    TEST_INIT();
    cb1_called = false;
    cb2_called = false;
    ARM_UC_SchedulerInit();
    // Pass a NULL function.
    CHECK_EQ(false, ARM_UC_PostCallback(NULL,NULL,0));
    // Pass a statically allocated callback
    arm_uc_callback_t local_cb = {0};
    CHECK_EQ(true, ARM_UC_PostCallback(&local_cb, emptycb, 0));
    // Pass a statically allocated callback that is already in use.
    CHECK_EQ(true, ARM_UC_PostCallback(&local_cb, emptycb2, 1));
    // Verify that it wasn't modified
    CHECK_EQ(emptycb, local_cb.callback);
    CHECK_EQ(0, local_cb.parameter);
    // Check the watermark.
    CHECK_EQ(1, ARM_UC_SchedulerGetHighWatermark());
    // Pass a NULL callback.
    CHECK_EQ(true, ARM_UC_PostCallback(NULL,emptycb,0));
    // Check the watermark.
    CHECK_EQ(2, ARM_UC_SchedulerGetHighWatermark());
    // Consume all remaining callbacks
    for (size_t i = 0; i < ARM_UC_SCHEDULER_STORAGE_POOL_SIZE - 2; i++) {
        CHECK_EQ(true, ARM_UC_PostCallback(NULL,emptycb,0));
    }
    // Check the watermark.
    CHECK_EQ(ARM_UC_SCHEDULER_STORAGE_POOL_SIZE, ARM_UC_SchedulerGetHighWatermark());
    // Pass one more NULL callback.
    CHECK_EQ(false, ARM_UC_PostCallback(NULL,emptycb,0));
    // Check the watermark.
    CHECK_EQ(ARM_UC_SCHEDULER_STORAGE_POOL_SIZE, ARM_UC_SchedulerGetHighWatermark());
    // Register the error handler.
    ARM_UC_SetSchedulerErrorHandler(errorHandler);
    handlerCalled = false;
    // Pass one more NULL callback.
    CHECK_EQ(false, ARM_UC_PostCallback(NULL,emptycb,0));
    // process all callbacks in the queue
    ARM_UC_ProcessQueue();
    // Verify that both callbacks were called.
    CHECK_EQ(true, cb1_called);
    CHECK_EQ(true, cb2_called);
    // Validate that error handler was called.
    CHECK_EQ(true, handlerCalled);
    // Register a notification handler
    notifyCalled = false;
    ARM_UC_AddNotificationHandler(notifyHandler);
    // Pass a NULL callback.
    CHECK_EQ(true, ARM_UC_PostCallback(NULL,emptycb,0));
    // Make sure that notify was called
    CHECK_EQ(true, notifyCalled);
    // process all callbacks in the queue
    ARM_UC_ProcessQueue();

    return TEST_RESULT();
}

uint32_t test_sum;
void sumHandler(uint32_t event)
{
    test_sum += event;
}

bool SchedulerTest_ProcessQueue()
{
    ARM_UC_SchedulerInit();
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // process an empty queue.
    ARM_UC_ProcessQueue();
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // Zero the sum handler
    test_sum = 0;
    // Queue an event:
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 1));
    // Read the queue counter.
    CHECK_EQ(1, ARM_UC_SchedulerGetQueuedCount());
    // Process the queue
    ARM_UC_ProcessQueue();
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // Verify that the sum handler was called.
    CHECK_EQ(1, test_sum);    
    // Queue several events.
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 1));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 2));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 4));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 8));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    // Read the queue counter.
    CHECK_EQ(8, ARM_UC_SchedulerGetQueuedCount());
    // Process the queue
    ARM_UC_ProcessQueue();
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // Verify that the sum handler was called.
    CHECK_EQ(16, test_sum);    
    return TEST_RESULT();
}

bool SchedulerTest_ProcessSingleCallback()
{
    ARM_UC_SchedulerInit();
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // process an empty queue.
    CHECK_EQ(false, ARM_UC_ProcessSingleCallback());
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // Zero the sum handler
    test_sum = 0;
    // Queue an event:
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 1));
    // Read the queue counter.
    CHECK_EQ(1, ARM_UC_SchedulerGetQueuedCount());
    // Process the queue
    CHECK_EQ(false, ARM_UC_ProcessSingleCallback());
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // Verify that the sum handler was called.
    CHECK_EQ(1, test_sum);    
    // Queue several events.
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 1));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 2));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 4));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, sumHandler, 8));
    CHECK_EQ(true, ARM_UC_PostCallback(NULL, emptycb,0));
    // Read the queue counter.
    CHECK_EQ(8, ARM_UC_SchedulerGetQueuedCount());
    // Process the queue
    CHECK_EQ(true, ARM_UC_ProcessSingleCallback());
    CHECK_EQ(2, test_sum);    
    CHECK_EQ(true, ARM_UC_ProcessSingleCallback());
    CHECK_EQ(2, test_sum);    
    CHECK_EQ(true, ARM_UC_ProcessSingleCallback());
    CHECK_EQ(4, test_sum);    
    CHECK_EQ(true, ARM_UC_ProcessSingleCallback());
    CHECK_EQ(4, test_sum);    
    CHECK_EQ(true, ARM_UC_ProcessSingleCallback());
    CHECK_EQ(8, test_sum);    
    CHECK_EQ(true, ARM_UC_ProcessSingleCallback());
    CHECK_EQ(8, test_sum);    
    CHECK_EQ(true, ARM_UC_ProcessSingleCallback());
    CHECK_EQ(16, test_sum);    
    CHECK_EQ(false, ARM_UC_ProcessSingleCallback());
    // Read the queue counter.
    CHECK_EQ(0, ARM_UC_SchedulerGetQueuedCount());
    // Verify that the sum handler was called.
    CHECK_EQ(16, test_sum);    
    return TEST_RESULT();
}

int main(int argc, char* argv[])
{
    bool success = SchedulerTest_PostCallback();
    PrintTestResult("Scheduler Test: Post Callback", success);

    success = SchedulerTest_ProcessQueue();
    PrintTestResult("Scheduler Test: Process Queue", success);

    success = SchedulerTest_ProcessSingleCallback();
    PrintTestResult("Scheduler Test: Process Single Callback", success);
    return 0;
}

