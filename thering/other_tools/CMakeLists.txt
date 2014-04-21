cmake_minimum_required (VERSION 2.6)
project (tools)
set(CMAKE_VERBOSE_MAKEFILE on)


# Modules to use
# PIN
set(Pin_DIR ..)
find_package(Pin)
add_definitions(${PIN_CXXFLAGS})
include_directories(${PIN_INCLUDES})
link_directories(${PIN_LPATHS})


#
# tool specific
#

# null tool
add_library(null SHARED null.cpp)
set_target_properties(null PROPERTIES PREFIX "")
target_link_libraries(null ${PIN_LDFLAGS} ${PIN_LIBS})

# cprofile
add_library(cprofile SHARED cprofile.cpp)
set_target_properties(cprofile PROPERTIES PREFIX "")
target_link_libraries(cprofile ${PIN_LDFLAGS} ${PIN_LIBS})

# regs tool
add_library(regs SHARED regs.cpp)
set_target_properties(regs PROPERTIES PREFIX "")
target_link_libraries(regs ${PIN_LDFLAGS} ${PIN_LIBS})
