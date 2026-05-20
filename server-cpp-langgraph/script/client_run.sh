#!/bin/bash

script_dir=$(dirname "$0")
src_dir=$script_dir/../
build_dir=$src_dir/build/debug

$src_dir/debug_build.sh

if [[ $1 != "--not-run-test" ]]; then
    LD_LIBRARY_PATH=$build_dir/exec $build_dir/exec/client
fi
