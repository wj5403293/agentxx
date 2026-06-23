CMAKE_MINIMUM_REQUIRED(VERSION 3.28)

project(agentxx_test VERSION 0.1.0 LANGUAGES C CXX)

add_executable(agentxx_test)

file(GLOB test_SOURCES "*.cpp" "*.cc" "*.cxx")
target_sources(agentxx_test PRIVATE
	${test_SOURCES}
)
target_include_directories(agentxx_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (IS_DEBUG_D)
    target_compile_options(agentxx_test PRIVATE "-fsanitize=address")
    target_link_options(agentxx_test PRIVATE "-fsanitize=address")
endif ()

find_package(libagentxx REQUIRED)
target_link_libraries(agentxx_test
	agentxx
)
if (IS_MACOS_D)
  target_link_options(agentxx_test PRIVATE 
    "-Wl,-rpath,@executable_path"
  )
endif()

if (IS_WIN_D)
	target_compile_definitions(
		agentxx_test PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

install(TARGETS agentxx_test DESTINATION "${AGENTXX_EXEC_INSTALL_PREFIX}")