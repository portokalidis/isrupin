# CMake stuff
cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)

# PIN
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})
include_directories(${PIN_INCLUDES})

# libdft specific
set(SOURCES libdft_api.c libdft_core.c syscall_desc.c tagmap.c)
set_source_files_properties(${SOURCES} PROPERTIES LANGUAGE CXX)
include_directories(../include)
add_library(dft STATIC ${SOURCES})
