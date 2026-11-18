ROOT := ../..
include $(ROOT)/make/common.mk

LIBRARY := gtsdl3
SDL_LIB_DIR := $(shell $(PKG_CONFIG) --variable=libdir sdl3 2>/dev/null)
LIBRARY_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3 2>/dev/null)
LIBRARY_DEPENDS := $(ROOT)/src/rtl/gtinfo.h $(ROOT)/include/xdev/gtinfo.ch $(ROOT)/include/xdev/gfxdef.ch \
	$(ROOT)/include/xdev/inkey.ch $(ROOT)/include/xdev/setcurs.ch ../fonts/xfont9x18.h
MERGE_ARCHIVES := $(SDL_LIB_DIR)/libSDL3.a

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) SDL3 sdl3 "$(MERGE_ARCHIVES)" sdl3 libsdl3-dev

$(OBJECTS) $(TARGET): | check-components
