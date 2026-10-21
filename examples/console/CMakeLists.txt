cmake_minimum_required(VERSION 3.24)

project(MNGLConsoleExample LANGUAGES CXX)

get_filename_component(MNGL_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_executable(mngl_console_example main.cpp)
target_compile_features(mngl_console_example PRIVATE cxx_std_23)
target_include_directories(mngl_console_example PRIVATE "${MNGL_SOURCE_ROOT}")
