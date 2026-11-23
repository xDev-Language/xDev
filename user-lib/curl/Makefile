ROOT := ../..
include $(ROOT)/make/common.mk

# The static libraries of libcurl are copied in, except OpenSSL and zlib (libxopenssl.a, libxzlib.a)
# and the system libraries; MSYS2 adds the same path relocation helper to libcurl and libcrypto,
# libxopenssl.a provides it
LIBRARY := curl
CURL_LIB_DIR := $(shell $(PKG_CONFIG) --variable=libdir libcurl 2>/dev/null)
CURL_LIBS := $(patsubst -l%,%,$(filter -l%,$(shell $(PKG_CONFIG) --static --libs libcurl 2>/dev/null)))
CURL_EXTERNAL_LIBS := ssl crypto z advapi32 bcrypt crypt32 gdi32 iphlpapi kernel32 secur32 user32 wldap32 ws2_32 \
	c m dl pthread rt resolv
LIBRARY_CPPFLAGS := $(shell $(PKG_CONFIG) --static --cflags libcurl 2>/dev/null)
LIBRARY_CFLAGS := -fms-extensions
MERGE_ARCHIVES := $(addprefix $(CURL_LIB_DIR)/lib,$(addsuffix .a,$(filter-out $(CURL_EXTERNAL_LIBS),$(sort $(CURL_LIBS)))))
MERGE_EXCLUDE := pathtools.c.obj

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) libcurl libcurl "$(CURL_LIB_DIR)/libcurl.a" curl libcurl4-openssl-dev

$(OBJECTS) $(TARGET): | check-components
