# Tools and flags shared by all xDev Makefiles; the Makefile sets ROOT before the include

ifneq ($(filter MINGW% MSYS% CYGWIN%,$(shell uname -s)),)
HOST_PLATFORM := win
EXE_EXT := .exe
else
HOST_PLATFORM := $(shell uname -s | tr '[:upper:]' '[:lower:]')
EXE_EXT :=
endif

CC := clang
CXX := clang++
AR := llvm-ar
NM := llvm-nm
OBJCOPY := llvm-objcopy
PKG_CONFIG := pkg-config
LLVM_CONFIG := llvm-config
XDEV := $(ROOT)/bin/xdev$(EXE_EXT)

XDEV_CPPFLAGS := -I$(ROOT)/include -I$(ROOT)/src/rtl
XDEV_CFLAGS := -fexceptions -O2 -Wall -Wextra -Werror=unused-variable
XDEV_CXXFLAGS := $(XDEV_CFLAGS) -std=c++17 -frtti
