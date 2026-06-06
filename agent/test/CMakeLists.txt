CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

aux_source_directory(. DIR_test)
set(NAME_test test)

if (IS_DEBUG_D)
    add_compile_options("-fsanitize=address")
    add_link_options("-fsanitize=address")
endif ()

add_executable(${NAME_test} ${DIR_test})
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

install(TARGETS ${NAME_test} DESTINATION "${EXE_INSTALL_PREFIX}")