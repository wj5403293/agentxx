CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

aux_source_directory(. DIR_test)
set(NAME_test test)

add_executable(${NAME_test})

file(GLOB test_SOURCES "*.cpp" "*.cc" "*.cxx")
target_sources(${NAME_test} PRIVATE
	${test_SOURCES}
)
target_include_directories(${NAME_test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

if (IS_DEBUG_D)
    target_compile_options(${NAME_test} PRIVATE "-fsanitize=address")
    target_link_options(${NAME_test} PRIVATE "-fsanitize=address")
endif ()

target_link_libraries(${NAME_test}
	agentxx
)
if (IS_MACOS_D)
  target_link_options(${NAME_test} PRIVATE 
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

add_dependencies(${NAME_test} agentxx)

install(TARGETS ${NAME_test} DESTINATION "${EXE_INSTALL_PREFIX}")