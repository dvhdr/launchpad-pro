BUILDDIR = build

TOOLS = tools

SOURCES += src/app.c

# firmware-only sources (USB MIDI 2.0 transport); not part of the simulator
FW_SOURCES += $(wildcard src/usb/*.c src/ump/*.c src/platform/*.c)

INCLUDES += -Iinclude -Isrc -I

# the vendor archive minus the objects src/usb replaces; c_midi.o gets its
# parser state (DinRx, USBParserToDev) made global for src/usb/usb_midi2.c
ORIG_LIB = lib/launchpad_pro.a
LIB = $(BUILDDIR)/launchpad_pro_trimmed.a
LIB_WORK = $(BUILDDIR)/lib_trim

OBJECTS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(SOURCES) $(FW_SOURCES))))

# output files
SYX = $(BUILDDIR)/launchpad_pro.syx
ELF = $(BUILDDIR)/launchpad_pro.elf
HEX = $(BUILDDIR)/launchpad_pro.hex
HEXTOSYX = $(BUILDDIR)/hextosyx
SIMULATOR = $(BUILDDIR)/simulator
TEST_BIN = $(BUILDDIR)/tests/run_tests

# host unit tests: later tasks append their files to TEST_SOURCES
TEST_SOURCES += tests/run_tests.c tests/test_harness.c
TEST_SOURCES += tests/test_ump_codec.c src/ump/ump_codec.c
TEST_SOURCES += tests/test_ump_tx.c src/ump/ump_tx.c tests/stubs/stubs.c
TEST_SOURCES += tests/test_ump_stream.c src/ump/ump_stream.c
TEST_SOURCES += tests/test_ump_rx.c src/ump/ump_rx.c
TEST_SOURCES += tests/test_usb_desc.c src/usb/usb_desc.c
TEST_SOURCES += tests/test_usb_prop.c src/usb/usb_prop.c src/usb/usb_midi2.c src/usb/usb_endp.c
GOLDEN_DESC = $(BUILDDIR)/tests/golden_usb_desc_rodata.bin
TEST_CFLAGS = -std=c99 -Wall -Werror -DHOST_TEST -Iinclude -Isrc -Itests -DGOLDEN_DESC=\"$(GOLDEN_DESC)\"

# tools
# -include cstdint fixes the pinned libintelhex header under modern g++
HOST_GPP = g++ -include cstdint
HOST_GCC = gcc
CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
AR = arm-none-eabi-ar

CFLAGS  = -Os -Wall -I.\
-D_STM32F103RBT6_  -D_STM3x_  -D_STM32x_ -mthumb -mcpu=cortex-m3 \
-fsigned-char  -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER -DHSE_VALUE=6000000UL \
-DCMSIS -DUSE_GLOBAL_CONFIG -ffunction-sections -std=c99  -mlittle-endian \
$(INCLUDES) -o

LDSCRIPT = stm32_flash.ld

LDFLAGS += -T$(LDSCRIPT) -u _start -u _Minimum_Stack_Size  -mcpu=cortex-m3 -mthumb -specs=nano.specs -specs=nosys.specs -nostdlib -Wl,-static -N -nostartfiles -Wl,--gc-sections
LDFLAGS += -Wl,--wrap=hal_send_midi,--wrap=hal_send_sysex,--wrap=usbmidi_poll,--wrap=midi_flush_windows_buffer,--wrap=midi_parsers_poll,--wrap=midi_parsers_init

all: $(SYX)

# build the final sysex file from the ELF - run the simulator first
$(SYX): $(HEX) $(HEXTOSYX) $(SIMULATOR)
	./$(SIMULATOR)
	./$(HEXTOSYX) $(HEX) $(SYX)
	$(TOOLS)/check_image.sh $(ELF)

# build the tool for conversion of ELF files to sysex, ready for upload to the unit
$(HEXTOSYX): | submodules
	mkdir -p $(BUILDDIR)
	$(HOST_GPP) -Ofast -std=c++0x -I./$(TOOLS)/libintelhex/include ./$(TOOLS)/libintelhex/src/intelhex.cc $(TOOLS)/hextosyx.cpp -o $(HEXTOSYX)

# build the simulator (it's a very basic test of the code before it runs on the device!)
$(SIMULATOR):
	mkdir -p $(BUILDDIR)
	$(HOST_GCC) -g3 -O0 -std=c99 -Iinclude $(TOOLS)/simulator.c $(SOURCES) -o $(SIMULATOR)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(ELF): $(OBJECTS) $(LIB)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LIB)

$(LIB): $(ORIG_LIB)
	rm -rf $(LIB_WORK)
	mkdir -p $(LIB_WORK)
	cp $(ORIG_LIB) $(LIB_WORK)/trimmed.a
	$(AR) d $(LIB_WORK)/trimmed.a usb_desc.o usb_prop.o usb_endp.o
	cd $(LIB_WORK) && $(AR) x $(CURDIR)/$(ORIG_LIB) c_midi.o
	$(OBJCOPY) --globalize-symbol=DinRx --globalize-symbol=USBParserToDev $(LIB_WORK)/c_midi.o
	$(AR) r $(LIB_WORK)/trimmed.a $(LIB_WORK)/c_midi.o
	mv $(LIB_WORK)/trimmed.a $@

DEPENDS := $(OBJECTS:.o=.d)

-include $(DEPENDS)

$(BUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

submodules:
	@test -f $(TOOLS)/libintelhex/include/intelhex.h || { echo "run: git submodule update --init"; exit 1; }

# original USB descriptors, extracted from the vendor archive (alt 0 must stay byte-identical)
$(GOLDEN_DESC): lib/launchpad_pro.a
	mkdir -p $(BUILDDIR)/tests/ar
	cd $(BUILDDIR)/tests/ar && arm-none-eabi-ar x $(CURDIR)/lib/launchpad_pro.a usb_desc.o
	arm-none-eabi-objcopy -O binary --only-section=.rodata $(BUILDDIR)/tests/ar/usb_desc.o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(GOLDEN_DESC) $(TEST_SOURCES) $(wildcard tests/*.h tests/stubs/*.h include/*.h src/*/*.h)
	mkdir -p $(dir $@)
	$(HOST_GCC) $(TEST_CFLAGS) $(TEST_SOURCES) -o $@

.PHONY: all test submodules clean
.DELETE_ON_ERROR:

clean:
	rm -rf $(BUILDDIR)
