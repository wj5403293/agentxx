# Windows 可执行程序/动态库 编译

- 系统环境: Linux，通过交叉编译得到 Windows exe/dll
- C++ 标准: Requires C++23 or higher.
- 编译器推荐
    - Clang/llvm

## 开始
### 安装 VS-Studio-2026、cmake、pkg-config
- 安装 cmake
    - https://cmake.org/download/
    - 选择win版本安装，如: cmake-x.x.x-windows-x86_64.msi
    - https://github.com/Kitware/CMake/releases/download/v4.4.0-rc2/cmake-4.4.0-rc2-windows-x86_64.msi
- 安装 pkg-config
```pwsh
# 管理员身份打开 powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

choco install pkgconfiglite

pkg-config --version
```
### Clang/llvm 编译
- 下载交叉编译工具链:
    - https://github.com/mstorsjo/llvm-mingw/releases
    - 下载并解压 llvm-mingw-xxx-msvcrt-x86_64.zip
    - 示例: 
```sh
cd {项目根目录}/agent/third_party/
wget https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/llvm-mingw-20260616-msvcrt-x86_64.zip
unzip llvm-mingw-20260616-msvcrt-x86_64.zip
mv llvm-mingw-20260616-msvcrt-x86_64 llvm-mingw-msvcrt-x86_64
```
### Boost 编译
- 安装或编译 Boost 1.91
- 自行编译, windows CMD:
```sh
cd boost\
# 然后编译结果到 agent/third_party/boost-build/
.\bootstrap.bat

# 创建 boost-build 目录，并回到 boost 目录
set "boost_source_dir=%CD%"
set "boost_install_dir=%CD%/../boost-build"
mkdir "%boost_install_dir%"
cd "%boost_install_dir%"
set "boost_install_dir=%CD%"
cd "%boost_source_dir%"

# 编译/安装
.\b2.exe install --layout=system --prefix="%boost_install_dir%" link=static runtime-link=shared address-model=64
# 如果想重新构建，可以先执行清理:
# .\b2.exe --clean-all
```
- PowerShell 使用:
```sh
.\b2.exe install --layout=system --prefix="$PWD/../boost-build" link=static runtime-link=shared address-model=64
```
### agentxx 编译
- - 启动编译 agentxx，会自动下载其他依赖库，编译成功后自动运行 命令行 client:
```sh
cd {项目根目录}/agent
./script/client_run.bat
```