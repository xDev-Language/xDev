#!/bin/sh
# Checks an external component of a user library before it is built:
#    component.sh <library> <component> <pkg-config package> <static library or ""> <MSYS2 package> <Debian package>

library=$1
component=$2
package=$3
static_library=$4
msys_package=$5
debian_package=$6
pkg_config=${PKG_CONFIG:-pkg-config}

missing()
{
   printf "! [%s] %s\n" "$library" "$1"
   if test -n "${MSYSTEM:-}"; then
      printf "! [%s] Install it in the MSYS2 CLANG64 shell: pacman -S mingw-w64-clang-x86_64-%s\n" "$library" "$msys_package"
   else
      printf "! [%s] Install its development package, e.g. on Debian/Ubuntu: sudo apt install %s\n" "$library" "$debian_package"
   fi
   exit 1
}

command -v "$pkg_config" >/dev/null 2>&1 || { msys_package=pkgconf; debian_package=pkg-config; missing "pkg-config was not found"; }
"$pkg_config" --exists "$package" || missing "$component was not found (pkg-config package: $package)"
test -z "$static_library" || test -f "$static_library" || missing "$component has no static library: $static_library"

printf "! [%s] %s %s found\n" "$library" "$component" "$("$pkg_config" --modversion "$package")"
