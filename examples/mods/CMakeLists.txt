cmake_minimum_required(VERSION 3.24)

project(MNGLMods LANGUAGES CXX)

include(CTest)

get_filename_component(MNGL_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT MSVC)
	find_package(SDL3 CONFIG REQUIRED)
endif()

option(MNGL_BUILD_MOD_EXAMPLE "Build the native mod host and example module" ON)
include("${MNGL_SOURCE_ROOT}/cmake/MNGLMods.cmake")
mngl_add_mod_targets("${MNGL_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}")
