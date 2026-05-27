# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0097 NEW)

project(agentxx VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IS_LINUX_D    0)
set(IS_WIN_D      0)
set(IS_MACOS_D    0)
set(IS_ANDROID_D  0)
set(IS_IOS_D      0)

if (WIN32)
  set(IS_WIN_D 1)
elseif (ANDROID)
  set(IS_ANDROID_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(IS_MACOS_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(IS_IOS_D 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(IS_LINUX_D 1)
endif ()

add_definitions(-DIS_LINUX_D=${IS_LINUX_D})
add_definitions(-DIS_WIN_D=${IS_WIN_D})
add_definitions(-DIS_MACOS_D=${IS_MACOS_D})
add_definitions(-DIS_ANDROID_D=${IS_ANDROID_D})
add_definitions(-DIS_IOS_D=${IS_IOS_D})

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
      if (CMAKE_BUILD_TYPE MATCHES "Debug")
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
if (DEFINED LUMENXX_BUILD_TYPE)
  add_definitions(-DLUMENXX_BUILD_TYPE=${LUMENXX_BUILD_TYPE})
endif()

# deps
set(third_party_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")

include(ExternalProject)
include(FetchContent)

# boost
# https://github.com/boostorg/boost
# - 下载 release/boost-xxx.7z 解压到 third_party/boost/
#   cd boost/ && ./bootstrap.sh
#   ./b2 --prefix=${root}/third_party/boost-build
#   ./b2 install --prefix=${root}/third_party/boost-build
set(Boost_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/boost-build/")
add_definitions(-DBOOST_PROCESS_V2_STANDALONE=1)
add_definitions(-DBOOST_ASIO_STANDALONE=1)
add_definitions(-DASIO_STANDALONE=1)

# fmt
ExternalProject_Add(
  fmt_repo
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/"
  UPDATE_COMMAND ""
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
      -DFMT_PEDANTIC=OFF
      -DFMT_INSTALL=OFF
      -DFMT_WERROR=OFF
      -DBUILD_SHARED_LIBS=OFF
      -DFMT_DOC=OFF
      -DFMT_TEST=OFF
      -DFMT_CUDA_TEST=OFF
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(fmt_repo SOURCE_DIR)
set(fmt_SOURCE_DIR "${SOURCE_DIR}")
ExternalProject_Get_Property(fmt_repo BINARY_DIR)
set(fmt_INSTALL_DIR "${BINARY_DIR}/")

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
  FetchContent_MakeAvailable(uchardet)
endblock()

# simdjson
FetchContent_Declare(
  simdjson
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/simdjson/"
  EXCLUDE_FROM_ALL
)
block()
  set(BUILD_SHARED_LIBS             OFF)
  set(BUILD_STATIC_LIBS             ON)
  FetchContent_MakeAvailable(simdjson)
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
  set(NEOGRAPH_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_PYBIND    OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_A2A       OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_POSTGRES  OFF CACHE BOOL "" FORCE)
  set(NEOGRAPH_BUILD_SQLITE    OFF CACHE BOOL "" FORCE)
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

# glob
set(glob_SOURCE_DIR "${third_party_DIR}/glob/")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/)
include_directories(${simdjson_SOURCE_DIR}/include/)
include_directories(${fmt_SOURCE_DIR}/include/)
include_directories(${UCHARDET_INSTALL_DIR}/include/)
include_directories(${LIBICONV_INSTALL_DIR}/include/)
include_directories(${vectorscan_SOURCE_DIR}/src/)
include_directories(${neograph_SOURCE_DIR}/deps/)
include_directories(${neograph_SOURCE_DIR}/deps/asio/include/)
include_directories(${html2md_SOURCE_DIR}/include/)
include_directories(${glob_SOURCE_DIR}/single_include/)
include_directories(${Boost_ROOT}/include/)
include_directories(${third_party_DIR})

aux_source_directory("./lib/util" DIR_UTIL)
aux_source_directory("./lib/tools" DIR_TOOLS)
aux_source_directory("./lib/nodes" DIR_NODES)
aux_source_directory("./lib/agent" DIR_AGENT)

add_library(agentxx SHARED
  ${DIR_UTIL}
  ${DIR_TOOLS}
  ${DIR_NODES}
  ${DIR_AGENT}
)

add_dependencies(agentxx fmt_repo)
add_dependencies(agentxx iconv_repo)

target_link_directories(agentxx PUBLIC 
  ${simdjson_INSTALL_DIR}
  ${fmt_INSTALL_DIR}
  ${neograph_INSTALL_DIR}
  ${vectorscan_INSTALL_DIR}/lib/
  ${UCHARDET_INSTALL_DIR}/lib/
  ${LIBICONV_INSTALL_DIR}/lib/
  ${Boost_ROOT}/lib/
)

target_link_libraries(agentxx PUBLIC
  simdjson
  html2md
  uchardet
  iconv
  neograph::core
  neograph::util
  neograph::async
  neograph::llm
  neograph::mcp
  neograph::acp
	hs_runtime
	hs
)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  # 仅单配置构建、debug 时使用
  target_link_libraries(agentxx PUBLIC
    fmtd
  )
else ()
  target_link_libraries(agentxx PUBLIC
    fmt
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

if (DEFINED LUMENXX_BUILD_TYPE)
  ADD_SUBDIRECTORY(test)
  add_dependencies(test agentxx)

  ADD_SUBDIRECTORY(client)
  add_dependencies(client agentxx)
endif()

if (DEFINED LUMENXX_BUILD_TYPE)
  install(TARGETS agentxx DESTINATION "${EXE_INSTALL_PREFIX}")
elseif (STATIC_LINK_FFMPEG)
  install(TARGETS agentxx RUNTIME)
endif()
