include(CheckFunctionExists)

check_function_exists(clock_gettime HAS_GETTIME)
if (NOT HAS_GETTIME)
	set(LIBRT rt)
endif ()

get_filename_component(LIBMINESTRONE_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(LIBMINESTRONE_DEFINITIONS -DMINESTRONE)
set(LIBMINESTRONE_LIBRARIES minestrone ${LIBRT})
set(LIBMINESTRONE_INCLUDE_DIR ${LIBMINESTRONE_CMAKE_DIR})
set(LIBMINESTRONE_INCLUDE_DIRS ${LIBMINESTRONE_INCLUDE_DIR})
set(LIBMINESTRONE_LIBRARY_DIRS ${LIBMINESTRONE_INCLUDE_DIR})
