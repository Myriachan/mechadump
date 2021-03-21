ifndef BIN_DIR
BIN_DIR = .
endif
ifndef BIN_DIR
OUT_DIR = .
endif

CRT0 = $(BIN_DIR)/payload-crt0.o

FLAGS = -march=armv4t -mfloat-abi=soft -nostdlib

CC = $(TOOL_PREFIX)gcc
AS = $(TOOL_PREFIX)gcc
LD = $(TOOL_PREFIX)gcc
OBJCOPY = $(TOOL_PREFIX)objcopy

# -fno-isolate-erroneous-paths-dereference is because 0 is a valid address
CCFLAGS = -O2 -std=c11 -Wall -fPIC -fno-isolate-erroneous-paths-dereference $(FLAGS)
ASFLAGS = $(FLAGS)
LDFLAGS = $(FLAGS)
OBJCOPYFLAGS = -O binary


all: $(BIN_DIR) $(OUT_DIR) $(OUT_DIR)/payload-fastdump.bin $(OUT_DIR)/payload-writenvm.bin \
	$(OUT_DIR)/payload-keystoredump.bin

$(BIN_DIR):
	mkdir $(BIN_DIR)
$(OUT_DIR):
	mkdir $(OUT_DIR)

clean:
	rm -f $(OUT_DIR)/payload-fastdump.bin $(BIN_DIR)/payload-fastdump.elf $(BIN_DIR)/payload-fastdump.o
	rm -f $(OUT_DIR)/payload-writenvm.bin $(BIN_DIR)/payload-writenvm.elf $(BIN_DIR)/payload-writenvm.o
	rm -f $(OUT_DIR)/payload-keystoredump.bin $(BIN_DIR)/payload-keystoredump.elf \
		$(BIN_DIR)/payload-keystoredump.o

$(OUT_DIR)/payload-fastdump.bin: $(BIN_DIR)/payload-fastdump.elf
	$(OBJCOPY) $(OBJCOPYFLAGS) $(BIN_DIR)/payload-fastdump.elf $(OUT_DIR)/payload-fastdump.bin
$(BIN_DIR)/payload-fastdump.elf: $(BIN_DIR)/payload-fastdump.o $(CRT0) payload.ld
	$(LD) $(LDFLAGS) $(CRT0) $(BIN_DIR)/payload-fastdump.o -T payload.ld -o $(BIN_DIR)/payload-fastdump.elf
$(BIN_DIR)/payload-fastdump.o: payload-fastdump.c
	$(CC) -c $(CCFLAGS) $^ -o $@

$(OUT_DIR)/payload-writenvm.bin: $(BIN_DIR)/payload-writenvm.elf
	$(OBJCOPY) $(OBJCOPYFLAGS) $(BIN_DIR)/payload-writenvm.elf $(OUT_DIR)/payload-writenvm.bin
$(BIN_DIR)/payload-writenvm.elf: $(BIN_DIR)/payload-writenvm.o $(CRT0) payload.ld
	$(LD) $(LDFLAGS) $(CRT0) $(BIN_DIR)/payload-writenvm.o -T payload.ld -o $(BIN_DIR)/payload-writenvm.elf
$(BIN_DIR)/payload-writenvm.o: payload-writenvm.c
	$(CC) -c $(CCFLAGS) $^ -o $@

$(OUT_DIR)/payload-keystoredump.bin: $(BIN_DIR)/payload-keystoredump.elf
	$(OBJCOPY) $(OBJCOPYFLAGS) $(BIN_DIR)/payload-keystoredump.elf $(OUT_DIR)/payload-keystoredump.bin
$(BIN_DIR)/payload-keystoredump.elf: $(BIN_DIR)/payload-keystoredump.o $(CRT0) payload.ld
	$(LD) $(LDFLAGS) $(CRT0) $(BIN_DIR)/payload-keystoredump.o -T payload.ld -o \
		$(BIN_DIR)/payload-keystoredump.elf
$(BIN_DIR)/payload-keystoredump.o: payload-keystoredump.c
	$(CC) -c $(CCFLAGS) $^ -o $@

$(CRT0): payload-crt0.S
	$(AS) -c $(ASFLAGS) $^ -o $@
