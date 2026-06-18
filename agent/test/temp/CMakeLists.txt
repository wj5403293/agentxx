# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0097 NEW)

project(test_temp VERSION 0.1.0 LANGUAGES C CXX)

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
set(third_party_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/")

include(ExternalProject)
include(FetchContent)

set(Boost_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/boost-build/")
add_definitions(-DBOOST_ASIO_STANDALONE=1)
add_definitions(-DASIO_STANDALONE=1)
# add_definitions(-DBOOST_PROCESS_V2_STANDALONE=1)
add_definitions(-DBOOST_PROCESS_USE_STD_FS=1)

include_directories(${Boost_ROOT}/include/)
include_directories(${Boost_ROOT}/include/boost/)
include_directories(${third_party_DIR})

if (IS_DEBUG_D)
    add_compile_options("-fsanitize=address")
    add_link_options("-fsanitize=address")
endif ()

file(GLOB_RECURSE test_temp_SOURCES "*.cpp" "*.cc" "*.cxx")
add_executable(test_temp)
target_sources(test_temp PRIVATE 
  ${test_temp_SOURCES}
)
target_link_directories(test_temp PUBLIC 
	${Boost_ROOT}/lib/
)
target_link_libraries(test_temp
	boost_process
)

if (IS_MACOS_D)
  target_link_options(test_temp PRIVATE 
    "-Wl,-rpath,@executable_path"
  )
endif()
if (IS_WIN_D)
	target_compile_definitions(
		${NAME_test} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

install(TARGETS test_temp DESTINATION "${EXE_INSTALL_PREFIX}")