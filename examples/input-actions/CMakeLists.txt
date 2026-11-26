cmake_minimum_required(VERSION 3.24)
project(mngl_input_actions LANGUAGES CXX)

find_package(MNGL CONFIG REQUIRED)

add_executable(mngl_input_actions main.cpp)
target_compile_features(mngl_input_actions PRIVATE cxx_std_23)
target_link_libraries(mngl_input_actions PRIVATE MNGL::MNGL)
