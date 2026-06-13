# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0097 NEW)

project(agentxx VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XX_BUILD_TYPE "Generate the client target." "RELEASE")
option(AGENTXX_BUILD_CLIENT "Generate the client target." OFF)
option(AGENTXX_BUILD_TEST "Generate the test target." OFF)

# 仅用于指示是否编译用于调试用途，不代表真实编译类型
# - [IS_DEBUG_D] == 1 时，启用大部分调试日志等信息，因此 profile 之类注重
# 性能的编译将视为 release
set(IS_DEBUG_D    0)
set(IS_RELEASE_D  1)

if (XX_BUILD_TYPE MATCHES "DEBUG")
  set(IS_DEBUG_D    1)
  set(IS_RELEASE_D  0)
endif()
add_definitions(-DIS_DEBUG_D=${IS_DEBUG_D})
add_definitions(-DIS_RELEASE_D=${IS_RELEASE_D})
add_definitions(-DXX_BUILD_TYPE=${XX_BUILD_TYPE})

set(IS_LINUX_D    0)
set(IS_WIN_D      0)
set(IS_MACOS_D    0)
set(IS_ANDROID_D  0)
set(IS_IOS_D      0)

if (WIN32)
  set(IS_WIN_D 1)

  set(WINDOWS_IOCP_SUPPORTED ON)
elseif (ANDROID)
  set(IS_ANDROID_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(IS_MACOS_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(IS_IOS_D 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(IS_LINUX_D 1)

  # 检查是否支持 [LINUX_IO_URING_SUPPORTED]
  file(READ "/usr/include/linux/version.h" _linux_version_h)
  string(REGEX MATCH "#define LINUX_VERSION_CODE ([0-9]+)" _match "${_linux_version_h}")
  if(_match AND (CMAKE_MATCH_1 GREATER_EQUAL 330240))  # >= 5.10.0
      set(LINUX_IO_URING_SUPPORTED ON)
  else()
      set(LINUX_IO_URING_SUPPORTED OFF)
  endif()
endif ()

add_definitions(-DIS_LINUX_D=${IS_LINUX_D})
add_definitions(-DIS_WIN_D=${IS_WIN_D})
add_definitions(-DIS_MACOS_D=${IS_MACOS_D})
add_definitions(-DIS_ANDROID_D=${IS_ANDROID_D})
add_definitions(-DIS_IOS_D=${IS_IOS_D})

# asio
if (WINDOWS_IOCP_SUPPORTED)
  add_definitions(-DASIO_HAS_IOCP=ON)
  add_definitions(-DBOOST_ASIO_HAS_IOCP=ON)
  add_definitions(-DASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE=ON)
  add_definitions(-DBOOST_ASIO_HAS_WINDOWS_RANDOM_ACCESS_HANDLE=ON)
  message("[agentxx] Enable asio/iocp")
endif()
if (LINUX_IO_URING_SUPPORTED)
  add_definitions(-DASIO_HAS_IO_URING=ON)
  add_definitions(-DBOOST_ASIO_HAS_IO_URING=ON)
  add_definitions(-DASIO_HAS_IO_URING_AS_DEFAULT=ON)
  add_definitions(-DBOOST_ASIO_HAS_IO_URING_AS_DEFAULT=ON)
  message("[agentxx] Enable asio/io_uring")
endif()

message("[agentxx] Current Configure Type: ${CMAKE_CONFIGURATION_TYPES}")
message("[agentxx] Current Build Type: ${CMAKE_BUILD_TYPE}")

message("[agentxx] CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
if (MSVC)
		set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}    /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE}  /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")
		set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}    /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE}  /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")
		set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}      /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}    /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")

    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(_AGENTXX_C_FLAGS   "-fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c17")
    set(_AGENTXX_CXX_FLAGS "-fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c++23")
    set(LDFLAGS "${LDFLAGS} -Wl,--gc-sections")

    if (DEFINED CMAKE_CONFIGURATION_TYPES)
      set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}   ${_AGENTXX_C_FLAGS}   ")
      set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} ${_AGENTXX_CXX_FLAGS} ")
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

