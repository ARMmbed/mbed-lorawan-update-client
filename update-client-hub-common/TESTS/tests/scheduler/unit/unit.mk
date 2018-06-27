# ----------------------------------------------------------------------------
# Copyright 2018 ARM Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ----------------------------------------------------------------------------
TESTROOT=common/TESTS/tests/scheduler/unit

BUILDDIR=common/TESTS/tests/scheduler/unit/BUILD/linux
TARGETNAME=unit
DEFINES=TARGET_LIKE_POSIX

TESTSRC=$(TESTROOT)/main.cpp

MODSRCS=common/source/arm_uc_scheduler.c $(wildcard atomic-queue/source/*.c)

INCLUDEDIRS=common atomic-queue

INCLUDES=$(patsubst %,-I%,$(INCLUDEDIRS))
CFLAGS+=$(patsubst %,-D%,$(DEFINES))
TARGET=$(BUILDDIR)/$(TARGETNAME)

SRCS=$(TESTSRC) $(MODSRCS)
OBJS=$(patsubst %.c,$(BUILDDIR)/%.o,$(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRCS)))

all: $(TARGET)

$(TARGET): $(OBJS) $(BUILDDIR)
	@echo "[LD] $@"
	@$(CC) -o $(TARGET) $(OBJS) -lc

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o : %.c $(BUILDDIR)
	@mkdir -p `dirname $@`
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILDDIR)/%.o : %.cpp $(BUILDDIR)
	@mkdir -p `dirname $@`
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

test: $(TARGET)
	$(TARGET)

clean:
	rm $(OBJS)
	rm $(TARGET)

.PHONY: all test clean
