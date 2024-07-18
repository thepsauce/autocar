#!/bin/bash

set -ex

CC=gcc
BUILD=bulid
C_FLAGS=('-std=gnu99' '-Wall' '-Wextra' '-Werror' '-Wpedantic' '-g' '-fsanitize=address')
C_LIBS=('-lm' '-lbfd' '-lreadline')
RAW_OBJECTS=('src/args' 'src/cli' 'src/cmd' 'src/conf' 'src/eval' 'src/file' 'src/salloc' 'src/util')
RAW_MAIN_OBJECTS=('src/main' 'tests/lol')
OBJECTS=()

for ro in "${RAW_OBJECTS[@]}" ; do
    o="$BUILD/$ro.o"
    s="$ro.c"
    mkdir -p "$(dirname "$o")"
    "$CC" "${C_FLAGS[@]}" -c "$s" -o "$o"
    OBJECTS+=("$o")
done

for ro in "${RAW_MAIN_OBJECTS[@]}" ; do
    o="$BUILD/$ro.o"
    s="$ro.c"
    mkdir -p "$(dirname "$o")"
    "$CC" "${C_FLAGS[@]}" -c "$s" -o "$o"
done

if [ "${#RAW_MAIN_OBJECTS[@]}" = 0 ] ; then
    exit 0
fi

for ro in "${RAW_MAIN_OBJECTS[@]}" ; do
    o="$BUILD/$ro.o"
    e="$BUILD/$ro"
    mkdir -p "$(dirname "$e")"
    "$CC" "${C_FLAGS[@]}" "${OBJECTS[@]}" "$o" -o "$e" "${C_LIBS[@]}"
done

set +x

echo "run any of the main objects:"
for ro in "${RAW_MAIN_OBJECTS[@]}" ; do
    e="$BUILD/$ro"
    echo "./$e"
done
