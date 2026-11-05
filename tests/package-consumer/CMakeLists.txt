cmake_minimum_required(VERSION 3.24)

project(MNGLPackageConsumer LANGUAGES CXX)

find_package(MNGL CONFIG REQUIRED)

add_executable(mngl_package_consumer "${CMAKE_CURRENT_LIST_DIR}/../audio_runtime_smoke.cpp")
target_link_libraries(mngl_package_consumer PRIVATE MNGL::MNGL)

enable_testing()
add_test(NAME mngl_package_consumer COMMAND mngl_package_consumer)
set_tests_properties(mngl_package_consumer PROPERTIES
	TIMEOUT 20
	ENVIRONMENT "SDL_AUDIODRIVER=dummy")
