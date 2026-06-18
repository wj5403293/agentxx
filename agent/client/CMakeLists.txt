CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

aux_source_directory(. DIR_client)
set(NAME_client client)

add_executable(${NAME_client} ${DIR_client})

if (IS_DEBUG_D)
    target_compile_options(${NAME_client} PRIVATE "-fsanitize=address")
    target_link_options(${NAME_client} PRIVATE "-fsanitize=address")
endif ()

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

install(TARGETS ${NAME_client} DESTINATION "${EXE_INSTALL_PREFIX}")