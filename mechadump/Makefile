BIN_DIR = bin$(OUT_SUFFIX)
ifndef OUT_DIR
OUT_DIR = $(BIN_DIR)
endif

IRXES = fileXio iomanX mcman mcserv mtapman padman sio2man usbd usbhdfsd
IRX_FILES = $(foreach irx,$(IRXES),$(PS2SDK)/iop/irx/$(irx).irx)
IRX_SOURCES = $(foreach irx,$(IRXES),$(BIN_DIR)/$(irx).irx.c)
IRX_OBJECTS = $(foreach irx,$(IRXES),$(BIN_DIR)/$(irx).irx.o)

PATCHES = patch-irq-hook patch-cdprotect-hook
PAYLOADS = payload-fastdump payload-writenvm payload-keystoredump
PATCH_OBJECTS = $(foreach obj,$(PATCHES),$(BIN_DIR)/$(obj).fixed.o)
PAYLOAD_OBJECTS = $(foreach obj,$(PAYLOADS),$(BIN_DIR)/$(obj).o)
BIN_OBJECTS = $(PATCH_OBJECTS) $(PAYLOAD_OBJECTS) $(IRX_OBJECTS)

C_OBJECTS = 
TARGET_C_OBJECTS = $(foreach obj,$(C_OBJECTS),$(BIN_DIR)/$(obj))
CXX_OBJECTS = configexploit.o dumper.o mechadump.o sysinfo.o
TARGET_CXX_OBJECTS = $(foreach obj,$(CXX_OBJECTS),$(BIN_DIR)/$(obj))

EE_DEFINES = -D_EE -G0
EE_INCLUDE = -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include
EE_LDFLAGS = -L$(PS2SDK)/ee/lib -T$(PS2SDK)/ee/startup/linkfile

EXE = $(OUT_DIR)/mechadump.elf

LIBS = -lmechacrypto -ldebug -lpadx -lmtap -lmc -lfileXio -lpatches -lc -lcdvd

CC = $(TOOL_PREFIX)gcc
CXX = $(TOOL_PREFIX)g++
LD = $(TOOL_PREFIX)g++
OBJCOPY = $(TOOL_PREFIX)objcopy

CPPFLAGS = -O2 -Wall $(EE_DEFINES) $(DEFINES) -I$(INCLUDE_DIR) $(EE_INCLUDE)
CFLAGS = $(CPPFLAGS) -std=c11
CXXFLAGS = $(CPPFLAGS) -std=c++14
LDFLAGS = $(EE_LDFLAGS)

.PHONY: all clean headers

all: $(BIN_DIR) $(OUT_DIR) $(EXE)

clean:
	rm -f $(BIN_DIR)/*.o $(BIN_DIR)/*.c $(EXE)

$(BIN_DIR):
	mkdir $@
$(OUT_DIR):
	mkdir $@

$(EXE): $(TARGET_C_OBJECTS) $(TARGET_CXX_OBJECTS) $(BIN_OBJECTS) | $(OUT_DIR)
	$(LD) $(LDFLAGS) -L$(LIB_DIR) $^ $(LIBS) -o $@

$(TARGET_C_OBJECTS): $(BIN_DIR)/%.o: %.c | headers
	$(CC) $(CFLAGS) -c $^ -o $@

$(TARGET_CXX_OBJECTS): $(BIN_DIR)/%.o: %.cpp | headers
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(IRX_OBJECTS): $(BIN_DIR)/%.irx.o: $(BIN_DIR)/%.irx.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(IRX_SOURCES): $(BIN_DIR)/%.irx.c: $(PS2SDK)/iop/irx/%.irx
	bin2c $^ $@ irx_`echo "$^" | sed s/^.*\\\\/// | sed s/\\\\.irx//`

$(BIN_DIR)/patch-irq-hook.fixed.o: $(BIN_DIR)/patch-irq-hook.fixed.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(BIN_DIR)/patch-irq-hook.fixed.c: $(PATCH_DIR)/patch-irq-hook.fixed.bin
	bin2c $^ $@ patch_irq_hook

$(BIN_DIR)/patch-cdprotect-hook.fixed.o: $(BIN_DIR)/patch-cdprotect-hook.fixed.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(BIN_DIR)/patch-cdprotect-hook.fixed.c: $(PATCH_DIR)/patch-cdprotect-hook.fixed.bin
	bin2c $^ $@ patch_cdprotect_hook

$(BIN_DIR)/payload-fastdump.o: $(BIN_DIR)/payload-fastdump.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(BIN_DIR)/payload-fastdump.c: $(PATCH_DIR)/payload-fastdump.bin
	bin2c $^ $@ payload_fastdump

$(BIN_DIR)/payload-writenvm.o: $(BIN_DIR)/payload-writenvm.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(BIN_DIR)/payload-writenvm.c: $(PATCH_DIR)/payload-writenvm.bin
	bin2c $^ $@ payload_writenvm

$(BIN_DIR)/payload-keystoredump.o: $(BIN_DIR)/payload-keystoredump.c
	$(CC) $(CFLAGS) -c $^ -o $@
$(BIN_DIR)/payload-keystoredump.c: $(PATCH_DIR)/payload-keystoredump.bin
	bin2c $^ $@ payload_keystoredump

headers: *.hpp