set(EXE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/exec")
message("[agentxx] CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("[agentxx] EXE_INSTALL_PREFIX: ${EXE_INSTALL_PREFIX}")
message("[agentxx] CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

add_definitions(-D__STDC_CONSTANT_MACROS)

# deps
set(third_party_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")

include(ExternalProject)
include(FetchContent)

# boost
# https://github.com/boostorg/boost
# 下载 release/boost-xxx.7z 解压到 agent/third_party/boost/
# 然后编译结果到 agent/third_party/boost-build/
# cd boost/
# ./bootstrap.sh
# ./b2 --prefix=${PWD}/../boost-build
# ./b2 install --prefix=${PWD}/../boost-build
set(Boost_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/boost-build/")
add_definitions(-DBOOST_PROCESS_V2_STANDALONE=1)
add_definitions(-DBOOST_ASIO_STANDALONE=1)
add_definitions(-DASIO_STANDALONE=1)

include_directories(${Boost_ROOT}/include/)

# iconv
set(LIBICONV_VERSION 1.18)
set(LIBICONV_URL "https://ftp.gnu.org/gnu/libiconv/libiconv-${LIBICONV_VERSION}.tar.gz")
set(LIBICONV_PREFIX ${CMAKE_BINARY_DIR}/libiconv-src)
set(LIBICONV_SOURCE_DIR ${LIBICONV_PREFIX}/src/iconv_repo)
set(LIBICONV_BINARY_DIR ${CMAKE_BINARY_DIR}/libiconv-build)
set(LIBICONV_INSTALL_DIR ${CMAKE_BINARY_DIR}/libiconv-install)
ExternalProject_Add(
  iconv_repo
  URL ${LIBICONV_URL}
  PREFIX ${LIBICONV_PREFIX} 
  SOURCE_DIR ${LIBICONV_SOURCE_DIR}
  BINARY_DIR ${LIBICONV_BINARY_DIR} 
  INSTALL_DIR ${LIBICONV_INSTALL_DIR} 
  CONFIGURE_COMMAND 
      cd ${LIBICONV_SOURCE_DIR} && ./configure
      --prefix=${LIBICONV_INSTALL_DIR} 
      --host=${CMAKE_C_COMPILER_TARGET}
      --enable-static=yes
      --enable-shared=no
      --enable-pic
      --disable-rpath
      --disable-nls
      --disable-libiconv-close
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      AR=${CMAKE_AR}
      RANLIB=${CMAKE_RANLIB}
      "CFLAGS=${CMAKE_C_FLAGS} -DANDROID_PLATFORM=${ANDROID_PLATFORM} -DANDROID_ABI=${ANDROID_ABI} -DANDROID_NDK=${ANDROID_NDK} -DANDROID_STL=${ANDROID_STL}"
  BUILD_COMMAND 
    cd ${LIBICONV_SOURCE_DIR} && make -j${CMAKE_BUILD_PARALLEL_LEVEL}
  INSTALL_COMMAND 
    cd ${LIBICONV_SOURCE_DIR} && make install
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

# liburing
set(LIBURING_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/liburing/)
set(LIBURING_INSTALL_DIR ${CMAKE_BINARY_DIR}/liburing-install)
if (LINUX_IO_URING_SUPPORTED)
  set(LIBURING_PREFIX ${CMAKE_BINARY_DIR}/liburing-src)
  set(LIBURING_BINARY_DIR ${CMAKE_BINARY_DIR}/liburing-build)
  ExternalProject_Add(
    liburing_repo
    PREFIX ${LIBURING_PREFIX}
    SOURCE_DIR ${LIBURING_SOURCE_DIR}
    BINARY_DIR ${LIBURING_BINARY_DIR}
    INSTALL_DIR ${LIBURING_INSTALL_DIR}
    CMAKE_ARGS 
      -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
      -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}"
      "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}" 
      "-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}"

      "-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}"
      "-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
      "-DANDROID_ABI=${ANDROID_ABI}"
      "-DANDROID_NDK=${ANDROID_NDK}"
      "-DANDROID_STL=${ANDROID_STL}"
      "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
      "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"

      "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
      "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"

      "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
      "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
      "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
      "-DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}"
      "-DCMAKE_C_FLAGS_PROFILE=${CMAKE_C_FLAGS_PROFILE}"
      "-DCMAKE_CXX_FLAGS_PROFILE=${CMAKE_CXX_FLAGS_PROFILE}"
      "-DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}"
      "-DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}"
    CONFIGURE_COMMAND 
        cd ${LIBURING_SOURCE_DIR} && ./configure
        --prefix=${LIBURING_INSTALL_DIR}
        --cc=${CMAKE_C_COMPILER}
        --cxx=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND 
      cd ${LIBURING_SOURCE_DIR} && make -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND 
      cd ${LIBURING_SOURCE_DIR} && make install
    UPDATE_COMMAND ""
    TEST_COMMAND ""
  )
endif()

# sqlite
set(SQLITE_PREFIX ${CMAKE_BINARY_DIR}/sqlite-src)
set(SQLITE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/")
set(SQLITE_BINARY_DIR ${CMAKE_BINARY_DIR}/sqlite-build)
set(SQLITE_INSTALL_DIR ${CMAKE_BINARY_DIR}/sqlite-install)
set(SQLITE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
ExternalProject_Add(
  sqlite_repo
  PREFIX ${SQLITE_PREFIX}
  SOURCE_DIR ${SQLITE_SOURCE_DIR}
  BINARY_DIR ${SQLITE_BINARY_DIR}
  INSTALL_DIR ${SQLITE_INSTALL_DIR}
  CONFIGURE_COMMAND 
      cd ${SQLITE_SOURCE_DIR} && ./configure
      --prefix=${SQLITE_INSTALL_DIR} 
      --host=${CMAKE_C_COMPILER_TARGET}
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      AR=${CMAKE_AR}
      RANLIB=${CMAKE_RANLIB}
      "CFLAGS=${SQLITE_C_FLAGS} -DANDROID_PLATFORM=${ANDROID_PLATFORM} -DANDROID_ABI=${ANDROID_ABI} -DANDROID_NDK=${ANDROID_NDK} -DANDROID_STL=${ANDROID_STL}"
  BUILD_COMMAND 
    cd ${SQLITE_SOURCE_DIR} && make -j${CMAKE_BUILD_PARALLEL_LEVEL}
  INSTALL_COMMAND 
    cd ${SQLITE_SOURCE_DIR} && make install
  UPDATE_COMMAND ""
  TEST_COMMAND ""
)

# fmt
FetchContent_Declare(
  fmt
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/"
)
block()
  set(FMT_PEDANTIC          OFF)
  set(FMT_INSTALL           OFF)
  set(FMT_WERROR            OFF)
  set(BUILD_SHARED_LIBS     OFF)
  set(BUILD_STATIC_LIBS     ON )
  set(FMT_DOC               OFF)
  set(FMT_TEST              OFF)
  FetchContent_MakeAvailable(fmt)
endblock()

# uchardet
FetchContent_Declare(
  uchardet
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uchardet/"
  EXCLUDE_FROM_ALL
)
block()
  set(CHECK_SSE2            ON)
  set(BUILD_BINARY          OFF)
  set(BUILD_SHARED_LIBS     OFF)
  set(BUILD_STATIC_LIBS     ON )
  FetchContent_MakeAvailable(uchardet)
endblock()

# codegraph-cpp
FetchContent_Declare(
  codegraph
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/codegraph-cpp/"
)
block()
  set(BUILD_SHARED_LIBS             OFF)
  set(BUILD_STATIC_LIBS             ON)
  FetchContent_MakeAvailable(codegraph)
endblock()

# vectorscan
FetchContent_Declare(
  vectorscan
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vectorscan/"
  EXCLUDE_FROM_ALL
)
block()
  set(BUILD_UNIT            OFF)
  set(BUILD_TOOLS           OFF)
  set(BUILD_EXAMPLES        OFF)
  set(BUILD_BENCHMARKS      OFF)
  set(BUILD_DOC             OFF)
  set(BUILD_SHARED_LIBS     OFF)
  set(BUILD_STATIC_LIBS     ON )
  set(FAT_RUNTIME           OFF)
  set(USE_CPU_NATIVE        ON )
  set(BUILD_AVX2            ON )
  set(BUILD_AVX512          ON )
  set(BUILD_AVX512VBMI      ON )
  set(BUILD_SIMDE_BACKEND   OFF)
  set(BUILD_SIMDE_NATIVE    OFF)
  # set(BUILD_SVE            ON)
  # set(BUILD_SVE2           ON)
  # set(BUILD_SVE2_BITPERM   ON)
  FetchContent_MakeAvailable(vectorscan)
endblock()

# neograph
FetchContent_Declare(
  NeoGraph
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/neograph/"
  EXCLUDE_FROM_ALL
)
block()
  if (LINUX_IO_URING_SUPPORTED)
    include_directories(${LIBURING_INSTALL_DIR}/include/)
  endif()
  set(NEOGRAPH_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_PYBIND    OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_A2A       OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_POSTGRES  OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_SQLITE    ON  CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_ASYNC     ON  CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_LLM       ON  CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_MCP       ON  CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_ACP       ON  CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_UTIL      ON  CACHE BOOL "" FORCE)
  set(NEOGRAPH_USE_LIBCURL     ON  CACHE BOOL "" FORCE)
  set(NEOGRAPH_INSTALL_HEADERS OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS             OFF)
  set(BUILD_STATIC_LIBS             ON)
  FetchContent_MakeAvailable(NeoGraph)
endblock()

# html2md
FetchContent_Declare(
  html2md
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/html2md/"
  EXCLUDE_FROM_ALL
)
block()
  set(BUILD_EXE                 OFF)
  set(BUILD_DOC                 OFF)
  set(BUILD_TEST                OFF)
  set(PYTHON_BINDINGS           OFF)
  set(BUILD_SHARED_LIBS         OFF)
  set(BUILD_STATIC_LIBS         ON)
  FetchContent_MakeAvailable(html2md)
endblock()

# yaml-cpp
FetchContent_Declare(
  yaml-cpp
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yaml-cpp/"
  EXCLUDE_FROM_ALL
)
block()
  set(YAML_CPP_BUILD_CONTRIB          OFF)
  set(YAML_CPP_BUILD_TOOLS            ON)
  set(YAML_BUILD_SHARED_LIBS          OFF)
  set(YAML_CPP_INSTALL                OFF)
  set(YAML_ENABLE_PIC                 ON)
  set(YAML_CPP_BUILD_TESTS            ON)
  set(BUILD_SHARED_LIBS               OFF)
  set(BUILD_STATIC_LIBS               ON )
  FetchContent_MakeAvailable(yaml-cpp)
endblock()

# glob
set(glob_SOURCE_DIR "${third_party_DIR}/glob/")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/)
include_directories(${fmt_SOURCE_DIR}/include/)
include_directories(${uchardet_INSTALL_DIR}/include/)
include_directories(${LIBICONV_INSTALL_DIR}/include/)
include_directories(${LIBURING_INSTALL_DIR}/include/)
include_directories(${SQLITE_SOURCE_DIR}/src/)
include_directories(${vectorscan_SOURCE_DIR}/src/)
include_directories(${codegraph_SOURCE_DIR}/include/)
include_directories(${neograph_SOURCE_DIR}/deps/)
include_directories(${neograph_SOURCE_DIR}/deps/asio/include/)
include_directories(${html2md_SOURCE_DIR}/include/)
include_directories(${glob_SOURCE_DIR}/single_include/)
include_directories(${third_party_DIR})

aux_source_directory("./lib/util" DIR_UTIL)
aux_source_directory("./lib/tools" DIR_TOOLS)
aux_source_directory("./lib/nodes" DIR_NODES)
aux_source_directory("./lib/middlewares" DIR_MIDDLEWARES)
aux_source_directory("./lib/agent" DIR_AGENT)

add_library(agentxx SHARED
  ${DIR_UTIL}
  ${DIR_TOOLS}
  ${DIR_NODES}
  ${DIR_MIDDLEWARES}
  ${DIR_AGENT}
)

add_dependencies(agentxx iconv_repo)
add_dependencies(agentxx sqlite_repo)
if (LINUX_IO_URING_SUPPORTED)
  add_dependencies(agentxx liburing_repo)
endif()

target_link_directories(agentxx PUBLIC 
  ${fmt_INSTALL_DIR}
  ${uchardet_INSTALL_DIR}/lib/
  ${LIBICONV_INSTALL_DIR}/lib/
  ${vectorscan_INSTALL_DIR}/lib/
  ${SQLITE_INSTALL_DIR}/lib/
  ${codegraph_INSTALL_DIR}
  ${neograph_INSTALL_DIR}
  ${Boost_ROOT}/lib/
)
if (LINUX_IO_URING_SUPPORTED)
  target_link_directories(agentxx PUBLIC 
    ${LIBURING_INSTALL_DIR}/lib/
  )
endif()

target_link_libraries(agentxx PUBLIC
  fmt::fmt
  html2md
  iconv
  libuchardet
  sqlite3
  neograph::core
  neograph::util
  neograph::async
  neograph::llm
  neograph::mcp
  neograph::acp
  codegraph_core
	hs_runtime
	hs
  yaml-cpp::yaml-cpp
)
if (LINUX_IO_URING_SUPPORTED)
  target_link_libraries(agentxx PUBLIC
    liburing.a 
    liburing-ffi.a
  )
endif()

# 统一输出名称 libagentxx
# - 不指定时，部分系统，如win可能编译输出缺少前缀 lib
set_target_properties(agentxx PROPERTIES
  PUBLIC_HEADER agentxx.h
  PREFIX "lib"
  OUTPUT_NAME "agentxx"
)

target_compile_definitions(agentxx PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

if (AGENTXX_BUILD_CLIENT)
  ADD_SUBDIRECTORY(client)
  add_dependencies(client agentxx)
endif()
if (AGENTXX_BUILD_TEST) 
  ADD_SUBDIRECTORY(test)
  add_dependencies(test agentxx)
endif()

if (DEFINED XX_BUILD_TYPE)
  install(TARGETS agentxx DESTINATION "${EXE_INSTALL_PREFIX}")
endif()
