ROOT := ../..
include $(ROOT)/make/common.mk

# The static libraries of OpenSSL are copied in, except the system libraries
LIBRARY := openssl
OPENSSL_PACKAGE := libssl libcrypto
OPENSSL_LIB_DIR := $(shell $(PKG_CONFIG) --variable=libdir libcrypto 2>/dev/null)
OPENSSL_LIBS := $(patsubst -l%,%,$(filter -l%,$(shell $(PKG_CONFIG) --static --libs $(OPENSSL_PACKAGE) 2>/dev/null)))
OPENSSL_EXTERNAL_LIBS := advapi32 bcrypt crypt32 gdi32 iphlpapi kernel32 secur32 user32 wldap32 ws2_32 \
	c m dl pthread rt resolv
LIBRARY_CPPFLAGS := $(shell $(PKG_CONFIG) --static --cflags $(OPENSSL_PACKAGE) 2>/dev/null)
LIBRARY_CFLAGS := -fms-extensions
MERGE_ARCHIVES := $(addprefix $(OPENSSL_LIB_DIR)/lib,$(addsuffix .a,$(filter-out $(OPENSSL_EXTERNAL_LIBS),$(sort $(OPENSSL_LIBS)))))

# OpenSSL functions named in capitals (MD5, SHA256, HMAC...) would collide with xDev functions of the same name,
# so every object of the library, ours too, uses them with the prefix OSSL_; only the objects that define or call
# one of these functions are rewritten
define MERGE_HOOK
$(NM) -g --defined-only $(wildcard $(MERGE_ARCHIVES)) > $(MERGE_DIR)/symbols.txt
awk '$$2 == "T" && $$3 ~ /^[A-Z][A-Z0-9_]*$$/ { print $$3 " OSSL_" $$3 }' $(MERGE_DIR)/symbols.txt | \
	sort -u > $(MERGE_DIR)/rename.txt
test -s $(MERGE_DIR)/rename.txt
set -e; for archive in $(wildcard $(MERGE_ARCHIVES)); do \
	name=$$(basename "$$archive" .a); \
	$(NM) "$$archive" | awk -v dir="$(MERGE_DIR)/$$name" -v list="$(MERGE_DIR)/rename.txt" \
		'BEGIN { while( ( getline line < list ) > 0 ) { split( line, word, " " ); renamed[word[1]] = 1 } } \
		/:$$/ { member = substr( $$0, 1, length( $$0 ) - 1 ); next } \
		$$NF in renamed { print dir "/" member }'; \
done | sort -u > $(MERGE_DIR)/objects.txt
set -e; for object in $(OBJECTS) $$(cat $(MERGE_DIR)/objects.txt); do \
	$(OBJCOPY) --redefine-syms=$(MERGE_DIR)/rename.txt "$$object"; \
done
endef

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) OpenSSL libcrypto "$(OPENSSL_LIB_DIR)/libcrypto.a" openssl libssl-dev

$(OBJECTS) $(TARGET): | check-components
