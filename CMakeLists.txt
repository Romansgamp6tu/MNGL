cmake_minimum_required(VERSION 3.24)

project(MNGL LANGUAGES CXX)

add_executable(mngl_demo
	MNGL/MNGL.cpp
	MNGL/imgui/imgui.cpp
	MNGL/imgui/imgui_draw.cpp
	MNGL/imgui/imgui_tables.cpp
	MNGL/imgui/imgui_widgets.cpp
	MNGL/imgui/backends/imgui_impl_sdl3.cpp
	MNGL/imgui/backends/imgui_impl_sdlgpu3.cpp
)

target_compile_features(mngl_demo PRIVATE cxx_std_23)
find_package(OpenMP QUIET COMPONENTS CXX)
if(OpenMP_CXX_FOUND)
	target_link_libraries(mngl_demo PRIVATE OpenMP::OpenMP_CXX)
endif()

target_include_directories(mngl_demo PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
	"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui"
)

if(MSVC)
	# The practicum's template-heavy translation unit can exceed COFF's section limit.
	target_compile_options(mngl_demo PRIVATE /bigobj)
	# utilities.hpp uses MSVC linker pragmas for these bundled libraries.
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(MNGL_LIBRARY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs")
	else()
		set(MNGL_LIBRARY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs/x86")
	endif()
	target_link_directories(mngl_demo PRIVATE "${MNGL_LIBRARY_DIRECTORY}")

	if(WIN32)
		add_custom_command(TARGET mngl_demo POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"$<TARGET_FILE_DIR:mngl_demo>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
				"$<TARGET_FILE_DIR:mngl_demo>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_demo>"
			VERBATIM
		)
	endif()
else()
	find_package(SDL3 CONFIG REQUIRED)
	find_package(SDL3_image CONFIG REQUIRED)
	find_package(Freetype REQUIRED)

	target_link_libraries(mngl_demo PRIVATE
		SDL3::SDL3
		SDL3_image::SDL3_image
		Freetype::Freetype
	)
endif()

add_custom_command(TARGET mngl_demo POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders"
		"$<TARGET_FILE_DIR:mngl_demo>/shaders"
	COMMAND "${CMAKE_COMMAND}" -E make_directory
		"$<TARGET_FILE_DIR:mngl_demo>/imgui/misc/fonts"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/misc/fonts/Roboto-Medium.ttf"
		"$<TARGET_FILE_DIR:mngl_demo>/imgui/misc/fonts/Roboto-Medium.ttf"
	VERBATIM
)

option(MNGL_BUILD_TESTS "Build tree coordinate regression checks" OFF)
if(MNGL_BUILD_TESTS)
	enable_testing()
	add_executable(mngl_tree_precision tests/tree_precision.cpp)
	target_compile_features(mngl_tree_precision PRIVATE cxx_std_23)
	target_include_directories(mngl_tree_precision PRIVATE include MNGL)
	target_compile_definitions(mngl_tree_precision PRIVATE
		MNGL_TEST_FONT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/misc/fonts/Roboto-Medium.ttf")
	if(MSVC)
		target_link_directories(mngl_tree_precision PRIVATE "${MNGL_LIBRARY_DIRECTORY}")
		target_compile_options(mngl_tree_precision PRIVATE /utf-8)
		foreach(dependency SDL3 SDL3_image freetype)
			add_custom_command(TARGET mngl_tree_precision POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"${MNGL_LIBRARY_DIRECTORY}/${dependency}.dll"
					"$<TARGET_FILE_DIR:mngl_tree_precision>" VERBATIM)
		endforeach()
	else()
		target_link_libraries(mngl_tree_precision PRIVATE
			SDL3::SDL3 SDL3_image::SDL3_image Freetype::Freetype)
	endif()
	if(OpenMP_CXX_FOUND)
		target_link_libraries(mngl_tree_precision PRIVATE OpenMP::OpenMP_CXX)
	endif()
	add_custom_command(TARGET mngl_tree_precision POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders"
			"$<TARGET_FILE_DIR:mngl_tree_precision>/shaders" VERBATIM)
	# The default check needs no GPU. Pass --render <directory> explicitly for readback comparisons.
	add_test(NAME tree_precision COMMAND mngl_tree_precision)
endif()

option(MNGL_BUILD_DEVEL_PACKAGE "Define install rules and package targets for the MNGL development package" OFF)
if(MNGL_BUILD_DEVEL_PACKAGE)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR "MNGL_BUILD_DEVEL_PACKAGE currently supports Linux only")
	endif()

	include(GNUInstallDirs)
	include(CMakePackageConfigHelpers)
	# CPack invokes the preinstall target; do not make it compile the demo executable.
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)

	find_package(SDL3 CONFIG REQUIRED)
	find_package(SDL3_image CONFIG REQUIRED)
	find_package(Freetype REQUIRED)
	find_package(glm CONFIG REQUIRED)

	add_library(mngl INTERFACE)
	add_library(MNGL::MNGL ALIAS mngl)
	set_target_properties(mngl PROPERTIES EXPORT_NAME MNGL)
	target_compile_features(mngl INTERFACE cxx_std_23)
	target_include_directories(mngl INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mngl>
	)
	target_link_libraries(mngl INTERFACE
		SDL3::SDL3
		SDL3_image::SDL3_image
		Freetype::Freetype
		glm::glm
	)

	add_library(mngl_imgui STATIC
		MNGL/imgui/imgui.cpp
		MNGL/imgui/imgui_draw.cpp
		MNGL/imgui/imgui_tables.cpp
		MNGL/imgui/imgui_widgets.cpp
		MNGL/imgui/backends/imgui_impl_sdl3.cpp
		MNGL/imgui/backends/imgui_impl_sdlgpu3.cpp
	)
	add_library(MNGL::ImGui ALIAS mngl_imgui)
	set_target_properties(mngl_imgui PROPERTIES
		EXPORT_NAME ImGui
		POSITION_INDEPENDENT_CODE ON
	)
	target_compile_features(mngl_imgui PUBLIC cxx_std_23)
	target_include_directories(mngl_imgui PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mngl>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mngl/imgui>
	)
	target_link_libraries(mngl_imgui PUBLIC mngl)

	install(TARGETS mngl EXPORT MNGLTargets)
	install(TARGETS mngl_imgui EXPORT MNGLTargets
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
	install(EXPORT MNGLTargets
		FILE MNGLTargets.cmake
		NAMESPACE MNGL::
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MNGL"
	)
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/MNGL/"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mngl"
		FILES_MATCHING
			PATTERN "*.h"
			PATTERN "*.hpp"
			PATTERN "examples" EXCLUDE
			PATTERN ".github" EXCLUDE
	)
	# tree_practicum_ui.hpp includes these project headers by a relative path.
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tree/"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tree"
		FILES_MATCHING PATTERN "*.hpp"
	)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/LICENSE.txt"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/libmngl-dev/licenses"
	)

	configure_package_config_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/MNGLConfig.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/MNGLConfig.cmake"
		INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MNGL"
	)
	set(MNGL_PACKAGE_VERSION "0.0.0" CACHE STRING "Version used for the MNGL development package")
	string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" MNGL_CMAKE_CONFIG_VERSION "${MNGL_PACKAGE_VERSION}")
	if(NOT MNGL_CMAKE_CONFIG_VERSION)
		set(MNGL_CMAKE_CONFIG_VERSION "0.0.0")
	endif()
	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/MNGLConfigVersion.cmake"
		VERSION "${MNGL_CMAKE_CONFIG_VERSION}"
		COMPATIBILITY SameMajorVersion
	)
	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/MNGLConfig.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/MNGLConfigVersion.cmake"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MNGL"
	)

	set(CPACK_GENERATOR "DEB")
	set(CPACK_PACKAGE_NAME "libmngl-dev")
	set(CPACK_PACKAGE_VERSION "${MNGL_PACKAGE_VERSION}")
	set(CPACK_PACKAGE_CONTACT "MNGL maintainers")
	set(CPACK_PACKAGE_VENDOR "MNGL")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++23 MNGL development headers and CMake targets")
	set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "C++23 MNGL development headers, CMake targets, and Dear ImGui integration")
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
	set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
	set(CPACK_DEBIAN_PACKAGE_SECTION "libdevel")
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfreetype-dev, libglm-dev, libsdl3-dev, libsdl3-image-dev")
	set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
	set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
	include(CPack)
endif()
