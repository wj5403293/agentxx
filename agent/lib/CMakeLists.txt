CMAKE_MINIMUM_REQUIRED(VERSION 3.28)

project(agentxx VERSION 0.1.0 LANGUAGES C CXX)

add_library(agentxx SHARED)

file(GLOB_RECURSE agentxx_SOURCES "*.cpp" "*.cc" "*.cxx")

target_sources(agentxx PRIVATE
  ${agentxx_SOURCES}
)

# 统一输出名称 libagentxx
# - 不指定时，部分系统，如win可能编译输出缺少前缀 lib
set_target_properties(agentxx PROPERTIES
  PUBLIC_HEADER agentxx.h
  PREFIX "lib"
  OUTPUT_NAME "agentxx"
  CXX_STANDARD 23
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
if (AGENTXX_ENABLE_HYPERSCAN)
  target_compile_definitions(agentxx PUBLIC AGENTXX_ENABLE_HYPERSCAN=1)
endif()

target_include_directories(agentxx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# 标记为 SYSTEM 防止 include 影响 warning 转 error 判断
target_include_directories(agentxx SYSTEM PUBLIC ${fmt_SOURCE_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${uchardet_INSTALL_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${LIBICONV_INSTALL_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${LIBURING_INSTALL_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${SQLITE3_INSTALL_DIR}/src/)
target_include_directories(agentxx SYSTEM PUBLIC ${vectorscan_SOURCE_DIR}/src/)
target_include_directories(agentxx SYSTEM PUBLIC ${html2md_SOURCE_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${glob_SOURCE_DIR}/single_include/)
target_include_directories(agentxx SYSTEM PUBLIC ${CODEGRAPH_SOURCE_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${NEOGRAPH_SOURCE_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${BOOST_INCLUDE_DIR}/)
target_include_directories(agentxx SYSTEM PUBLIC ${BOOST_INCLUDE_DIR}/boost/)
target_include_directories(agentxx SYSTEM PUBLIC ${NEOGRAPH_SOURCE_DIR}/deps/)

if (IS_DEBUG_D)
    target_compile_options(agentxx PRIVATE "-fsanitize=address")
    target_link_options(agentxx PRIVATE "-fsanitize=address")
endif ()

find_package(Boost REQUIRED)
find_package(iconv REQUIRED)
find_package(fmt REQUIRED)
find_package(uchardet REQUIRED)
find_package(codegraph REQUIRED)
find_package(neograph REQUIRED)
find_package(html2md REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(glob REQUIRED)

target_link_directories(agentxx PUBLIC 
  ${BOOST_LIBRARY_DIR}
)

target_link_libraries(agentxx PUBLIC
  Iconv::Iconv
  fmt::fmt
  html2md
  uchardet::libuchardet
  sqlite3
  neograph::core
  neograph::util
  neograph::async
  neograph::llm
  neograph::mcp
  neograph::acp
  yaml-cpp::yaml-cpp
  boost::process
)
if (AGENTXX_ENABLE_HYPERSCAN)
  find_package(vectorscan REQUIRED)
  target_link_libraries(agentxx PUBLIC 
    vectorscan
  )
endif()
if (AGENTXX_ENABLE_CODEGRAPH)
  find_package(codegraph REQUIRED)
  target_link_libraries(agentxx PUBLIC 
    codegraph-cpp::codegraph_core
  )
endif()

install(TARGETS agentxx DESTINATION "${AGENTXX_EXEC_INSTALL_PREFIX}")

include(cmake/CPM.cmake)
CPMAddPackage(
        NAME PackageProject.cmake
        GITHUB_REPOSITORY TheLartians/PackageProject.cmake
        VERSION 1.3
)
string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)
packageProject(
        NAME ${PROJECT_NAME}
        VERSION ${PROJECT_VERSION}
        BINARY_DIR ${PROJECT_BINARY_DIR}
        INCLUDE_DIR ${PROJECT_SOURCE_DIR}/lib
        INCLUDE_DESTINATION include/${PROJECT_NAME}
        VERSION_HEADER "${VERSION_HEADER_LOCATION}"
        DEPENDENCIES ""
)
