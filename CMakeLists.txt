cmake_minimum_required(VERSION 2.8)

project(video_recorder CXX C)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 17)

if(UNIX)
	option(BUILD_SHARED_LIBS "Build Shared Libraries" OFF)
endif()

set(vr_include_dirs ${PROJECT_SOURCE_DIR})
set(vr_lib_deps)

include(cmake/dependencies.cmake)
include(cmake/utils.cmake)

if(MSVC)
    msvc_multi_threaded_static_turn(ON)
endif()

set(LIB_TYPE STATIC)
if(BUILD_SHARED_LIBS)
    set(LIB_TYPE SHARED)
endif()

add_subdirectory(vr)
add_subdirectory(examples)