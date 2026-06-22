#!/bin/bash

script_dir=$(cd "$(dirname "$0")" && pwd)
src_dir=$(cd "$script_dir/../" && pwd)
build_dir=$(cd "$script_dir/../build/debug" && pwd)

cmake -B "$build_dir" -S "$src_dir" -DAGENTXX_BUILD_CLIENT=ON -DAGENTXX_BUILD_TEST=ON -DXX_BUILD_TYPE=DEBUG -DCMAKE_BUILD_TYPE=Debug

if [[ $? -ne 0 ]]; then
    echo "cmake config failed!"
    exit 1
fi

cmake --build "$build_dir" --config debug

if [[ $? -ne 0 ]]; then
    echo "cmake build failed!"
    exit 1
fi

cmake --install "$build_dir" --config Debug

if [[ $? -ne 0 ]]; then
    echo "cmake install failed!"
    exit 1
fi
