cmake_minimum_required(VERSION 3.24)

project(MNGLVcpkgConsumer LANGUAGES CXX)

find_package(MNGL CONFIG REQUIRED)

add_executable(mngl_vcpkg_consumer main.cpp)
target_link_libraries(mngl_vcpkg_consumer PRIVATE MNGL::MNGL)

enable_testing()
add_test(NAME mngl_vcpkg_consumer COMMAND mngl_vcpkg_consumer)
set_tests_properties(mngl_vcpkg_consumer PROPERTIES
    TIMEOUT 20
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
