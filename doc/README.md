# xDev Install

xDev for Windows is built and used in the [MSYS2](https://www.msys2.org/) CLANG64 environment.

A program can also load modules built from `.prg` files as DLLs. The program and its modules share
one runtime. How to build and load them: [5. DLL modules](#5-dll-modules).

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
```

`make` builds everything from scratch into the xDev folder: the compiler `bin/xdev.exe`, the runtime
library `lib/libxdev.a` and the user libraries in `lib/xdev`. `make dynamic` builds the same and the DLLs
of [5. DLL modules](#5-dll-modules).

## 3. PATH

Add two folders to PATH: the `bin` folder of xDev and the `clang64\bin` folder of MSYS2, which has clang
and its libraries. xDev then works in the MSYS2 shell and in `cmd` as well.

`make install` is not needed. It is for those who do not want the `bin` folder of xDev on PATH: it copies
xDev into the MSYS2 CLANG64 folder: `clang64/bin`, `clang64/lib`, `clang64/lib/xdev` and
`clang64/include/xdev`.

## 4. First program

```sh
cd examples
xdev tictactoe.prg
./tictactoe
```

`tictactoe.exe` is written to the current folder. Libraries in `lib/xdev` are linked automatically.

`-each` builds every file as a separate program, named after its `.prg` file, for example `Alert.exe`.
One command builds a whole folder:

```sh
cd tests
xdev -each *.prg
```

Run all programs of the folder in the MSYS2 shell:

```sh
for f in *.exe; do "./$f"; done
```

and in `cmd`:

```bat
for %f in (*.exe) do "%f"
```

In a `.bat` file write `%%f` instead of `%f`.

## 5. DLL modules

A program can be split into a shell and modules: DLLs built from `.prg` files. The shell and all its
modules share one runtime, `libxdev.dll`, and one DBF library, `libxrdd.dll`, so they see the same
work areas, classes and screen. Modules work on Windows.

Build xDev with the DLLs:

```sh
make dynamic
```

`make dynamic` builds everything that `make` builds, and also `bin/libxdev.dll` and `bin/libxrdd.dll`
next to `xdev.exe`; their import libraries go into `lib`. With `bin` on PATH, programs find the DLLs.

Build a module with `--shared`:

```sh
xdev --shared Invoice.prg -o Invoice.dll
```

The module has no `Main()`. Its FUNCTION and PROCEDURE are exported; a STATIC FUNCTION stays inside.
xdev also writes the import library `Invoice.dll.a`.

Build the shell with `--dynamic` and load the module while it runs:

```sh
xdev --dynamic Shell.prg
```

```
nInvoice := ModuleLoad( "Invoice.dll" )     // sets the STATIC variables and runs INIT PROCEDURE
QOut( ModuleCall( nInvoice, "Start" ) )     // calls the function Start() of the module
ModuleUnload( nInvoice )                    // runs EXIT PROCEDURE and frees the DLL
```

A shell can also be linked with the module: `xdev --dynamic Shell.prg Invoice.dll.a`. It then calls
the functions of the module by name, and the module starts with the shell. The shell finds its modules
next to it, and `libxdev.dll` and `libxrdd.dll` in `bin` on PATH. A whole shell with two modules is in
`examples/plugin`.

Limits:

- A module must be built by the same xDev as the runtime. `ModuleLoad()` rejects another version.
- `ModuleUnload()` does not free a module whose class is in use; it returns `.F.`.
- After `ModuleUnload()` the shell must not use code blocks or `@Function()` of the module, nor an
  `ErrorBlock()` that the module set. A module must not unload itself.
- Only the runtime and rdd are shared. Another user library is linked into each DLL that uses it, with
  its own state. `SHARED_USER_LIBS` in `Makefile` lists the libraries built as DLLs.
