#!/bin/bash

script_dir=$(cd "$(dirname "$0")" && pwd)
src_dir=$(cd "$script_dir/../" && pwd)
build_dir=$(cd "$script_dir/../build/win-debug" && pwd)

$script_dir/mingw_debug_build.sh

if [[ $? -ne 0 ]]; then
    exit $?
fi

LD_LIBRARY_PATH=$build_dir/exec $build_dir/exec/client