#!/usr/bin/env sh

set -eu

cxx=${CXX:-clang++}
objcopy=${OBJCOPY:-llvm-objcopy}
compiler=$1
destination=$2

mkdir -p "$destination"
: > "$destination/libraries"

copy_file()
{
   name=$1
   source=$("$cxx" -print-file-name="$name")
   if [ "$source" = "$name" ] || [ ! -f "$source" ]; then
      printf 'Missing Linux toolchain file: %s\n' "$name" >&2
      exit 1
   fi
   cp -L "$source" "$destination/$name"
}

copy_library()
{
   copy_file "$1"
   printf '%s\n' "$1" >> "$destination/libraries"
}

for startup in crt1.o crti.o crtbegin.o crtend.o crtn.o; do
   copy_file "$startup"
done

driver=$("$cxx" -### -x c++ /dev/null -o /dev/null 2>&1)
if printf '%s\n' "$driver" | grep -q '"-lc++"'; then
   copy_library libc++.so.1
   copy_library libc++abi.so.1
else
   copy_library libstdc++.so
fi

builtins=$("$cxx" -print-libgcc-file-name)
if [ ! -f "$builtins" ]; then
   printf 'Missing Linux compiler runtime: %s\n' "$builtins" >&2
   exit 1
fi
builtins_name=$(basename "$builtins")
cp -L "$builtins" "$destination/$builtins_name"
printf '%s\n' "$builtins_name" >> "$destination/libraries"

if printf '%s\n' "$driver" | grep -q '"-lunwind"'; then
   copy_library libunwind.so.1
else
   copy_library libgcc_s.so.1
fi

glibc=$("$cxx" -print-file-name=libc.so.6)
if [ "$glibc" != libc.so.6 ] && [ -f "$glibc" ]; then
   copy_library libm.so.6
   copy_library libc.so.6
   copy_library libc_nonshared.a
else
   copy_library libc.so
fi
for optional in libpthread.so.0 libdl.so.2; do
   optional_path=$("$cxx" -print-file-name="$optional")
   if [ "$optional_path" != "$optional" ] && [ -f "$optional_path" ]; then
      copy_library "$optional"
   fi
done

probe=$(mktemp)
trap 'rm -f "$probe"' 0
"$objcopy" --dump-section ".interp=$destination/interpreter" "$compiler" "$probe"
if [ ! -s "$destination/interpreter" ]; then
   printf 'Missing ELF interpreter in %s\n' "$compiler" >&2
   exit 1
fi
"$cxx" -print-search-dirs | sed -n 's/^libraries: =//p' | tr ':' '\n' > "$destination/search-paths"
if [ ! -s "$destination/search-paths" ]; then
   printf 'Missing Linux library search paths\n' >&2
   exit 1
fi
