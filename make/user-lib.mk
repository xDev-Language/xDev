# Common rules of the xDev user libraries, included by user-lib/<name>/Makefile.
#
# The Makefile includes make/common.mk, sets the variables below, includes this file and adds
# its own rules after it (check-components, objects built in another way).
#
#   LIBRARY            name of the library: libx$(LIBRARY).a
#   C_SOURCES          C files of the library, all of src/*.c when not set
#   PRG_SOURCES        xDev files of the library, all of src/*.prg when not set;
#                      a library has either C files or xDev files
#   LIBRARY_PLATFORM   the only platform the library is built on (win), empty for all
#   LIBRARY_CPPFLAGS   more preprocessor flags of the C files
#   LIBRARY_CFLAGS     more compiler flags of the C files
#   LIBRARY_DEPENDS    more files the C objects depend on
#   EXTRA_OBJECTS      more objects of the library, built by a rule of the Makefile
#   MERGE_ARCHIVES     static libraries whose objects are copied into the library
#   MERGE_EXCLUDE      objects of MERGE_ARCHIVES that are left out
#   MERGE_HOOK         commands run after MERGE_ARCHIVES are unpacked into $(MERGE_DIR)/<archive name>
#
# make builds the library, make install copies it into lib/xdev and its .ch files into include/xdev,
# make uninstall removes it from lib/xdev.

TARGET := libx$(LIBRARY).a
LABEL := $(shell printf '%s' '$(LIBRARY)' | tr '[:lower:]' '[:upper:]')
BUILD_DIR := build
MERGE_DIR := $(BUILD_DIR)/deps

ifneq ($(LIBRARY_PLATFORM),)
ifneq ($(HOST_PLATFORM),$(LIBRARY_PLATFORM))
$(error xDev $(LABEL) is built only on the platform $(LIBRARY_PLATFORM))
endif
endif

C_SOURCES ?= $(wildcard src/*.c)
PRG_SOURCES ?= $(wildcard src/*.prg)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
CH_FILES := $(wildcard include/xdev/*.ch)

CPPFLAGS := -I include $(XDEV_CPPFLAGS) $(LIBRARY_CPPFLAGS)
CFLAGS := $(XDEV_CFLAGS) $(LIBRARY_CFLAGS)

.PHONY: all build install uninstall clean tidy

all:
	$(SHELL) $(ROOT)/make/rebuild.sh $(LABEL) "$(MAKE)" build clean tidy

build: $(TARGET)

ifneq ($(PRG_SOURCES),)

$(TARGET): $(PRG_SOURCES) $(wildcard src/*.ch) $(CH_FILES)
	rm -f $@
	$(XDEV) $(PRG_SOURCES) -I src -I include/xdev -I $(ROOT)/include/xdev -o $@

else

$(BUILD_DIR)/%.o: src/%.c $(wildcard src/*.h) $(CH_FILES) $(LIBRARY_DEPENDS)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# The objects of MERGE_ARCHIVES are added through a response file, so a long list fits on the command line
$(TARGET): $(OBJECTS) $(EXTRA_OBJECTS)
	rm -f $@
ifneq ($(MERGE_ARCHIVES),)
	rm -rf $(MERGE_DIR)
	set -e; for archive in $(abspath $(MERGE_ARCHIVES)); do \
		test -f "$$archive" || continue; \
		name=$$(basename "$$archive" .a); \
		mkdir -p $(MERGE_DIR)/$$name; \
		( cd $(MERGE_DIR)/$$name && $(AR) x "$$archive" $(if $(MERGE_EXCLUDE),&& rm -f $(MERGE_EXCLUDE)) && ls > ../$$name.rsp ); \
	done
	$(MERGE_HOOK)
endif
ifneq ($(OBJECTS)$(EXTRA_OBJECTS),)
	$(AR) qc $@ $(OBJECTS) $(EXTRA_OBJECTS)
endif
ifneq ($(MERGE_ARCHIVES),)
	set -e; for list in $(MERGE_DIR)/*.rsp; do \
		test -f "$$list" || continue; \
		name=$$(basename "$$list" .rsp); \
		( cd $(MERGE_DIR)/$$name && $(AR) qc "$(abspath $@)" @../$$name.rsp ); \
	done
endif
	$(AR) s $@

endif

# The library is not rebuilt here: make removes the objects after a build
install:
	test -f $(TARGET) || { echo "Build the library first: make"; exit 1; }
	$(XDEV) install $(TARGET)
	$(if $(CH_FILES),install -m 644 $(CH_FILES) $(ROOT)/include/xdev/)

uninstall:
	$(XDEV) uninstall $(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

tidy:
	rm -rf $(BUILD_DIR)
