# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0097 NEW)

project(agentxx_project VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XX_BUILD_TYPE "Generate the client target." "RELEASE")
option(AGENTXX_BUILD_CLIENT "Generate the client target." OFF)
option(AGENTXX_BUILD_TEST "Generate the test target." OFF)
option(AGENTXX_ENABLE_HYPERSCAN "Add HyperScan for regex support. Use std::regex if disable." ON)
option(AGENTXX_ENABLE_CODEGRAPH "Add CodeGraph-CPP for code analysis." ON)

# ccache
find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif ()

# 仅用于指示是否编译用于调试用途，不代表真实编译类型
# - [IS_DEBUG_D] == 1 时，启用大部分调试日志等信息，因此 profile 之类注重
# 性能的编译将视为 release
set(IS_DEBUG_D    0)
set(IS_RELEASE_D  1)

if (XX_BUILD_TYPE MATCHES "DEBUG")
  set(IS_DEBUG_D    1)
  set(IS_RELEASE_D  0)
endif()

set(IS_LINUX_D    0)
set(IS_WIN_D      0)
set(IS_MACOS_D    0)
set(IS_ANDROID_D  0)
set(IS_IOS_D      0)

if (WIN32)
  set(IS_WIN_D 1)

  set(AGENTXX_WINDOWS_IOCP_SUPPORTED ON)
