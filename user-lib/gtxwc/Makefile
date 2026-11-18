ROOT := ../..
include $(ROOT)/make/common.mk

LIBRARY := gtxwc
X11_LDLIBS := $(shell $(PKG_CONFIG) --libs x11 2>/dev/null)
LIBRARY_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags x11 2>/dev/null)
LIBRARY_DEPENDS := $(ROOT)/src/rtl/gtinfo.h $(ROOT)/include/xdev/gtinfo.ch $(ROOT)/include/xdev/gfxdef.ch \
	$(ROOT)/include/xdev/inkey.ch $(ROOT)/include/xdev/setcurs.ch

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) X11 x11 "" "" libx11-dev
	printf "! [%s] X11 linker flags: %s\n" "$(LABEL)" "$(X11_LDLIBS)"

$(OBJECTS) $(TARGET): | check-components
