cmake_minimum_required (VERSION 2.6)
project (libminestrone)
set(CMAKE_VERBOSE_MAKEFILE on)

# PIN
set(Pin_DIR ..)
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})

# libminestrone specific
set(SOURCES notify.cpp watchdog.cpp)

add_library(minestrone STATIC ${SOURCES})
include_directories(${PIN_INCLUDES})
