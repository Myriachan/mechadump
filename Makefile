KEY_DEFINES = -DMECHA_PATCH_KEY=$(MECHA_PATCH_KEY) -DGLOBAL_FLAGS_KEY=$(GLOBAL_FLAGS_KEY)

PATCHES = patch-irq-hook patch-cdprotect-hook
PATCHES_UNFIXED = $(foreach obj,$(PATCHES),bin/$(obj).unfixed.bin)
PATCHES_FIXED = $(foreach obj,$(PATCHES),bin/$(obj).fixed.bin)
PATCHES_DECRYPTED = $(foreach obj,$(PATCHES),bin/$(obj).decrypted.bin)
PAYLOADS = bin/payload-fastdump.bin bin/payload-writenvm.bin bin/payload-keystoredump.bin

# Make subcommands so we can repeat them between clean and all more easily.
MAKECMD_MECHADUMP = $(MAKE) TOOL_PREFIX=$(EE_PREFIX) OUT_DIR=../out LIB_DIR=../mechacrypto/lib-ee \
	INCLUDE_DIR=../mechacrypto PATCH_DIR=../bin DEFINES="$(KEY_DEFINES)" -C mechadump
MAKECMD_MECHAPATCHTOOL = $(MAKE) TOOL_PREFIX=$(HOST_PREFIX) OUT_DIR=../bin BIN_DIR=bin-host \
	LIB_DIR=../mechacrypto/lib-host INCLUDE_DIR=../mechacrypto DEFINES="$(KEY_DEFINES)" -C mechapatchtool
MAKECMD_MECHACRYPTO_HOST = $(MAKE) TOOL_PREFIX=$(HOST_PREFIX) OUT_DIR=lib-host BIN_DIR=bin-host \
	-C mechacrypto
MAKECMD_MECHACRYPTO_EE = $(MAKE) TOOL_PREFIX=$(EE_PREFIX) OUT_DIR=lib-ee BIN_DIR=bin-ee -C mechacrypto
MAKECMD_PATCH = $(MAKE) TOOL_PREFIX=$(ARM_PREFIX) OUT_DIR=../bin BIN_DIR=bin-arm -C patch
MAKECMD_PAYLOAD = $(MAKE) TOOL_PREFIX=$(ARM_PREFIX) OUT_DIR=../bin BIN_DIR=bin-arm -C payload


.PHONY: all clean debugdata mechadump mechapatchtool libmechacrypto-host libmechacrypto-ee

all: mechadump libmechacrypto-ee debugdata

clean:
	rm -f bin/mechapatchtool bin/*.bin
	$(MAKECMD_MECHADUMP) clean
	$(MAKECMD_MECHAPATCHTOOL) clean
	$(MAKECMD_MECHACRYPTO_HOST) clean
	$(MAKECMD_MECHACRYPTO_EE) clean
	$(MAKECMD_PATCH) clean
	$(MAKECMD_PAYLOAD) clean

bin:
	mkdir bin
out:
	mkdir out

debugdata: $(PATCHES_DECRYPTED)

mechadump: $(PATCHES_FIXED) $(PAYLOADS) libmechacrypto-ee | bin out
	$(MAKECMD_MECHADUMP)

$(PATCHES_DECRYPTED): %.decrypted.bin: %.fixed.bin | bin mechapatchtool
	bin/mechapatchtool --decrypt --in="$^" --out="$@"
$(PATCHES_FIXED): %.fixed.bin: %.unfixed.bin | bin mechapatchtool
	bin/mechapatchtool --fixup --in="$^" --out="$@"
$(PATCHES_UNFIXED): | bin
	$(MAKECMD_PATCH)

$(PAYLOADS):
	$(MAKECMD_PAYLOAD)

mechapatchtool: libmechacrypto-host | bin
	$(MAKECMD_MECHAPATCHTOOL)

libmechacrypto-host:
	$(MAKECMD_MECHACRYPTO_HOST)
libmechacrypto-ee:
	$(MAKECMD_MECHACRYPTO_EE)
