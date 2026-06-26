CMAKE_MINIMUM_REQUIRED(VERSION 3.30)

project(agentxx_test VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (XX_IS_MSVC)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib;${CMAKE_FIND_LIBRARY_SUFFIXES}")
else ()
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;${CMAKE_FIND_LIBRARY_SUFFIXES}")
endif()

add_executable(agentxx_test)

file(GLOB test_SOURCES 
	CONFIGURE_DEPENDS
	"*.cpp" "*.cc" "*.cxx"
)
target_sources(agentxx_test PRIVATE
	${test_SOURCES}
)

set_target_properties(agentxx_test PROPERTIES
  VS_GLOBAL_VcpkgEnabled false
)

target_include_directories(agentxx_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)

find_package(iconv REQUIRED)
find_package(uchardet REQUIRED)
find_package(fmt REQUIRED)
find_package(simdjson REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS process)
find_package(yaml-cpp REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(html2md REQUIRED)
find_package(Glob REQUIRED)

find_package(agentxx_static REQUIRED)

target_link_directories(agentxx_test PUBLIC
  ${CMAKE_PREFIX_PATH}/lib/
)
if(Boost_FOUND)
  target_include_directories(agentxx_test SYSTEM PRIVATE ${Boost_INCLUDE_DIRS}/)
  target_include_directories(agentxx_test SYSTEM PRIVATE ${Boost_INCLUDE_DIRS}/boost/)
  target_link_directories(agentxx_test PRIVATE 
    ${BOOST_LIBRARY_DIR}
  )
endif()

target_link_libraries(agentxx_test PRIVATE
	agentxx_static
)
if (AGENTXX_ENABLE_HYPERSCAN OR AGENTXX_ENABLE_VECTORSCAN)
  pkg_check_modules(hyperscan REQUIRED IMPORTED_TARGET libhs)
  target_link_libraries(agentxx_test PRIVATE 
    PkgConfig::hyperscan
    hs_runtime
  )
endif()
if (AGENTXX_LINUX_IO_URING_SUPPORTED)
  pkg_check_modules(uring REQUIRED IMPORTED_TARGET liburing)
  target_link_libraries(agentxx_test PRIVATE 
    PkgConfig::uring
  )
endif()

if (XX_IS_MSVC)
	target_compile_options(agentxx_test PRIVATE 
		"/bigobj"
	)
	target_compile_definitions(agentxx_test PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()
if (XX_IS_MACOS_D)
  target_link_options(agentxx_test PRIVATE 
    "-Wl,-rpath,@executable_path"
  )
endif()

install(TARGETS agentxx_test DESTINATION "${AGENTXX_EXEC_INSTALL_PREFIX}")