# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0091 NEW)

project(agentxx_project VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XX_BUILD_TYPE "Generate the client target." "RELEASE")
option(AGENTXX_BUILD_CLIENT "Generate the client target." OFF)
option(AGENTXX_BUILD_TEST "Generate the test target." OFF)
option(AGENTXX_ENABLE_HYPERSCAN "Add HyperScan for regex support. Use std::regex if disable." ON)
option(AGENTXX_ENABLE_CODEGRAPH "Add CodeGraph-CPP for code analysis." ON)
option(AGENTXX_ENABLE_CUSTOM_CURL "Use custom curl." ON)

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

# /D_WIN32_WINNT=0x0A00 限制 win10 或更高:
# - httplib 要求 >= win10
if (MSVC) 
  set(_AGENTXX_COMMON_C_CXX_FLAGS 
    "${_AGENTXX_COMMON_C_CXX_FLAGS} /D_WIN32_WINNT=0x0A00 /D__STDC_CONSTANT_MACROS=1 /DCPPHTTPLIB_OPENSSL_SUPPORT=1 /DBOOST_ASIO_STANDALONE=1 /DASIO_STANDALONE=1 /DBOOST_PROCESS_USE_STD_FS=1"
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
  if (IS_WIN_D)
    # 可能交叉编译
    set(_AGENTXX_COMMON_C_CXX_FLAGS
      ${_AGENTXX_COMMON_C_CXX_FLAGS}
      -D_WIN32_WINNT=0x0A00
    )
  endif()
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
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if (MSVC)
    set(_AGENTXX_C_FLAGS   "${_AGENTXX_COMMON_C_CXX_FLAGS} /bigobj /utf-8 /wd4200 /std:c17 /D_GNU_SOURCE")
    set(_AGENTXX_CXX_FLAGS "${_AGENTXX_COMMON_C_CXX_FLAGS} /bigobj /utf-8 /wd4200 /Zc:__cplusplus /std:c++latest")

		set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}   ${_AGENTXX_C_FLAGS}   /Gy")
		set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} ${_AGENTXX_CXX_FLAGS} /Gy")
		set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}   ${_AGENTXX_C_FLAGS}")
		set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE} ${_AGENTXX_CXX_FLAGS}")
		set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}     ${_AGENTXX_C_FLAGS}")
		set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}   ${_AGENTXX_CXX_FLAGS}")

    if (IS_RELEASE_D)
      set(LDFLAGS "${LDFLAGS} /OPT:REF /OPT:ICF")
    else ()
      # windows/msvc 使用 /fsanitize=address 时可能导致链接错误 (uchatdet)
      set(LDFLAGS "${LDFLAGS}")
    endif()

    unset(_AGENTXX_C_FLAGS)
    unset(_AGENTXX_CXX_FLAGS)

    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(_AGENTXX_C_FLAGS   "${_AGENTXX_COMMON_C_CXX_FLAGS} -fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c17 -D_GNU_SOURCE")
    set(_AGENTXX_CXX_FLAGS "${_AGENTXX_COMMON_C_CXX_FLAGS} -fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c++23")
    
    set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}   ${_AGENTXX_C_FLAGS}   -O3 -flto")
    set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} ${_AGENTXX_CXX_FLAGS} -O3 -flto")
    set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}   ${_AGENTXX_C_FLAGS}   ")
    set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE} ${_AGENTXX_CXX_FLAGS} ")
    set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}     ${_AGENTXX_C_FLAGS}   -fsanitize=address -g -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}   ${_AGENTXX_CXX_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer -fno-optimize-sibling-calls")

    if (IS_RELEASE_D)
      set(LDFLAGS "${LDFLAGS} -Wl,--gc-sections")
    else ()
      set(LDFLAGS "${LDFLAGS} -fsanitize=address")
    endif ()

    unset(_AGENTXX_C_FLAGS)
    unset(_AGENTXX_CXX_FLAGS)
else ()
  message("[agentxx] 不支持的编译器")
  return()
endif ()

# exe install dir
set(AGENTXX_EXEC_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/exec")
message("[agentxx] AGENTXX_EXEC_INSTALL_PREFIX: ${AGENTXX_EXEC_INSTALL_PREFIX}")
message("[agentxx] CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("[agentxx] CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# deps
set(AGENTXX_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(AGENTXX_INSTALL_DIR "${CMAKE_BINARY_DIR}/agentxx-project-install/")

# boost 系统安装库 或 自行编译
# https://github.com/boostorg/boost
# 下载 release/boost-xxx.7z 解压到 agent/third_party/boost/
# 自行按文档编译参考: /docs/build/linux.md 或 /docs/build/windows.md
set (Boost_USE_STATIC_LIBS ON)
set (Boost_USE_STATIC_RUNTIME OFF) # 静态链接标准库
set (Boost_USE_DEBUG_LIBS ${IS_DEBUG_D})
set (Boost_USE_RELEASE_LIBS ${IS_RELEASE_D})
set (Boost_USE_DEBUG_RUNTIME ${IS_DEBUG_D})
set (Boost_USE_RELEASE_RUNTIME ${IS_RELEASE_D})
if (NOT DEFINED BOOST_ROOT OR NOT EXISTS ${BOOST_ROOT})
    # 尝试预编译目录，按顺序:
    #   - ${BOOST_ROOT}
    #   - boost-build-debug / boost-build-release
    #   - boost-build
    #   - find_package
    if (IS_DEBUG_D)
      set(_BOOST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost-build-debug/")
    else ()
      set(_BOOST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost-build-release/")
    endif()
    if (NOT EXISTS ${_BOOST_DIR})
      set(_BOOST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost-build/")
    endif ()

    if (EXISTS ${_BOOST_DIR})
      set(BOOST_ROOT ${_BOOST_DIR} CACHE PATH "Boost root directory" FORCE)
    else ()
        # 尝试系统 find_package
        find_package(Boost REQUIRED)
        if (NOT Boost_FOUND)
            message(FATAL_ERROR "Boost not found. Please set BOOST_ROOT.")
        endif()
    endif()

    unset(_BOOST_DIR)
endif()

# openssl
set (OPENSSL_USE_STATIC_LIBS ON)
if (NOT DEFINED OPENSSL_ROOT_DIR OR NOT EXISTS ${OPENSSL_ROOT_DIR})
  # 预编译目录
  set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenSSL/")
  set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include/")

  if (IS_WIN_D)
    # 动态链接标准库
    if (IS_DEBUG_D)
      set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/MDd")
      set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MDd")
      set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MDd")
    else ()
      set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC/MD")
      set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MD")
      set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/MD")
    endif()
  else()
    find_package(OpenSSL REQUIRED)
    if (NOT OpenSSL_FOUND)
        message(FATAL_ERROR "OpenSSL not found. Please set OPENSSL_ROOT.")
    endif()
  endif()
endif()

set(_AGENTXX_COMMON_CMAKE_ARGS
    "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
    "-DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
    "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}"
    "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}" 
    "-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}"

    "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_PREFIX_PATH=${AGENTXX_INSTALL_DIR}"
    "-DCMAKE_INSTALL_PREFIX=${AGENTXX_INSTALL_DIR}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

    -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW

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
    -DBOOST_USE_STATIC_LIBS=${Boost_USE_STATIC_LIBS}
    -DBoost_USE_STATIC_RUNTIME=${Boost_USE_STATIC_RUNTIME}
    -DBoost_USE_DEBUG_LIBS=${Boost_USE_DEBUG_LIBS}
    -DBoost_USE_RELEASE_LIBS=${Boost_USE_RELEASE_LIBS}
    -DBoost_USE_DEBUG_RUNTIME=${Boost_USE_DEBUG_RUNTIME}
    -DBoost_USE_RELEASE_RUNTIME=${Boost_USE_RELEASE_RUNTIME}
    "-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}"
    "-DOPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}"
    "-DOPENSSL_LIB_DIR=${OPENSSL_LIB_DIR}"
    "-DOPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}"
    "-DOPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}"
    -DOPENSSL_USE_STATIC_LIBS=${OPENSSL_USE_STATIC_LIBS}

    -DXX_BUILD_TYPE=${XX_BUILD_TYPE}
    -DIS_DEBUG_D=${IS_DEBUG_D}
    -DIS_RELEASE_D=${IS_RELEASE_D}
    -DIS_LINUX_D=${IS_LINUX_D}
    -DIS_WIN_D=${IS_WIN_D}
    -DIS_MACOS_D=${IS_MACOS_D}  
    -DIS_ANDROID_D=${IS_ANDROID_D}
    -DIS_IOS_D=${IS_IOS_D}

    "-DAGENTXX_EXEC_INSTALL_PREFIX=${AGENTXX_EXEC_INSTALL_PREFIX}"
    "-DAGENTXX_THIRD_PARTY_DIR=${AGENTXX_THIRD_PARTY_DIR}"
    -DAGENTXX_LINUX_IO_URING_SUPPORTED=${AGENTXX_LINUX_IO_URING_SUPPORTED}
    -DAGENTXX_WINDOWS_IOCP_SUPPORTED=${AGENTXX_WINDOWS_IOCP_SUPPORTED}
)

include(ExternalProject)
include(FetchContent)

# liburing
set(LIBURING_SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/liburing/")
if (AGENTXX_LINUX_IO_URING_SUPPORTED)
  set(LIBURING_BINARY_DIR "${CMAKE_BINARY_DIR}/liburing-build")
  ExternalProject_Add(
    liburing_repo
    SOURCE_DIR ${LIBURING_SOURCE_DIR}
    BINARY_DIR ${LIBURING_BINARY_DIR}
    INSTALL_DIR ${AGENTXX_INSTALL_DIR}
    CONFIGURE_COMMAND 
        cd ${LIBURING_SOURCE_DIR} && 
        ${LIBURING_SOURCE_DIR}/configure
        --prefix=${AGENTXX_INSTALL_DIR}
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
  SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/libiconv-native/"
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
  SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/sqlite3-cmake/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    "-DSQLITE3_SOURCE_DIR=${AGENTXX_THIRD_PARTY_DIR}/sqlite"
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
ExternalProject_Add(
  fmt_repo
  SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/fmt/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    -DFMT_PEDANTIC=ON
    -DFMT_INSTALL=ON
    -DFMT_WERROR=OFF
    -DFMT_DOC=OFF
    -DFMT_TEST=OFF
    -DFMT_OS=OFF
    -DFMT_UNICODE=ON
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# uchardet
ExternalProject_Add(
  uchardet_repo
  SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/uchardet/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    -DCHECK_SSE2=ON
    -DBUILD_BINARY=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

if (AGENTXX_ENABLE_CUSTOM_CURL)
  # curl
  ExternalProject_Add(
    curl_repo
    SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/curl/"
    INSTALL_DIR ${AGENTXX_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
      -DBUILD_CURL_EXE=OFF
      -DCURL_DISABLE_INSTALL=OFF
      -DBUILD_CURL_EXE=OFF
      -DCURL_BUILD_EVERYTHING=OFF
      -DENABLE_UNICODE=ON
      -DCURL_LTO=ON
      -DENABLE_DEBUG=OFF
      -DCURL_DROP_UNUSED=ON
      -DCURL_USE_CMAKECONFIG=ON
      -DCURL_USE_PKGCONFIG=ON
      -DCURL_ENABLE_EXPORT_TARGET=ON
      -DUSE_LIBIDN2=OFF
      -DCURL_USE_LIBPSL=OFF
      -DCURL_USE_LIBSSH2=OFF
      -DCURL_USE_LIBSSH=OFF
      -DCURL_USE_GSASL=OFF
      -DCURL_USE_GSSAPI=OFF
      -DCURL_USE_LIBBACKTRACE=OFF
      -DBUILD_LIBCURL_DOCS=OFF
      -DBUILD_MISC_DOCS=OFF
      -DENABLE_CURL_MANUAL=OFF
      -DENABLE_IPV6=ON
      -DCURL_ENABLE_SSL=ON
      -DCURL_USE_OPENSSL=ON
      -DBUILD_TESTING=OFF
      -DBUILD_EXAMPLES=OFF
      -DBUILD_SHARED_LIBS=OFF
      -DBUILD_STATIC_LIBS=ON
    TEST_COMMAND ""
  )
endif()

# codegraph-cpp
if (AGENTXX_ENABLE_CODEGRAPH)
  ExternalProject_Add(
    codegraph_repo
    DEPENDS sqlite3_repo
    SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/codegraph-cpp/"
    INSTALL_DIR ${AGENTXX_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
      -DBUILD_SHARED_LIBS=OFF
      -DBUILD_STATIC_LIBS=ON
    TEST_COMMAND ""
  )
endif()

if (AGENTXX_ENABLE_HYPERSCAN)
  # vectorscan
  ExternalProject_Add(
    vectorscan_repo
    SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/vectorscan/"
    INSTALL_DIR ${AGENTXX_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
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
      -DSANITIZE=""
      -DBUILD_SIMDE_BACKEND=OFF
      -DBUILD_SIMDE_NATIVE=OFF
    TEST_COMMAND ""
  )
else()
  message("[agentxx] Disable Hyperscan")
endif()

# neograph
set(neograph_SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/neograph/")
ExternalProject_Add(
  neograph_repo
  DEPENDS sqlite3_repo
  SOURCE_DIR ${neograph_SOURCE_DIR}
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
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
if (AGENTXX_ENABLE_CUSTOM_CURL)
  add_dependencies(neograph_repo curl_repo)
endif()

# html2md
ExternalProject_Add(
  html2md_repo
  SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/html2md/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    -DBUILD_EXE=OFF
    -DBUILD_DOC=OFF
    -DBUILD_TEST=OFF
    -DPYTHON_BINDINGS=OFF
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
  TEST_COMMAND ""
)

# yaml-cpp
ExternalProject_Add(
  yaml_cpp_repo
  SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/yaml-cpp/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
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
ExternalProject_Add(
  glob_repo
  SOURCE_DIR "${AGENTXX_THIRD_PARTY_DIR}/glob/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
  TEST_COMMAND ""
)

ExternalProject_Add(
  agentxx_lib_repo
  DEPENDS 
    libiconv_repo
    sqlite3_repo
    fmt_repo
    uchardet_repo
    neograph_repo
    html2md_repo
    yaml_cpp_repo
    glob_repo
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/"
  INSTALL_DIR ${AGENTXX_INSTALL_DIR}
  CMAKE_ARGS
    ${_AGENTXX_COMMON_CMAKE_ARGS}
    -Dneograph_SOURCE_DIR=${neograph_SOURCE_DIR}
  TEST_COMMAND ""
)
if (AGENTXX_LINUX_IO_URING_SUPPORTED)
  add_dependencies(agentxx_lib_repo liburing_repo)
endif()
if (AGENTXX_ENABLE_HYPERSCAN)
  add_dependencies(agentxx_lib_repo vectorscan_repo)
endif()
if (AGENTXX_ENABLE_CODEGRAPH)
  add_dependencies(agentxx_lib_repo codegraph_repo)
endif()

if (AGENTXX_BUILD_CLIENT)
  ExternalProject_Add(
    agentxx_client_repo
    DEPENDS agentxx_lib_repo
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/client/"
    INSTALL_DIR ${AGENTXX_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
    TEST_COMMAND ""
  )
endif()
if (AGENTXX_BUILD_TEST)
  ExternalProject_Add(
    agentxx_test_repo
    DEPENDS agentxx_lib_repo
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/"
    INSTALL_DIR ${AGENTXX_INSTALL_DIR}
    CMAKE_ARGS
      ${_AGENTXX_COMMON_CMAKE_ARGS}
    TEST_COMMAND ""
  )
endif()