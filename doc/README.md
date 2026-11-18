# xDev Install

xDev for Windows is built and used in the [MSYS2](https://www.msys2.org/) CLANG64 environment.

## 1. Tools and libraries

Open the MSYS2 CLANG64 shell and install:

```sh
pacman -S make mingw-w64-clang-x86_64-clang \
          mingw-w64-clang-x86_64-lld        \
          mingw-w64-clang-x86_64-llvm       \
          mingw-w64-clang-x86_64-pkgconf    \
          mingw-w64-clang-x86_64-sdl3       \
          mingw-w64-clang-x86_64-glfw       \
          mingw-w64-clang-x86_64-zlib       \
          mingw-w64-clang-x86_64-libpng     \
          mingw-w64-clang-x86_64-openssl    \
          mingw-w64-clang-x86_64-curl
```

## 2. Build

In the xDev folder, in the CLANG64 shell:

```sh
make
make install
```

`make` builds everything from scratch into the xDev folder: the compiler `bin/xdev.exe`, the runtime
library `lib/libxdev.a` and the user libraries in `lib/xdev`. `make install` copies xDev into the MSYS2
CLANG64 folder: `clang64/bin`, `clang64/lib`, `clang64/lib/xdev` and `clang64/include/xdev`.

## 3. PATH

Add the `clang64\bin` folder of MSYS2 to PATH. xDev then works in the MSYS2 shell and in `cmd` as well.

## 4. First program

```sh
cd examples
xdev tictactoe.prg
./tictactoe
```

`tictactoe.exe` is written to the current folder. Libraries in `lib/xdev` are linked automatically.

`-each` builds every file as a separate program, so one command builds a whole folder:

```sh
cd tests
xdev -each *.prg
```

Every program gets the name of its `.prg` file, for example `Alert.exe`.
