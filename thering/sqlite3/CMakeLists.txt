cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)

find_package(Threads)


# Sqlite shell
add_executable(sqlite3 shell.c)
target_link_libraries(sqlite3 libsqlite3 ${CMAKE_THREAD_LIBS_INIT})
if (UNIX)
	target_link_libraries(sqlite3 dl)
endif ()


# Sqlite library
add_library(libsqlite3 STATIC sqlite3.c)
set_target_properties(libsqlite3 PROPERTIES OUTPUT_NAME sqlite3)
add_definitions(-Wno-array-bounds)
