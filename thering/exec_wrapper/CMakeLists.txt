cmake_minimum_required (VERSION 2.6)
project (tools)
set(CMAKE_VERBOSE_MAKEFILE on)

# execution wrapper
add_executable(isrexecwrapper exec_wrapper.c)
