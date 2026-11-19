ROOT := ../..
include $(ROOT)/make/common.mk

BUILD_DIR := build
SHARED_BUILD_DIR := build-shared
TARGET := $(ROOT)/lib/libxdev.a

C_SOURCES := $(wildcard *.c)
CXX_SOURCES := $(wildcard *.cpp)
OBJECT_NAMES := $(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(OBJECT_NAMES))
SHARED_OBJECTS := $(addprefix $(SHARED_BUILD_DIR)/,$(OBJECT_NAMES))

CPPFLAGS := $(XDEV_CPPFLAGS)
CFLAGS := $(XDEV_CFLAGS)
CXXFLAGS := $(XDEV_CXXFLAGS)
ifeq ($(HOST_PLATFORM),win)
CXXFLAGS += -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS
endif

# The shared runtime of make dynamic: one copy for the program and all its DLL modules.
# Windows exports every symbol, so the list of exports needs no care. The DLL goes into bin next to xdev,
# so PATH to bin is enough to run programs; the import library libxdev.dll.a goes into lib.
SHARED_CPPFLAGS := $(CPPFLAGS) -DX_BUILDING_DLL
ifeq ($(HOST_PLATFORM),win)
SHARED_TARGET := $(ROOT)/bin/libxdev.dll
SHARED_IMPORT := $(ROOT)/lib/libxdev.dll.a
SHARED_PIC :=
SHARED_LDFLAGS := -shared -Wl,--export-all-symbols -Wl,--out-implib,$(SHARED_IMPORT)
SHARED_LIBS := $(XDEV_WINDOWS_LIBS)
else
SHARED_TARGET := $(ROOT)/lib/libxdev.so
SHARED_IMPORT :=
SHARED_PIC := -fPIC
SHARED_LDFLAGS := -shared -Wl,-soname,libxdev.so
SHARED_LIBS := -lm -lpthread -ldl
endif

.PHONY: all build build-shared clean tidy

all:
	$(SHELL) $(ROOT)/make/rebuild.sh "xDev RTL" "$(MAKE)" build clean tidy

build: $(TARGET)

build-shared: $(SHARED_TARGET)

$(TARGET): $(OBJECTS)
	mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $(OBJECTS)

$(SHARED_TARGET): $(SHARED_OBJECTS)
	mkdir -p $(dir $@)
	rm -f $@ $(SHARED_IMPORT)
	$(CXX) $(SHARED_LDFLAGS) -o $@ $(SHARED_OBJECTS) $(SHARED_LIBS)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(SHARED_BUILD_DIR)/%.o: %.c
	mkdir -p $(SHARED_BUILD_DIR)
	$(CC) $(SHARED_CPPFLAGS) $(CFLAGS) $(SHARED_PIC) -c $< -o $@

$(SHARED_BUILD_DIR)/%.o: %.cpp
	mkdir -p $(SHARED_BUILD_DIR)
	$(CXX) $(SHARED_CPPFLAGS) $(CXXFLAGS) $(SHARED_PIC) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(SHARED_BUILD_DIR) $(TARGET) $(SHARED_TARGET) $(SHARED_IMPORT)

tidy:
	rm -rf $(BUILD_DIR) $(SHARED_BUILD_DIR)
