ROOT := ../..
include $(ROOT)/make/common.mk

# OpenSSL headers only; the OpenSSL code comes with libxopenssl.a, the XML code with libxxml.a
LIBRARY := xades
OPENSSL_CPPFLAGS := $(shell $(PKG_CONFIG) --static --cflags libssl libcrypto 2>/dev/null)
OPENSSL_LIB_DIR := $(shell $(PKG_CONFIG) --variable=libdir libcrypto 2>/dev/null)
LIBRARY_CPPFLAGS := -I ../openssl/src -I ../openssl/include -I ../xml/src -I ../xml/include $(OPENSSL_CPPFLAGS)
LIBRARY_CFLAGS := -fms-extensions

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) OpenSSL libcrypto "$(OPENSSL_LIB_DIR)/libcrypto.a" openssl libssl-dev

$(OBJECTS) $(TARGET): | check-components