elseif (ANDROID)
  set(IS_ANDROID_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(IS_MACOS_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(IS_IOS_D 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(IS_LINUX_D 1)

  # 检查是否支持 [AGENTXX_LINUX_IO_URING_SUPPORTED]
  file(READ "/usr/include/linux/version.h" _linux_version_h)
  string(REGEX MATCH "#define LINUX_VERSION_CODE ([0-9]+)" _match "${_linux_version_h}")
  if(_match AND (CMAKE_MATCH_1 GREATER_EQUAL 330240))  # >= 5.10.0
      set(AGENTXX_LINUX_IO_URING_SUPPORTED ON)
  else()
      set(AGENTXX_LINUX_IO_URING_SUPPORTED OFF)
  endif()
endif ()

message("[agentxx] Current Configure Type: ${CMAKE_CONFIGURATION_TYPES}")
message("[agentxx] Current Build Type: ${CMAKE_BUILD_TYPE}")

set(_AGENTXX_COMMON_C_CXX_FLAGS)

if (MSVC) 
  set(_AGENTXX_COMMON_C_CXX_FLAGS 
    "${_AGENTXX_COMMON_C_CXX_FLAGS} /D__STDC_CONSTANT_MACROS=1 /DCPPHTTPLIB_OPENSSL_SUPPORT=1 /DBOOST_ASIO_STANDALONE=1 /DASIO_STANDALONE=1 /DBOOST_PROCESS_USE_STD_FS=1"
  )
  if (AGENTXX_WINDOWS_IOCP_SUPPORTED)
    set(_AGENTXX_COMMON_C_CXX_FLAGS 
      "${_AGENTXX_COMMON_C_CXX_FLAGS} /DASIO_HAS_IOCP=ON /DBOOST_ASIO_HAS_IOCP=ON /DASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE=ON /DBOOST_ASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE=ON"
    )
    message("[agentxx] Enable asio/iocp")
  endif()
  if (AGENTXX_LINUX_IO_URING_SUPPORTED)
    set(_AGENTXX_COMMON_C_CXX_FLAGS
      "${_AGENTXX_COMMON_C_CXX_FLAGS} /DASIO_HAS_IO_URING=ON /DBOOST_ASIO_HAS_IO_URING=ON /DASIO_HAS_IO_URING_AS_DEFAULT=ON /DBOOST_ASIO_HAS_IO_URING_AS_DEFAULT=ON"
    )
    message("[agentxx] Enable asio/io_uring")
  endif()
else ()
  set(_AGENTXX_COMMON_C_CXX_FLAGS
    ${_AGENTXX_COMMON_C_CXX_FLAGS}
    -D__STDC_CONSTANT_MACROS=1
    -DCPPHTTPLIB_OPENSSL_SUPPORT=1
    # boost/asio
    -DBOOST_ASIO_STANDALONE=1
    -DASIO_STANDALONE=1
    # boost/process
    -DBOOST_PROCESS_USE_STD_FS=1
  )
  if (AGENTXX_WINDOWS_IOCP_SUPPORTED)
    set(_AGENTXX_COMMON_C_CXX_FLAGS
      ${_AGENTXX_COMMON_C_CXX_FLAGS}
      -DASIO_HAS_IOCP=ON
      -DBOOST_ASIO_HAS_IOCP=ON
      -DASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE=ON
      -DBOOST_ASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE=ON
    )
    message("[agentxx] Enable asio/iocp")
  endif()
  if (AGENTXX_LINUX_IO_URING_SUPPORTED)
    set(_AGENTXX_COMMON_C_CXX_FLAGS
      ${_AGENTXX_COMMON_C_CXX_FLAGS}
      -DASIO_HAS_IO_URING=ON
      -DBOOST_ASIO_HAS_IO_URING=ON
      -DASIO_HAS_IO_URING_AS_DEFAULT=ON
      -DBOOST_ASIO_HAS_IO_URING_AS_DEFAULT=ON
    )
    message("[agentxx] Enable asio/io_uring")
  endif()
endif()

message("[agentxx] CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
if (MSVC)
		set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}   ${_AGENTXX_COMMON_C_CXX_FLAGS} /utf-8 /MD /wd4200 /std:c17 /D_GNU_SOURCE")
		set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} ${_AGENTXX_COMMON_C_CXX_FLAGS} /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++latest")
		set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}   ${_AGENTXX_COMMON_C_CXX_FLAGS} /utf-8 /MD /wd4200 /std:c17 /D_GNU_SOURCE")
		set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE} ${_AGENTXX_COMMON_C_CXX_FLAGS} /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++latest")
		set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}     ${_AGENTXX_COMMON_C_CXX_FLAGS} /utf-8 /MD /wd4200 /std:c17 /D_GNU_SOURCE")
		set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}   ${_AGENTXX_COMMON_C_CXX_FLAGS} /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++latest")

    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(_AGENTXX_C_FLAGS   "${_AGENTXX_COMMON_C_CXX_FLAGS} -fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c17 -D_GNU_SOURCE")
    set(_AGENTXX_CXX_FLAGS "${_AGENTXX_COMMON_C_CXX_FLAGS} -fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c++23")
    set(LDFLAGS "${LDFLAGS} -Wl,--gc-sections")

    if (DEFINED CMAKE_CONFIGURATION_TYPES)
      set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}   ${_AGENTXX_C_FLAGS}   -O3 -flto")
      set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} ${_AGENTXX_CXX_FLAGS} -O3 -flto")
      set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}   ${_AGENTXX_C_FLAGS}   ")
      set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE} ${_AGENTXX_CXX_FLAGS} ")
      set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}     ${_AGENTXX_C_FLAGS}   -fno-omit-frame-pointer -fno-optimize-sibling-calls")
      set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}   ${_AGENTXX_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    else ()
      if (IS_DEBUG_D)
        set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS}     ${_AGENTXX_C_FLAGS}   -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}   ${_AGENTXX_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
      else ()
        set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS}   ${_AGENTXX_C_FLAGS}     ")
        set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${_AGENTXX_CXX_FLAGS}   ")
      endif ()
    endif ()
else ()
  message("[agentxx] 不支持的编译器")
  return()
endif ()

