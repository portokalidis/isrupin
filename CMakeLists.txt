cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)
project(isrupin)

option(DEBUG "Enable debugging" OFF)

# Build type
if (DEBUG)
	set(CMAKE_BUILD_TYPE Debug)
	add_definitions(-DDEBUG)
else ()
	set(CMAKE_BUILD_TYPE Release)
endif ()

set(Pin_DIR ${CMAKE_SOURCE_DIR})
set(libcrossdev_DIR ${CMAKE_SOURCE_DIR}/libcrossdev)
set(libsqlite3_DIR ${CMAKE_SOURCE_DIR}/sqlite3)

add_definitions(-Wall -Werror)

# isr
set(libisr_DIR ${CMAKE_SOURCE_DIR}/libisr)
add_subdirectory(libisr)
add_subdirectory(sqlite3)
add_subdirectory(exec_wrapper)
if (CMAKE_SIZEOF_VOID_P MATCHES 8)
	option(ISOLATION "Isolate memory with ISR" OFF)
else ()
	option(ISOLATION "Isolate memory with ISR" ON)
endif ()

configure_file(setup.sh setup.sh COPYONLY)
configure_file(isr.conf isr.conf COPYONLY)
configure_file(image_keys.sql image_keys.sql COPYONLY)
file(COPY scripts DESTINATION .)
file(COPY bin DESTINATION .)

add_subdirectory(libcrossdev)
add_subdirectory(tools)
add_subdirectory(other_tools)
