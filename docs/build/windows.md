# Windows 可执行程序/动态库 编译

- 系统环境: Windows
- C++ 标准: Requires C++23 or higher.
- 编译器: MSVC

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
- - release 编译可以运行:
```sh
cd {项目根目录}/agent
./script/release_build.bat
```

## 常见错误

### 链接错误 uchardet.lib(uchardet.obj) : error LNK2038
- windows上 msvc 编译出来的库分为 debug 和 release 版本，且区分 静态链接c++标准库 和 动态链接c++标准库，因此一共分为 4种 情况
- 这个报错是由于在构建 debug 库中链接了 release 版本的库，或者在构建 release 库中链接了 debug 版本的库
- 对于 uchardet 需要修改其 CMakeLists.txt，删除或注释掉这段内容，然后删除编译缓存重新编译即可:
```cmake
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(version_suffix .debug)
    add_compile_options("-fsanitize=address")
    add_link_options("-fsanitize=address")
endif (CMAKE_BUILD_TYPE MATCHES Debug)
```
- 有些库编译需要定义 `CMAKE_MSVC_RUNTIME_LIBRARY`，并开启 `CMAKE_POLICY_DEFAULT_CMP0091=NEW` 即可，这需要`cmake 3.16+`。这里举例声明固定为动态链接标准库；如果希望静态链接标准库，可以把 `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL` 改为 `MultiThreaded$<$<CONFIG:Debug>:Debug>` 即可.
```cmake
# 如果可以修改项目的 CMakeLists.txt，则增加: 
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# 如果是导入依赖库，可以添加`CMAKE_ARGS`变量:
ExternalProject_Add(
  glob_repo
  SOURCE_DIR "path/to/glob/"
  CMAKE_ARGS
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
)
```
### 链接错误 error C1128
- 编译参数添加 `/bigobj` 即可.
```cmake
if (MSVC)
	target_compile_options(your_target PRIVATE 
		"/bigobj"
	)
endif ()
```