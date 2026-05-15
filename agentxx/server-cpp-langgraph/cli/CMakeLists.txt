CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

aux_source_directory(. DIR_client)
set(NAME_client client)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options("-fsanitize=address")
    add_link_options("-fsanitize=address")
endif (CMAKE_BUILD_TYPE MATCHES Debug)

add_executable(${NAME_client} ${DIR_client})
target_link_libraries(${NAME_client}
	agentxx
)
if (IS_MACOS_D)
  target_link_options(${NAME_client} PRIVATE 
    "-Wl,-rpath,@executable_path"
  )
endif()
if (IS_WIN_D)
	target_compile_definitions(
		${NAME_client} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

if (DEFINED LUMENXX_BUILD_TYPE)
	install(TARGETS ${NAME_test} DESTINATION "${EXE_INSTALL_PREFIX}")
endif()