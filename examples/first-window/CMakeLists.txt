cmake_minimum_required(VERSION 3.24)
project(mngl_first_window LANGUAGES CXX)

find_package(MNGL CONFIG REQUIRED)

add_executable(mngl_first_window main.cpp)
target_compile_features(mngl_first_window PRIVATE cxx_std_23)
target_link_libraries(mngl_first_window PRIVATE MNGL::MNGL)
