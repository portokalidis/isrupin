cmake_minimum_required (VERSION 2.6)
project (tools)
set(CMAKE_VERBOSE_MAKEFILE on)

find_package(Threads)

add_executable(sqlite3 sqlite3.c shell.c)
target_link_libraries(sqlite3 ${CMAKE_THREAD_LIBS_INIT})

if (UNIX)
	target_link_libraries(sqlite3 dl)
endif ()
