#!/bin/bash

set -ex

for ro in 'src/args' 'src/cli' 'src/cmd' 'src/conf' 'src/eval' 'src/file' 'src/salloc' 'src/util' ; do
    o='bulid'/"$ro"'.o'
    s="$ro"'.c'
    mkdir -p "$(dirname "$o")"
    'gcc' '-std=gnu99' '-Wall' '-Wextra' '-Werror' '-Wpedantic' '-g' '-fsanitize=address' -c "$s" -o "$o"
done

if [ 2 = 0 ] ; then
    echo "no main executables"
    exit 0
fi

for ro in 'src/main' 'tests/lol' ; do
    o='bulid'"/$ro"'.o'
    s="$ro"'.c'
    e='bulid'/"$ro"''
    mkdir -p "$(dirname "$o")"
    'gcc' '-std=gnu99' '-Wall' '-Wextra' '-Werror' '-Wpedantic' '-g' '-fsanitize=address' -c "$s" -o "$o"
    'gcc' '-std=gnu99' '-Wall' '-Wextra' '-Werror' '-Wpedantic' '-g' '-fsanitize=address' 'bulid/src/args.o' 'bulid/src/cli.o' 'bulid/src/cmd.o' 'bulid/src/conf.o' 'bulid/src/eval.o' 'bulid/src/file.o' 'bulid/src/salloc.o' 'bulid/src/util.o' "$o" -o "$e" '-lm' '-lbfd' '-lreadline'
done

set +x

echo "run any of the main executables:"
for o in 'bulid/src/main' 'bulid/tests/lol' ; do
    echo "./$o"
done
