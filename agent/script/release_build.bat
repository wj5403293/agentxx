@echo off
rem set utf8
chcp 65001 > NUL

set "crude_dir=%CD%"

set "script_dir=%~dp0"
set "src_dir=%script_dir%\..\"
set "build_dir=%script_dir%\..\build\release"

cd %script_dir%
set script_dir=%CD%

cd %src_dir%
set src_dir=%CD%

mkdir %build_dir%
cd %build_dir%
set build_dir=%CD%

cd %crude_dir%

rem MSVC output english
set VSLANG=1033
set VCPKG_ROOT=

cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DAGENTXX_BUILD_CLIENT=ON -DAGENTXX_BUILD_TEST=ON -DXX_BUILD_TYPE=RELEASE -DCMAKE_BUILD_TYPE=Release -DCMAKE_CONFIGURATION_TYPES=Release -DAGENTXX_ENABLE_HYPERSCAN=OFF -DAGENTXX_ENABLE_CODEGRAPH=OFF -DCMAKE_SYSTEM_PROCESSOR=AMD64 -A x64 -B %build_dir% -S %src_dir%
if %ERRORLEVEL% neq 0 (
    echo cmake config failed!
    exit /b 1
)

cmake --build "%build_dir%" --config release
if %ERRORLEVEL% neq 0 (
    echo cmake build failed!
    exit /b 1
)

cmake --install "%build_dir%" --config release
if %ERRORLEVEL% neq 0 (
    echo cmake install failed!
    exit /b 1
)