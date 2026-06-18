CMAKE_MINIMUM_REQUIRED(VERSION 3.6)

set(NAME_client client)

add_executable(${NAME_client})

file(GLOB_RECURSE client_SOURCES "*.cpp" "*.cc" "*.cxx")
target_sources(${NAME_client} PRIVATE
	${client_SOURCES}
)

target_include_directories(${NAME_client} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

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