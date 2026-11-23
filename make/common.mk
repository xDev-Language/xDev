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

# The Windows libraries of the DLLs of make dynamic
XDEV_WINDOWS_LIBS := -ladvapi32 -ldinput8 -lgdi32 -limm32 -lole32 -loleaut32 -lsetupapi \
	-lshell32 -luser32 -luuid -lversion -lwinmm -lkernel32

# make XDEV_MEMORY_CHECKS=0: the runtime neither checks the magic numbers around memory blocks nor counts them
XDEV_MEMORY_CHECKS ?= 1
XDEV_CPPFLAGS := -I$(ROOT)/include -I$(ROOT)/src/rtl -DX_MEMORY_CHECKS=$(XDEV_MEMORY_CHECKS)
# Every function and every variable in a section of its own: xdev links with --gc-sections and a program
# gets only the functions it reaches, not whole objects of the libraries
XDEV_CFLAGS := -fexceptions -O2 -ffunction-sections -fdata-sections -Wall -Wextra -Werror=unused-variable
XDEV_CXXFLAGS := $(XDEV_CFLAGS) -std=c++17 -frtti
