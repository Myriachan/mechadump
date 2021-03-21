BIN_DIR = bin$(OUT_SUFFIX)
ifndef OUT_DIR
OUT_DIR = $(BIN_DIR)
endif

CXX_OBJECTS = mechapatchtool.o
TARGET_CXX_OBJECTS = $(foreach obj,$(CXX_OBJECTS),$(BIN_DIR)/$(obj))

EXE = $(OUT_DIR)/mechapatchtool

CC = $(TOOL_PREFIX)gcc
CXX = $(TOOL_PREFIX)g++
LD = $(TOOL_PREFIX)g++

BASEFLAGS = -O2 -Wall $(DEFINES) -I$(INCLUDE_DIR)
CCFLAGS = $(BASEFLAGS) -std=c11
CXXFLAGS = $(BASEFLAGS) -std=c++14

.PHONY: all clean

all: $(BIN_DIR) $(OUT_DIR) $(EXE)

clean:
	rm -f $(BIN_DIR)/*.o $(BIN_DIR)/*.a $(EXE)

$(BIN_DIR):
	mkdir $@
$(OUT_DIR):
	mkdir $@

$(EXE): $(TARGET_CXX_OBJECTS) | $(OUT_DIR)
	$(LD) $^ -L$(LIB_DIR) -lmechacrypto -o $@

$(TARGET_CXX_OBJECTS): $(BIN_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@
