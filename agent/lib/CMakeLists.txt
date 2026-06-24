CMAKE_MINIMUM_REQUIRED(VERSION 3.30)

project(agentxx VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(agentxx SHARED)

file(GLOB_RECURSE agentxx_SOURCES
  CONFIGURE_DEPENDS
 "src/*.cpp" "src/*.cc" "src/*.cxx"
)

target_sources(agentxx PRIVATE
  ${agentxx_SOURCES}
)

# 统一输出名称 libagentxx
# - 不指定时，部分系统，如win可能编译输出缺少前缀 lib
set_target_properties(agentxx PROPERTIES
  PUBLIC_HEADER "include/agentxx/agentxx.h"
  PREFIX "lib"
  OUTPUT_NAME "agentxx"
  CXX_STANDARD 23
  LINK_LIBRARIES_STRATEGY "REORDER_MINIMALLY"
)

if (IS_WIN_D)
	target_compile_definitions(
		agentxx PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()
target_compile_definitions(agentxx PUBLIC __STDC_CONSTANT_MACROS)
target_compile_definitions(agentxx PUBLIC IS_DEBUG_D=${IS_DEBUG_D})
target_compile_definitions(agentxx PUBLIC IS_RELEASE_D=${IS_RELEASE_D})
target_compile_definitions(agentxx PUBLIC XX_BUILD_TYPE=${XX_BUILD_TYPE})

target_compile_definitions(agentxx PUBLIC IS_LINUX_D=${IS_LINUX_D})
target_compile_definitions(agentxx PUBLIC IS_WIN_D=${IS_WIN_D})
target_compile_definitions(agentxx PUBLIC IS_MACOS_D=${IS_MACOS_D})
target_compile_definitions(agentxx PUBLIC IS_ANDROID_D=${IS_ANDROID_D})
target_compile_definitions(agentxx PUBLIC IS_IOS_D=${IS_IOS_D})
target_compile_definitions(agentxx PUBLIC NEOGRAPH_USE_BOODT_ASIO)
if (AGENTXX_ENABLE_HYPERSCAN)
  target_compile_definitions(agentxx PUBLIC AGENTXX_ENABLE_HYPERSCAN=1)
endif()
if (AGENTXX_ENABLE_CODEGRAPH)
  target_compile_definitions(agentxx PUBLIC AGENTXX_ENABLE_CODEGRAPH=1)
endif()

if (IS_DEBUG_D)
  target_compile_options(agentxx PRIVATE "-fsanitize=address")
  target_link_options(agentxx PRIVATE "-fsanitize=address")
endif ()

find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS process)
if(Boost_FOUND)
  target_include_directories(agentxx SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}/)
  target_include_directories(agentxx SYSTEM PUBLIC ${Boost_INCLUDE_DIRS}/boost/)
  target_link_directories(agentxx PUBLIC 
    ${BOOST_LIBRARY_DIR}
  )
endif()
find_package(iconv REQUIRED)
find_package(uchardet REQUIRED)
find_package(sqlite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(html2md REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Glob REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

target_include_directories(agentxx PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# 标记为 SYSTEM 防止 include 影响 warning 转 error 判断
target_include_directories(agentxx PUBLIC ${CMAKE_PREFIX_PATH}/include/)
target_include_directories(agentxx PUBLIC ${neograph_SOURCE_DIR}/deps/yyjson/)
target_include_directories(agentxx PUBLIC ${neograph_SOURCE_DIR}/deps/)

target_link_directories(agentxx PUBLIC
  ${CMAKE_PREFIX_PATH}/lib/
)

target_link_libraries(agentxx PUBLIC
  Iconv::Iconv
  uchardet::libuchardet
  # === neograph vvv 链接顺序是必要的
  neograph_sqlite
  neograph_acp
  neograph_mcp
  neograph_async
  neograph_llm
  neograph_core
  yyjson
  # === neograph ^^^
  SQLite3::SQLite3
  CURL::libcurl
  OpenSSL::SSL
  OpenSSL::Crypto
  fmt::fmt
  html2md
  yaml-cpp::yaml-cpp
  Glob
  Boost::process
)
if (AGENTXX_LINUX_IO_URING_SUPPORTED)
  pkg_check_modules(uring REQUIRED IMPORTED_TARGET liburing)
  target_link_libraries(agentxx PRIVATE 
    PkgConfig::uring
  )
endif()
if (AGENTXX_ENABLE_HYPERSCAN)
  pkg_check_modules(hyperscan REQUIRED IMPORTED_TARGET libhs)
  target_link_libraries(agentxx PRIVATE 
    PkgConfig::hyperscan
    hs_runtime
  )
endif()
if (AGENTXX_ENABLE_CODEGRAPH)
  target_link_libraries(agentxx PUBLIC 
    codegraph_core
  )
endif()

install(TARGETS agentxx DESTINATION "${AGENTXX_EXEC_INSTALL_PREFIX}")

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CPM.cmake")
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.6.0")
string(TOLOWER agentxx/version.h VERSION_HEADER_LOCATION)
packageProject(
  NAME ${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include/agentxx
  INCLUDE_DESTINATION include/agentxx
  VERSION_HEADER "${VERSION_HEADER_LOCATION}"
  DEPENDENCIES "Boost;Iconv;uchardet;sqlite3;fmt;html2md;yaml-cpp;Glob"
)
