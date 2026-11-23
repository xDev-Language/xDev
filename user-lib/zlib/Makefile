ROOT := ../..
include $(ROOT)/make/common.mk

# zlib for every user library that needs it (curl, gui, hpdf), so it is linked only once
LIBRARY := zlib
ZLIB_LIB_DIR := $(shell $(PKG_CONFIG) --variable=libdir zlib 2>/dev/null)
MERGE_ARCHIVES := $(ZLIB_LIB_DIR)/libz.a

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) zlib zlib "$(MERGE_ARCHIVES)" zlib zlib1g-dev

$(OBJECTS) $(TARGET): | check-components
