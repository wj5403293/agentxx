@echo off
rem set utf8
chcp 65001 > NUL

set SCRIPT_DIR=%~dp0
cd %SCRIPT_DIR%\\..\\deep-agents-ui
yarn dev