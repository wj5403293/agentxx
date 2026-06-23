@echo off
setlocal

set "script_dir=%~dp0"
set "src_dir=%script_dir%\..\"
set "build_dir=%script_dir%\..\build\debug"

cd %script_dir%
set script_dir=%CD%

cd %src_dir%
set src_dir=%CD%

cd %build_dir%
set build_dir=%CD%

cmake -DAGENTXX_BUILD_CLIENT=ON -DAGENTXX_BUILD_TEST=ON -DXX_BUILD_TYPE=DEBUG -DCMAKE_BUILD_TYPE=Debug -DAGENTXX_ENABLE_HYPERSCAN=OFF -DAGENTXX_ENABLE_CODEGRAPH=OFF -A x64 -DCMAKE_SYSTEM_PROCESSOR=AMD64 -B "%build_dir%" -S "%src_dir%"
if errorlevel 1 (
    echo cmake config failed!
    exit /b 1
)

cmake --build "%build_dir%" --config debug
if errorlevel 1 (
    echo cmake build failed!
    exit /b 1
)

cmake --install "%build_dir%" --config Debug
if errorlevel 1 (
    echo cmake install failed!
    exit /b 1
)

echo All steps completed successfully.
endlocal