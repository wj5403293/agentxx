CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

set(NAME_agentxx agentxx)

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

target_include_directories(agentxx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/)
# 标记为 SYSTEM 防止 include 影响 warning 转 error 判断
target_include_directories(agentxx SYSTEM PUBLIC ${fmt_SOURCE_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${uchardet_INSTALL_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${LIBICONV_INSTALL_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${LIBURING_INSTALL_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${SQLITE_SOURCE_DIR}/src/)
target_include_directories(agentxx SYSTEM PUBLIC ${vectorscan_SOURCE_DIR}/src/)
target_include_directories(agentxx SYSTEM PUBLIC ${codegraph_SOURCE_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${neograph_SOURCE_DIR}/deps/)
target_include_directories(agentxx SYSTEM PUBLIC ${html2md_SOURCE_DIR}/include/)
target_include_directories(agentxx SYSTEM PUBLIC ${glob_SOURCE_DIR}/single_include/)
target_include_directories(agentxx SYSTEM PUBLIC ${Boost_ROOT}/include/boost/)

if (IS_DEBUG_D)
    target_compile_options(agentxx PRIVATE "-fsanitize=address")
    target_link_options(agentxx PRIVATE "-fsanitize=address")
endif ()


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
  boost_process
)
if (LINUX_IO_URING_SUPPORTED)
  target_link_libraries(agentxx PUBLIC
    liburing.a 
    liburing-ffi.a
  )
endif()

install(TARGETS agentxx DESTINATION "${EXE_INSTALL_PREFIX}")