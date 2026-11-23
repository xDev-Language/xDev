ROOT := ../..
include $(ROOT)/make/common.mk

# PNG pictures are unpacked with zlib from libxzlib.a; the DLL keeps zlib inside
LIBRARY := gui
LIBRARY_PLATFORM := win
LIBRARY_CFLAGS := -fms-extensions
LIBRARY_DEPENDS := ../fonts/xfont-9x18.h
LIBRARY_SHARED := $(ROOT)/lib/xdev/libxzlib.a

include $(ROOT)/make/user-lib.mk
