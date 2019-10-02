# Copyright (C) 2019 Fredrik Öhrström

# To compile for Raspberry PI ARM:
# make HOST=arm
#
# To build with debug information:
# make DEBUG=true
# make DEBUG=true HOST=arm

ifeq "$(HOST)" "arm"
    CXX=arm-linux-gnueabihf-g++
    STRIP=arm-linux-gnueabihf-strip
    BUILD=build_arm
	DEBARCH=armhf
else
    CXX=g++
    STRIP=strip
#--strip-unneeded --remove-section=.comment --remove-section=.note
    BUILD=build
	DEBARCH=amd64
endif

ifeq "$(DEBUG)" "true"
    DEBUG_FLAGS=-O0 -ggdb -fsanitize=address -fno-omit-frame-pointer
    STRIP_BINARY=
    BUILD:=$(BUILD)_debug
    DEBUG_LDFLAGS=-lasan
else
    DEBUG_FLAGS=-Os
    STRIP_BINARY=$(STRIP) $(BUILD)/xmq
endif

$(shell mkdir -p $(BUILD))

COMMIT_HASH:=$(shell git log --pretty=format:'%H' -n 1)
TAG:=$(shell git describe --tags)
CHANGES:=$(shell git status -s | grep -v '?? ')
TAG_COMMIT_HASH:=$(shell git show-ref --tags | grep $(TAG) | cut -f 1 -d ' ')

ifeq ($(COMMIT),$(TAG_COMMIT))
  # Exactly on the tagged commit. The version is the tag!
  VERSION:=$(TAG)
  DEBVERSION:=$(TAG)
else
  VERSION:=$(TAG)++
  DEBVERSION:=$(TAG)++
endif

ifneq ($(strip $(CHANGES)),)
  # There are changes, signify that with a +changes
  VERSION:=$(VERSION) with local changes
  COMMIT_HASH:=$(COMMIT_HASH) with local changes
  DEBVERSION:=$(DEBVERSION)l
endif

$(shell echo "#define VERSION \"$(VERSION)\"" > $(BUILD)/version.h.tmp)
$(shell echo "#define COMMIT \"$(COMMIT_HASH)\"" >> $(BUILD)/version.h.tmp)

PREV_VERSION=$(shell cat -n $(BUILD)/version.h 2> /dev/null)
CURR_VERSION=$(shell cat -n $(BUILD)/version.h.tmp 2>/dev/null)
ifneq ($(PREV_VERSION),$(CURR_VERSION))
$(shell mv -f $(BUILD)/version.h.tmp $(BUILD)/version.h)
else
$(shell rm -f $(BUILD)/version.h.tmp)
endif

$(info Building $(VERSION))

CXXFLAGS := $(DEBUG_FLAGS) -fPIC -fmessage-length=0 -std=c++11 -Wall -Wno-unused-function -I$(BUILD) -I.

$(BUILD)/%.o: src/%.cc $(wildcard src/%.h)
	$(CXX) $(CXXFLAGS) $< -c -E > $@.src
	$(CXX) $(CXXFLAGS) $< -MMD -c -o $@

XMQ_OBJS:=\
	$(BUILD)/cmdline.o \
	$(BUILD)/util.o \
	$(BUILD)/parse.o \
	$(BUILD)/xml2xmq.o \
	$(BUILD)/xmq2xml.o

all: $(BUILD)/xmq $(BUILD)/testinternals
	@$(STRIP_BINARY)

$(BUILD)/xmq: $(XMQ_OBJS) $(BUILD)/main.o
	$(CXX) -o $(BUILD)/xmq $(XMQ_OBJS) $(BUILD)/main.o $(DEBUG_LDFLAGS) -lpthread

$(BUILD)/testinternals: $(XMQ_OBJS) $(BUILD)/testinternals.o
	$(CXX) -o $(BUILD)/testinternals $(XMQ_OBJS) $(BUILD)/testinternals.o $(DEBUG_LDFLAGS) -lpthread

clean:
	rm -rf build/* build_arm/* build_debug/* build_arm_debug/* *~

test:
	@./build/testinternals
	@./test.sh build

testdebug:
	@echo Test internals
	@./build_debug/testinternals
	@./test.sh build_debug

install:
	rm -f /usr/share/man/man1/wmbusmeters.1.gz
	mkdir -p /usr/share/man/man1
	gzip -c xmq.1 > /usr/share/man/man1/xmq.1.gz
	cp build/xmq /usr/local/bin
	cp scripts/xmq-less /usr/local/bin
	cp scripts/xmq-git-diff /usr/local/bin

# Include dependency information generated by gcc in a previous compile.
include $(wildcard $(patsubst %.o,%.d,$(XMQ_OBJS)))
