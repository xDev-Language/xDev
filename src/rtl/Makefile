ROOT := ../..
include $(ROOT)/make/common.mk

BUILD_DIR := build
TARGET := $(ROOT)/lib/libxdev.a

C_SOURCES := $(wildcard *.c)
CXX_SOURCES := $(wildcard *.cpp)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o))

CPPFLAGS := $(XDEV_CPPFLAGS)
CFLAGS := $(XDEV_CFLAGS)
CXXFLAGS := $(XDEV_CXXFLAGS)
ifeq ($(HOST_PLATFORM),win)
CXXFLAGS += -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS
endif

.PHONY: all build clean tidy

all:
	$(SHELL) $(ROOT)/make/rebuild.sh "xDev RTL" "$(MAKE)" build clean tidy

build: $(TARGET)

$(TARGET): $(OBJECTS)
	mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $(OBJECTS)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

tidy:
	rm -rf $(BUILD_DIR)
