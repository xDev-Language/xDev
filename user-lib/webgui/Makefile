ROOT := ../..
include $(ROOT)/make/common.mk

# WebView2Loader.dll of the package mingw-w64-clang-x86_64-webview2-loader is loaded when a window runs
LIBRARY := webgui
LIBRARY_PLATFORM := win
# #pragma comment( lib ) puts ole32 and shlwapi into the object, as in the gui library
LIBRARY_CFLAGS := -fms-extensions
# The runtime of the page is put into the library with #embed
LIBRARY_DEPENDS := web/webgui.js

include $(ROOT)/make/user-lib.mk
