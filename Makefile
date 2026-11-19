ROOT := .
include $(ROOT)/make/common.mk

ifeq ($(HOST_PLATFORM),win)
USER_LIBS := gtwvt gtsdl3 gtglfw rdd tui gui mytiff xml json zlib openssl xades curl hpdf qrcode svg
# make dynamic builds these also as DLLs, so a program and its modules share one copy of their state
SHARED_USER_LIBS := rdd
PREFIX ?= $(MINGW_PREFIX)
else
USER_LIBS := gtxwc gtglfw rdd tui mytiff xml json zlib openssl xades curl hpdf qrcode svg
PREFIX ?= /usr/local
endif

# $(call EACH_LIB,target) runs make target in every user library
EACH_LIB = set -e; for library in $(USER_LIBS); do $(MAKE) -C user-lib/$$library $(1); done

.PHONY: all dynamic build build-dynamic install uninstall clean tidy

all:
	$(SHELL) $(ROOT)/make/rebuild.sh xDev "$(MAKE)" build clean tidy

# make dynamic builds everything of make, the shared runtime and the DLLs of SHARED_USER_LIBS for --shared and --dynamic
dynamic:
	$(SHELL) $(ROOT)/make/rebuild.sh xDev "$(MAKE)" build-dynamic clean tidy

build:
	$(MAKE) -C src/rtl build
	$(MAKE) -C src/compiler build
	$(call EACH_LIB,build install)

build-dynamic: build
	$(MAKE) -C src/rtl build-shared
	set -e; for library in $(SHARED_USER_LIBS); do $(MAKE) -C user-lib/$$library build-shared install-shared; done

# make puts everything into bin, lib and include of this folder. make install copies xDev into the system:
# the MSYS2 CLANG64 folder (MINGW_PREFIX of its shell) on Windows, /usr/local elsewhere, or PREFIX=<folder>.
install:
	test -n "$(PREFIX)" || { echo "make install: run it in the MSYS2 CLANG64 shell or give PREFIX=<folder>"; exit 1; }
	test -f $(XDEV) || { echo "make install: build xDev first: make"; exit 1; }
	install -d $(PREFIX)/bin $(PREFIX)/lib/xdev $(PREFIX)/include/xdev
	install -m 755 $(XDEV) $(PREFIX)/bin/
	install -m 644 lib/libxdev.a $(PREFIX)/lib/
	$(if $(wildcard bin/*.dll),install -m 755 bin/*.dll $(PREFIX)/bin/)
	$(if $(wildcard lib/libxdev.dll.a),install -m 644 lib/libxdev.dll.a $(PREFIX)/lib/)
	$(if $(wildcard lib/libxdev.so),install -m 755 lib/libxdev.so $(PREFIX)/lib/)
	install -m 644 lib/xdev/*.a $(PREFIX)/lib/xdev/
	install -m 644 include/xdev/*.ch $(PREFIX)/include/xdev/

uninstall:
	test -n "$(PREFIX)" || { echo "make uninstall: run it in the MSYS2 CLANG64 shell or give PREFIX=<folder>"; exit 1; }
	rm -rf $(PREFIX)/bin/xdev$(EXE_EXT) $(PREFIX)/lib/libxdev.a $(PREFIX)/lib/xdev $(PREFIX)/include/xdev
	rm -f $(PREFIX)/bin/libxdev.dll $(PREFIX)/lib/libxdev.dll.a $(PREFIX)/lib/libxdev.so
	rm -f $(foreach library,$(SHARED_USER_LIBS),$(PREFIX)/bin/libx$(library).dll)

clean:
	$(MAKE) -C src/compiler clean
	$(MAKE) -C src/rtl clean
	$(call EACH_LIB,clean)
	rm -rf lib/xdev
	rm -f $(foreach library,$(SHARED_USER_LIBS),bin/libx$(library).dll)

tidy:
	$(MAKE) -C src/compiler tidy
	$(MAKE) -C src/rtl tidy
	$(call EACH_LIB,tidy)