set(AGENTXX_EXEC_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/exec")
message("[agentxx] CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("[agentxx] AGENTXX_EXEC_INSTALL_PREFIX: ${AGENTXX_EXEC_INSTALL_PREFIX}")
message("[agentxx] CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# deps
set(agentxx_third_party_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(AGENTXX_INSTALL_DIR "${CMAKE_BINARY_DIR}/agentxx-project-install/")

# boost 系统安装库 或 自行编译
# https://github.com/boostorg/boost
# 下载 release/boost-xxx.7z 解压到 agent/third_party/boost/
# 然后编译结果到 agent/third_party/boost-build/
# cd boost/
# ./bootstrap.sh
# ./b2 --prefix=${PWD}/../boost-build
# ./b2 install --prefix=${PWD}/../boost-build
if (NOT DEFINED BOOST_ROOT OR NOT EXISTS ${BOOST_ROOT})
    # 尝试预编译目录
    set(_BOOST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost-build/")
    if (EXISTS ${_BOOST_DIR})
        set(BOOST_ROOT ${_BOOST_DIR} CACHE PATH "Boost root directory" FORCE)
    else()
        # 尝试系统 find_package
        find_package(Boost QUIET)
        if (Boost_FOUND)
            # 从 Boost_INCLUDE_DIRS 推导出根目录
            get_filename_component(BOOST_ROOT "${Boost_INCLUDE_DIRS}/.." ABSOLUTE)
            set(BOOST_ROOT ${BOOST_ROOT} CACHE PATH "Boost root directory" FORCE)
        else()
            message(FATAL_ERROR "Boost not found. Please set BOOST_ROOT.")
        endif()
    endif()
endif()

set(_AGENTXX_COMMON_CMAKE_ARGS
    # -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
    # -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
    # "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    # "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    # "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}"
    # "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}" 
    # "-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}"

    # "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
    # "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    # "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_INSTALL_PREFIX=${AGENTXX_INSTALL_DIR}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"

    "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
    "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
    "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
    "-DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}"
    "-DCMAKE_C_FLAGS_PROFILE=${CMAKE_C_FLAGS_PROFILE}"
    "-DCMAKE_CXX_FLAGS_PROFILE=${CMAKE_CXX_FLAGS_PROFILE}"
    "-DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}"
    "-DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}"
    "-DLDFLAGS=${LDFLAGS}"

    "-DBOOST_ROOT=${BOOST_ROOT}"

    -DXX_BUILD_TYPE=${XX_BUILD_TYPE}
    -DIS_DEBUG_D=${IS_DEBUG_D}
    -DIS_RELEASE_D=${IS_RELEASE_D}
    -DIS_LINUX_D=${IS_LINUX_D}
    -DIS_WIN_D=${IS_WIN_D}
    -DIS_MACOS_D=${IS_MACOS_D}  
    -DIS_ANDROID_D=${IS_ANDROID_D}
    -DIS_IOS_D=${IS_IOS_D}

    "-DAGENTXX_EXEC_INSTALL_PREFIX=${AGENTXX_EXEC_INSTALL_PREFIX}"
    "-Dagentxx_third_party_DIR=${agentxx_third_party_DIR}"
    -DAGENTXX_LINUX_IO_URING_SUPPORTED=${AGENTXX_LINUX_IO_URING_SUPPORTED}
    -DAGENTXX_WINDOWS_IOCP_SUPPORTED=${AGENTXX_WINDOWS_IOCP_SUPPORTED}
)

if (IS_WIN_D)
  set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenSSL/")
  set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include/")
  IF (IS_DEBUG_D)
    set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/MDd")
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MDd")
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MDd")
  else ()
    set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/MD")
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MD")
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MD")
  endif()
endif()

include(ExternalProject)
include(FetchContent)

# liburing
set(LIBURING_SOURCE_DIR "${agentxx_third_party_DIR}/liburing/")
set(LIBURING_INSTALL_DIR "${CMAKE_BINARY_DIR}/liburing-install")
if (AGENTXX_LINUX_IO_URING_SUPPORTED)
  set(LIBURING_BINARY_DIR "${CMAKE_BINARY_DIR}/liburing-build")
  ExternalProject_Add(
    liburing_repo
    SOURCE_DIR ${LIBURING_SOURCE_DIR}
    BINARY_DIR ${LIBURING_BINARY_DIR}
    INSTALL_DIR ${LIBURING_INSTALL_DIR}
    CONFIGURE_COMMAND 
        cd ${LIBURING_SOURCE_DIR} && 
        ${LIBURING_SOURCE_DIR}/configure
        --prefix=${LIBURING_INSTALL_DIR}
        --cc=${CMAKE_C_COMPILER}
        --cxx=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND 
      cd ${LIBURING_SOURCE_DIR} && make -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND 
      cd ${LIBURING_SOURCE_DIR} && make install
    TEST_COMMAND ""
  )
  include_directories(${LIBURING_INSTALL_DIR}/include/)
endif()

# iconv
ExternalProject_Add(
  libiconv_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/libiconv-native/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    -DICONV_ENABLE_EXTRA=OFF
    -DICONV_EMBED_MODE=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# sqlite
# https://sqlite.org/download.html
# 复制版本和链接 sqlite-amalgamation-xxx.zip 
ExternalProject_Add(
  sqlite3_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/sqlite3-cmake/"
  INSTALL_DIR ${SQLITE3_CMAKE_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${SQLITE3_CMAKE_INSTALL_DIR}"
    "-DSQLITE3_SOURCE_DIR=${agentxx_third_party_DIR}/sqlite"
    "-DSQLITE3_VERSION=3.53.2"
    "-DSQLITE3_URL=https://sqlite.org/2026/sqlite-amalgamation-3530200.zip"
    -Dsqlite3_ENABLE_THREADSAFE=ON
    -Dsqlite3_ENABLE_DYNAMIC_EXTENSIONS=OFF
    -Dsqlite3_ENABLE_MATH=ON
    -Dsqlite3_ENABLE_FTS3=OFF
    -Dsqlite3_ENABLE_FTS4=ON
    -Dsqlite3_ENABLE_FTS5=ON
    -Dsqlite3_ENABLE_RTREE=ON
    -Dsqlite3_ENABLE_SESSION=OFF
    -Dsqlite3_OMIT_DEPRECATED=ON
    -Dsqlite3_BUILD_SHARED_LIBS=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# fmt
set(FMT_INSTALL_DIR "${CMAKE_BINARY_DIR}/fmt-install")
ExternalProject_Add(
  fmt_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/fmt/"
  INSTALL_DIR ${FMT_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${FMT_INSTALL_DIR}"
    -DFMT_PEDANTIC=OFF
    -DFMT_INSTALL=ON
    -DFMT_WERROR=OFF
    -DFMT_DOC=OFF
    -DFMT_TEST=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# uchardet
set(UCHARDET_INSTALL_DIR "${CMAKE_BINARY_DIR}/uchardet-install")
ExternalProject_Add(
  uchardet_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/uchardet/"
  INSTALL_DIR ${UCHARDET_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${UCHARDET_INSTALL_DIR}"
    -DCHECK_SSE2=ON
    -DBUILD_BINARY=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# codegraph-cpp
if (AGENTXX_ENABLE_CODEGRAPH)
  set(CODEGRAPH_INSTALL_DIR "${CMAKE_BINARY_DIR}/codegraph-install")
  ExternalProject_Add(
    codegraph_repo
    DEPENDS sqlite3_repo
    SOURCE_DIR "${agentxx_third_party_DIR}/codegraph-cpp/"
    INSTALL_DIR ${CODEGRAPH_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
      "-DCMAKE_INSTALL_PREFIX=${CODEGRAPH_INSTALL_DIR}"
      -DBUILD_SHARED_LIBS=OFF
      -DBUILD_STATIC_LIBS=ON
    TEST_COMMAND ""
  )
endif()

if (AGENTXX_ENABLE_HYPERSCAN)
  # vectorscan
  set(VECTORSCAN_INSTALL_DIR "${CMAKE_BINARY_DIR}/vectorscan-install")
  ExternalProject_Add(
    vectorscan_repo
    SOURCE_DIR "${agentxx_third_party_DIR}/vectorscan/"
    INSTALL_DIR ${VECTORSCAN_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
      "-DCMAKE_INSTALL_PREFIX=${VECTORSCAN_INSTALL_DIR}"
      "-DBOOST_ROOT=${BOOST_ROOT}"
      -DBUILD_UNIT=OFF
      -DBUILD_TOOLS=OFF
      -DBUILD_EXAMPLES=OFF
      -DBUILD_BENCHMARKS=OFF
      -DBUILD_DOC=OFF
      -DBUILD_SHARED_LIBS=OFF
      -DBUILD_STATIC_LIBS=ON 
      -DFAT_RUNTIME=OFF
      -DUSE_CPU_NATIVE=ON 
      -DBUILD_AVX2=ON 
      -DBUILD_AVX512=ON 
      -DBUILD_AVX512VBMI=ON 
      -DBUILD_SIMDE_BACKEND=OFF
      -DBUILD_SIMDE_NATIVE=OFF
    TEST_COMMAND ""
  )
else()
  message("[agentxx] Disable Hyperscan")
endif()

# neograph
set(NEOGRAPH_INSTALL_DIR "${CMAKE_BINARY_DIR}/neograph-install")
ExternalProject_Add(
  neograph_repo
  DEPENDS sqlite3_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/neograph/"
  INSTALL_DIR ${NEOGRAPH_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${NEOGRAPH_INSTALL_DIR}"
    "-DBOOST_ROOT=${BOOST_ROOT}"
    -DNEOGRAPH_BUILD_EXAMPLES=OFF
    -DNEOGRAPH_BUILD_TESTS=OFF
    -DNEOGRAPH_BUILD_PYBIND=OFF
    -DNEOGRAPH_BUILD_A2A=OFF
    -DNEOGRAPH_BUILD_POSTGRES=OFF
    -DNEOGRAPH_BUILD_SQLITE=ON
    -DNEOGRAPH_BUILD_ASYNC=ON
    -DNEOGRAPH_BUILD_LLM=ON
    -DNEOGRAPH_BUILD_MCP=ON
    -DNEOGRAPH_BUILD_ACP=ON
    -DNEOGRAPH_BUILD_UTIL=ON
    -DNEOGRAPH_USE_LIBCURL=OFF
    -DNEOGRAPH_INSTALL_HEADERS=ON
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# html2md
set(HTML2MD_INSTALL_DIR "${CMAKE_BINARY_DIR}/html2md-install")
ExternalProject_Add(
  html2md_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/html2md/"
  INSTALL_DIR ${HTML2MD_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${HTML2MD_INSTALL_DIR}"
    -DBUILD_EXE=OFF
    -DBUILD_DOC=OFF
    -DBUILD_TEST=OFF
    -DPYTHON_BINDINGS=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# yaml-cpp
set(YAML_CPP_INSTALL_DIR "${CMAKE_BINARY_DIR}/yaml-cpp-install")
ExternalProject_Add(
  yaml_cpp_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/yaml-cpp/"
  INSTALL_DIR ${YAML_CPP_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${YAML_CPP_INSTALL_DIR}"
    -DYAML_CPP_BUILD_CONTRIB=OFF
    -DYAML_CPP_BUILD_TOOLS=ON
    -DYAML_BUILD_SHARED_LIBS=OFF
    -DYAML_ENABLE_PIC=ON
    -DYAML_CPP_BUILD_TESTS=OFF
    -DYAML_CPP_INSTALL=ON
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON 
  TEST_COMMAND ""
)

# glob
set(GLOB_INSTALL_DIR "${CMAKE_BINARY_DIR}/glob-install")
ExternalProject_Add(
  glob_repo
  SOURCE_DIR "${agentxx_third_party_DIR}/glob/"
  INSTALL_DIR ${GLOB_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${GLOB_INSTALL_DIR}"
  TEST_COMMAND ""
)

set(LIBAGENTXX_INSTALL_DIR "${CMAKE_BINARY_DIR}/agentxx-install")
ExternalProject_Add(
  libagentxx_repo
  DEPENDS 
    liburing_repo
    libiconv_repo
    sqlite3_repo
    fmt_repo
    uchardet_repo
    codegraph_repo
    vectorscan_repo
    neograph_repo
    html2md_repo
    yaml_cpp_repo
    glob_repo
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/"
  INSTALL_DIR ${LIBAGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DCMAKE_INSTALL_PREFIX=${LIBAGENTXX_INSTALL_DIR}"
  TEST_COMMAND ""
)

if (AGENTXX_BUILD_CLIENT)
  set(AGENTXX_CLI_INSTALL_DIR "${CMAKE_BINARY_DIR}/agentxx-cli-install")
  ExternalProject_Add(
    agentxx_client_repo
    DEPENDS libagentxx_repo
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/client/"
    INSTALL_DIR ${AGENTXX_CLI_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
      "-DCMAKE_INSTALL_PREFIX=${AGENTXX_CLI_INSTALL_DIR}"
    TEST_COMMAND ""
  )
endif()
if (AGENTXX_BUILD_TEST)
  set(AGENTXX_TEST_INSTALL_DIR "${CMAKE_BINARY_DIR}/agentxx-test-install")
  ExternalProject_Add(
    agentxx_test_repo
    DEPENDS libagentxx_repo
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/"
    INSTALL_DIR ${AGENTXX_TEST_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
      "-DCMAKE_INSTALL_PREFIX=${AGENTXX_TEST_INSTALL_DIR}"
    TEST_COMMAND ""
  )
endif()
