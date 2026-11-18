#!/bin/sh

set -u

component=$1
make_program=$2
build_target=$3
clean_target=$4
tidy_target=$5

cleanup()
{
   status=$?
   trap - 0 1 2 3 15

   printf "! [%s] Removing intermediate build files\n" "$component"
   "$make_program" --no-print-directory "$tidy_target"
   tidy_status=$?

   if [ "$status" -eq 0 ] && [ "$tidy_status" -ne 0 ]; then
      status=$tidy_status
   fi

   if [ "$status" -eq 0 ]; then
      printf "! [%s] Build completed; intermediate files removed\n" "$component"
   else
      printf "! [%s] Build failed with status %s; intermediate files removed\n" \
         "$component" "$status"
   fi

   exit "$status"
}

trap cleanup 0
trap 'exit 129' 1
trap 'exit 130' 2
trap 'exit 131' 3
trap 'exit 143' 15

printf "! [%s] Removing previous build products\n" "$component"
"$make_program" --no-print-directory "$clean_target" || exit $?

printf "! [%s] Starting full build\n" "$component"
"$make_program" --no-print-directory "$build_target" || exit $?

