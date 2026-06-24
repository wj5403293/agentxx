@echo off
rem set utf8
chcp 65001 > NUL

set "script_dir=%~dp0"
set "src_dir=%script_dir%\..\"
set "build_dir=%script_dir%\..\build\debug"

cd %script_dir%
set script_dir=%CD%

cd %src_dir%
set src_dir=%CD%

mkdir %build_dir%
cd %build_dir%
set build_dir=%CD%

echo %build_dir%

cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DAGENTXX_BUILD_CLIENT=ON -DAGENTXX_BUILD_TEST=ON -DXX_BUILD_TYPE=DEBUG -DCMAKE_BUILD_TYPE=Debug -DAGENTXX_ENABLE_HYPERSCAN=OFF -DAGENTXX_ENABLE_CODEGRAPH=OFF -A x64 -DCMAKE_SYSTEM_PROCESSOR=AMD64 -B %build_dir% -S %src_dir%
if %ERRORLEVEL% neq 0 (
    echo cmake config failed!
    exit /b 1
)

cmake --build "%build_dir%" --config debug
if %ERRORLEVEL% neq 0 (
    echo cmake build failed!
    exit /b 1
)

cmake --install "%build_dir%" --config debug
if %ERRORLEVEL% neq 0 (
    echo cmake install failed!
    exit /b 1
)
