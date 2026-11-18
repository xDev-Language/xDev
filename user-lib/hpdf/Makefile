ROOT := ../..
include $(ROOT)/make/common.mk

# libharu 2.4.6 is built from its sources, with the ToUnicode CMap of the UTF-8 encoding corrected in
# hpdf_font_cid.c; libpng is copied in; zlib comes from libxzlib.a
LIBRARY := hpdf
HPDF_SOURCES := $(wildcard libharu/src/*.c)
PNG_CPPFLAGS := $(shell $(PKG_CONFIG) --static --cflags libpng 2>/dev/null)
PNG_LIB_DIR := $(shell $(PKG_CONFIG) --variable=libdir libpng 2>/dev/null)
LIBRARY_CPPFLAGS := -I libharu/include $(PNG_CPPFLAGS)
EXTRA_OBJECTS := $(patsubst libharu/src/%.c,build/libharu/%.o,$(HPDF_SOURCES))
MERGE_ARCHIVES := $(PNG_LIB_DIR)/libpng16.a

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) libpng libpng "$(MERGE_ARCHIVES)" libpng libpng-dev

$(BUILD_DIR)/libharu/%.o: libharu/src/%.c
	mkdir -p $(dir $@)
	$(CC) -I libharu/include $(PNG_CPPFLAGS) -O2 -w -c $< -o $@

$(OBJECTS) $(EXTRA_OBJECTS) $(TARGET): | check-components
