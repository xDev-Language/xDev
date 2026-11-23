ROOT := ../..
include $(ROOT)/make/common.mk

# The libraries GLFW needs are written into the object, so a program links them without extra options
LIBRARY := gtglfw
GLFW_LIB_DIR := $(shell $(PKG_CONFIG) --variable=libdir glfw3 2>/dev/null)
GLFW_STATIC_LDLIBS := $(shell $(PKG_CONFIG) --static --libs glfw3 2>/dev/null)
GLFW_DEPENDENT_LIBS := $(sort $(filter-out glfw glfw3,$(patsubst -l%,%,$(filter -l%,$(GLFW_STATIC_LDLIBS)))) \
	$(if $(filter -pthread,$(GLFW_STATIC_LDLIBS)),pthread))
LIBRARY_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags glfw3 2>/dev/null)
LIBRARY_CFLAGS := $(foreach library,$(GLFW_DEPENDENT_LIBS),-Xclang --dependent-lib=$(library))
ifeq ($(HOST_PLATFORM),win)
LIBRARY_CFLAGS += -fms-extensions
endif
LIBRARY_DEPENDS := $(ROOT)/src/rtl/gtinfo.h $(ROOT)/include/xdev/gtinfo.ch $(ROOT)/include/xdev/gfxdef.ch \
	$(ROOT)/include/xdev/inkey.ch $(ROOT)/include/xdev/setcurs.ch ../fonts/xfont-9x18.h
MERGE_ARCHIVES := $(GLFW_LIB_DIR)/libglfw3.a

include $(ROOT)/make/user-lib.mk

.PHONY: check-components

check-components:
	PKG_CONFIG="$(PKG_CONFIG)" $(SHELL) $(ROOT)/make/component.sh $(LABEL) GLFW glfw3 "$(MERGE_ARCHIVES)" glfw libglfw3-dev
	printf "! [%s] GLFW static library: %s\n" "$(LABEL)" "$(MERGE_ARCHIVES)"
	printf "! [%s] GLFW link dependencies: %s\n" "$(LABEL)" "$(if $(GLFW_DEPENDENT_LIBS),$(GLFW_DEPENDENT_LIBS),none)"

$(OBJECTS) $(TARGET): | check-components
