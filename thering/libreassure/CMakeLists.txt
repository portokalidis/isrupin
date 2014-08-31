# CMake stuff
cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)

if (SAFECOPY_RESTORE)
	add_definitions(-DSAFECOPY_RESTORE)
endif ()

if (BLOCKINGRP)
	add_definitions(-DBLOCKINGRP)
endif ()

# PIN
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})
include_directories(${PIN_INCLUDES})

# libcrossdev
find_package(libcrossdev)
include_directories(${LIBCROSSDEV_INCLUDE_DIRS})


# libreassure specific
set(SOURCES	libreassure.cpp RescuePoint.cpp threadstate.cpp utils.cpp 
		writeslog.cpp fork.cpp filter.cpp)
include_directories(../include)
add_library(reassure STATIC ${SOURCES})

# configuration test tool - OBSOLETE
#add_library(conftest SHARED conftest.cpp utils.cpp)
#set_target_properties(conftest PROPERTIES PREFIX "")
#target_link_libraries(conftest ${PIN_LDFLAGS} ${PIN_LIBS})

