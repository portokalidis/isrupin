cmake_minimum_required (VERSION 2.6)
project (tools)
set(CMAKE_VERBOSE_MAKEFILE on)

set(SOURCES thering.cpp)

# libisr
if (USE_ISR)
	find_package(libisr)
	add_definitions(-DUSE_ISR ${LIBISR_DEFINITIONS})
	include_directories(${LIBISR_INCLUDE_DIRS})
	link_directories(${LIBISR_LIBRARY_DIRS})
	find_package(Threads)
endif ()

# libreassure
if (USE_REASSURE)
	find_package(libreassure)
	add_definitions(-DUSE_REASSURE ${LIBREASSURE_DEFINITIONS})
	include_directories(${LIBREASSURE_INCLUDE_DIRS})
	link_directories(${LIBREASSURE_LIBRARY_DIRS})
	set(NEEDS_LIBXDEV 1)
endif ()

# libdta
if (USE_DTA)
	find_package(libdft)
	add_definitions(-DUSE_DTA ${LIBDFT_DEFINITIONS})
	include_directories(${LIBDFT_INCLUDE_DIRS})
	link_directories(${LIBDFT_LIBRARY_DIRS})
	set(SOURCES ${SOURCES} dta_tool.cpp)
endif ()

# libcrossdev
if (NEEDS_LIBXDEV)
	set(libcrossdev_DIR ../libcrossdev)
	find_package(libcrossdev)
	include_directories(${LIBCROSSDEV_INCLUDE_DIRS})
	link_directories(${LIBCROSSDEV_LIBRARY_DIRS})
endif ()

# PIN
set(Pin_DIR ..)
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})
include_directories(${PIN_INCLUDES})
link_directories(${PIN_LPATHS})


#
# tool specific
#
include_directories(../include)


# The name of the main pin tool should be no more characters than
# the wrapper tool to easily replace it in the command line
add_library(thering SHARED ${SOURCES})
set_target_properties(thering PROPERTIES PREFIX "") # drop 'lib' prefix
target_link_libraries(thering ${LIBISR_LIBRARIES})
target_link_libraries(thering ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(thering ${LIBREASSURE_LIBRARIES})
target_link_libraries(thering ${LIBDFT_LIBRARIES})
target_link_libraries(thering ${LIBCROSSDEV_LIBRARIES})
target_link_libraries(thering ${PIN_LDFLAGS} ${PIN_LIBS})


# wrapper
if (USE_ISR)
	add_definitions(-DISR_WRAPPER_NAME="isr_wrapper")
	add_definitions(-DISR_NAME="thering")
	add_library(isr_wrapper SHARED isr_wrapper.cpp cmdln.cpp)
	set_target_properties(isr_wrapper PROPERTIES PREFIX "")
	target_link_libraries(isr_wrapper ${PIN_LDFLAGS} ${PIN_LIBS})
endif ()

