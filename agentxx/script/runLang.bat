@echo off
rem set utf8
chcp 65001 > NUL

set SCRIPT_DIR=%~dp0
cd %SCRIPT_DIR%\\..

call .venv\\Scripts\\activate

langgraph dev --no-browser --host 0.0.0.0 