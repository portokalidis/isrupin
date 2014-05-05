cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)

option(USE_ISR "Enable ISR module" ON)
option(USE_REASSURE "Enable REASSURE module" ON)
option(USE_DTA "Enable DTA module" ON)
option(SAFECOPY_RESTORE "REASSURE uses safecopy to restore checkpoints" ON)
option(BLOCKINGRP "REASSURE supports blocking rescue points" ON)
option(DEBUG "Enable debugging" OFF)
option(MINESTRONE "Use minestrone-style reporting and watchdog" OFF)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

if (DEBUG)
	set (CMAKE_BUILD_TYPE Debug)
endif ()

add_definitions(-Wall -Wextra -Wno-unused-parameter -Wno-unused-result -Werror)

# MINESTRONE style
if (MINESTRONE)
	add_definitions(-DMINESTRONE)
	add_subdirectory(libminestrone)
endif ()


# DTA
if (USE_DTA)
	if (CMAKE_SIZEOF_VOID_P MATCHES 8)
		set(libdft_DIR ${CMAKE_SOURCE_DIR}/libdft_linux-amd64)
		add_subdirectory(libdft_linux-amd64)
	else ()
		set(libdft_DIR ${CMAKE_SOURCE_DIR}/libdft_linux-i386)
		add_subdirectory(libdft_linux-i386)
	endif ()
endif ()

# reassure
if (USE_REASSURE)
	set(libreassure_DIR ${CMAKE_SOURCE_DIR}/libreassure)
	add_subdirectory(libreassure)
endif ()

# isr
if (USE_ISR)
	set(libisr_DIR ${CMAKE_SOURCE_DIR}/libisr)
	add_subdirectory(libisr)
	add_subdirectory(sqlite)
	add_subdirectory(exec_wrapper)
	if (CMAKE_SIZEOF_VOID_P MATCHES 8)
		option(ISOLATION "Isolate memory with ISR" OFF)
	else ()
		option(ISOLATION "Isolate memory with ISR" ON)
	endif ()
endif ()

add_subdirectory(libcrossdev)
add_subdirectory(tools)
add_subdirectory(other_tools)
