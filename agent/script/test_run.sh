#!/bin/bash

script_dir=$(dirname "$0")
src_dir=$script_dir/../
build_dir=$src_dir/build/debug

$script_dir/debug_build.sh

if [[ $? -ne 0 ]]; then
    exit $?
fi

LD_LIBRARY_PATH=$build_dir/exec $build_dir/exec/test
