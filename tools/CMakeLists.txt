cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)

set(SOURCES isrtool.cpp)

# libisr
find_package(libisr)
find_package(libsqlite3)
find_package(Threads)
add_definitions(-DUSE_ISR ${LIBISR_DEFINITIONS})
include_directories(${LIBISR_INCLUDE_DIRS})
link_directories(${LIBISR_LIBRARY_DIRS})


# PIN
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
# We have all the sources here!
add_library(isrtool SHARED ${SOURCES})
set_target_properties(isrtool PROPERTIES PREFIX "") # drop 'lib' prefix
target_link_libraries(isrtool ${LIBISR_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
					${LIBSQLITE3_LIBRARIES})
#target_link_libraries(thering ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(isrtool ${LIBCROSSDEV_LIBRARIES})
target_link_libraries(isrtool ${PIN_LDFLAGS} ${PIN_LIBS})


# wrapper
add_definitions(-DISR_WRAPPER_NAME="isr_wrapper")
add_definitions(-DISR_NAME="thering")
add_library(isr_wrapper SHARED isr_wrapper.cpp cmdln.cpp)
set_target_properties(isr_wrapper PROPERTIES PREFIX "")
target_link_libraries(isr_wrapper ${PIN_LDFLAGS} ${PIN_LIBS})

