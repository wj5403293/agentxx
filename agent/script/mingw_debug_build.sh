#!/bin/bash
# 交叉编译 win 程序

script_dir=$(cd "$(dirname "$0")" && pwd)
src_dir="$script_dir/../"
build_dir="$script_dir/../build/win-debug"

export PATH=$src_dir/third_party/llvm-mingw-msvcrt-x86_64/bin:$PATH

TARGET_ARCH=x86_64-w64-mingw32
MINGW_INSTALL_PREFIX="$src_dir/third_party/llvm-mingw-msvcrt-x86_64"

cmake -DGCC_ARCH=x86-64 -DCOMPILER_TOOLCHAIN=clang \
    -DCMAKE_PROGRAM_PATH="$MINGW_INSTALL_PREFIX/bin" \
    -DCMAKE_FIND_ROOT_PATH="$MINGW_INSTALL_PREFIX/x86_64-w64-mingw32/" \
    -DCMAKE_SYSROOT="$MINGW_INSTALL_PREFIX/x86_64-w64-mingw32/" \
    -DCMAKE_CROSSCOMPILING=ON \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
    -DCMAKE_C_COMPILER_TARGET=$TARGET_ARCH \
    -DCMAKE_CXX_COMPILER_TARGET=$TARGET_ARCH \
    -DCMAKE_C_COMPILER=${TARGET_ARCH}-gcc.exe \
    -DCMAKE_CXX_COMPILER=${TARGET_ARCH}-g++.exe \
    -DCMAKE_RC_COMPILER_INIT=${TARGET_ARCH}-windres.exe \
    -DCMAKE_ASM_COMPILER=${TARGET_ARCH}-gcc.exe \
    -DCMAKE_AR=${TARGET_ARCH}-ar.exe \
    -DCMAKE_RANLIB=${TARGET_ARCH}-ranlib.exe \
    -DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=clang-scan-deps \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -G Ninja -B "$build_dir" -S "$src_dir" \
    -DAGENTXX_BUILD_CLIENT=ON -DAGENTXX_BUILD_TEST=ON -DAGENTXX_ENABLE_VECTORSCAN=OFF -DAGENTXX_ENABLE_HYPERSCAN=ON -DAGENTXX_ENABLE_CODEGRAPH=ON \
    -DXX_BUILD_TYPE=DEBUG -DCMAKE_BUILD_TYPE=Debug

if [[ $? -ne 0 ]]; then
    echo "cmake config failed!"
    exit 1
fi

cmake --build "$build_dir" --config debug

if [[ $? -ne 0 ]]; then
    echo "cmake build failed!"
    exit 1
fi

cmake --install "$build_dir" --config debug

if [[ $? -ne 0 ]]; then
    echo "cmake install failed!"
    exit 1
fi
