# Windows 可执行程序/动态库 编译

- 系统环境: Linux，通过交叉编译得到 Windows exe/dll
- C++ 标准: Requires C++23 or higher.
- 编译器推荐
    - Clang/llvm

## 开始
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
- 自行编译:
```sh
cd boost\
# 然后编译结果到 agent/third_party/boost-build/
.\bootstrap.bat
.\b2 --prefix=%CD%/../boost-build
.\b2 install --prefix=%CD%/../boost-build

# power shell 使用:
# .\b2 --prefix=$PWD/../boost-build
# .\b2 install --prefix=$PWD/../boost-build
```
### agentxx 编译
- - 启动编译 agentxx，会自动下载其他依赖库，编译成功后自动运行 命令行 client:
```sh
cd {项目根目录}/agent
./script/client_run.bat
```