ifndef BIN_DIR
BIN_DIR = .
endif
ifndef BIN_DIR
OUT_DIR = .
endif

FLAGS = -march=armv4t -mfloat-abi=soft -nostdlib

AS = $(TOOL_PREFIX)gcc
LD = $(TOOL_PREFIX)gcc
OBJCOPY = $(TOOL_PREFIX)objcopy

ASFLAGS = $(FLAGS)
LDFLAGS = $(FLAGS)
OBJCOPYFLAGS = -O binary


all: $(BIN_DIR) $(OUT_DIR) $(OUT_DIR)/patch-irq-hook.unfixed.bin $(OUT_DIR)/patch-cdprotect-hook.unfixed.bin

$(BIN_DIR):
	mkdir $(BIN_DIR)
$(OUT_DIR):
	mkdir $(OUT_DIR)

clean:
	rm -f $(OUT_DIR)/*.bin $(BIN_DIR)/*.elf $(BIN_DIR)/*.o

# patch-irq-hook
$(OUT_DIR)/patch-irq-hook.unfixed.bin: $(BIN_DIR)/patch-irq-hook.elf
	$(OBJCOPY) $(OBJCOPYFLAGS) $^ $@
$(BIN_DIR)/patch-irq-hook.elf: $(BIN_DIR)/patch-irq-hook.o patch.ld
	$(LD) $(LDFLAGS) $(BIN_DIR)/patch-irq-hook.o -T patch.ld -o $(BIN_DIR)/patch-irq-hook.elf
$(BIN_DIR)/patch-irq-hook.o: patch-irq-hook.S
	$(AS) -c $(ASFLAGS) $^ -o $@

# patch-cdprotect-hook
$(OUT_DIR)/patch-cdprotect-hook.unfixed.bin: $(BIN_DIR)/patch-cdprotect-hook.elf
	$(OBJCOPY) $(OBJCOPYFLAGS) $^ $@
$(BIN_DIR)/patch-cdprotect-hook.elf: $(BIN_DIR)/patch-cdprotect-hook.o patch.ld
	$(LD) $(LDFLAGS) $(BIN_DIR)/patch-cdprotect-hook.o -T patch.ld -o $(BIN_DIR)/patch-cdprotect-hook.elf
$(BIN_DIR)/patch-cdprotect-hook.o: patch-cdprotect-hook.S
	$(AS) -c $(ASFLAGS) $^ -o $@
