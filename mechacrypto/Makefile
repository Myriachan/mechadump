BIN_DIR = bin$(OUT_SUFFIX)
ifndef OUT_DIR
OUT_DIR = $(BIN_DIR)
endif

C_OBJECTS = cipher.o crc32.o util.o
TARGET_C_OBJECTS = $(foreach obj,$(C_OBJECTS),$(BIN_DIR)/$(obj))

LIBRARY = $(OUT_DIR)/libmechacrypto$(OUT_SUFFIX).a

CC = $(TOOL_PREFIX)gcc
CXX = $(TOOL_PREFIX)g++
AR = $(TOOL_PREFIX)ar

BASEFLAGS = -O2 -Wall
CCFLAGS = $(BASEFLAGS) -std=c11
CXXFLAGS = $(BASEFLAGS) -std=c++14

.PHONY: all clean headers

all: $(BIN_DIR) $(OUT_DIR) $(LIBRARY)

clean:
	rm -f $(BIN_DIR)/*.o $(BIN_DIR)/*.a $(LIBRARY)

$(BIN_DIR):
	mkdir $@
$(OUT_DIR):
	mkdir $@

$(LIBRARY): $(TARGET_C_OBJECTS) | $(OUT_DIR)
	rm -f $@
	$(AR) -crs $@ $^

# Will need to fix...
#$(TARGET_C_OBJECTS): $(BIN_DIR)/%.o: %.cpp
#	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(TARGET_C_OBJECTS): $(BIN_DIR)/%.o: %.c | $(BIN_DIR) headers
	$(CC) $(CCFLAGS) -c $^ -o $@

headers: *.h
