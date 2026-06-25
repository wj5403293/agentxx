# Linux 可执行程序/动态库 编译

- 系统环境: Linux
- C++ 标准: Requires C++23 or higher.
- 编译器推荐: Linux/gcc 16.1. 此前使用 gcc 13.2 编译时，部分协程函数会导致编译器自身崩溃

## 开始
- 安装或编译 Boost 1.91
- 安装可以通过系统包管理器直接安装，但需要注意版本
- 自行编译:
```sh
# https://github.com/boostorg/boost
# 下载 release/boost-xxx.7z 解压到 agent/third_party/boost/
cd boost/
# 然后编译结果到 agent/third_party/boost-build/
./bootstrap.sh

boost_install_dir=$(cd "$(dirname "$0")" && pwd)

./b2 install --layout=system --prefix=${boost_install_dir} link=static runtime-link=shared address-model=64
# 如果想重新构建，可以先执行清理:
# ./b2 --clean-all
```
- 启动编译 agentxx，会自动下载其他依赖库，编译成功后自动运行 命令行 client:
```sh
cd {项目根目录}/agent
./script/client_run.sh
```
- - release 编译可以运行:
```sh
cd {项目根目录}/agent
./script/release_build.sh
```