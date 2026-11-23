ROOT := ../..
include $(ROOT)/make/common.mk

LLVM_FLAGS := $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_CPPFLAGS := $(filter -D%,$(LLVM_FLAGS)) -idirafter $(shell $(LLVM_CONFIG) --includedir)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS := $(shell $(LLVM_CONFIG) --system-libs --libs core native analysis object transformutils passes)

BUILD_DIR := build
LIB_DIR := $(ROOT)/lib
RTL_LIBRARY := $(LIB_DIR)/libxdev.a
TARGET := $(ROOT)/bin/xdev$(EXE_EXT)

C_SOURCES := $(wildcard frontend/*.c)
CXX_SOURCES := main.cpp $(wildcard frontend/*.cpp backend/*.cpp)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o))

CPPFLAGS := $(XDEV_CPPFLAGS) -Ifrontend -Ibackend
CFLAGS := $(XDEV_CFLAGS) -Wno-unused-parameter -Wno-unused-but-set-variable
CXXFLAGS := $(XDEV_CXXFLAGS) $(filter -stdlib=%,$(LLVM_FLAGS)) -Wno-extern-c-compat

# On Windows LLD takes the start files and C libraries from the C compiler (clang64\lib), on Linux
# from lib/toolchain, which toolchain-linux.sh fills
ifeq ($(HOST_PLATFORM),win)
LLD_LIBS := -llldMinGW -llldCOFF -llldCommon
else
LLD_LIBS := -llldELF -llldCommon
TOOLCHAIN_DIR := $(LIB_DIR)/toolchain
endif

# xdev --version shows the time of the runtime library
$(BUILD_DIR)/main.o: REVISION = -DX_REVISION='"$(shell date -r $(RTL_LIBRARY) '+%Y-%m-%d %H:%M')"'

.PHONY: all build toolchain clean tidy

all:
	$(SHELL) $(ROOT)/make/rebuild.sh "xDev compiler" "$(MAKE)" build clean tidy

build: $(TARGET) $(if $(TOOLCHAIN_DIR),toolchain)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(LLVM_CPPFLAGS) $(CXXFLAGS) $(REVISION) -c $< -o $@

$(TARGET): $(OBJECTS) $(RTL_LIBRARY)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LLVM_LDFLAGS) -o $@ $(OBJECTS) $(RTL_LIBRARY) $(LLD_LIBS) $(LLVM_LIBS)

toolchain: $(TARGET)
	CXX="$(CXX)" OBJCOPY="$(OBJCOPY)" $(SHELL) $(ROOT)/make/toolchain-linux.sh "$(TARGET)" "$(TOOLCHAIN_DIR)"

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TOOLCHAIN_DIR)

tidy:
	rm -rf $(BUILD_DIR)
