cmake_minimum_required(VERSION 3.24)

project(MNGL LANGUAGES CXX)

include(CTest)

execute_process(
	COMMAND git branch --show-current
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	OUTPUT_VARIABLE MNGL_CURRENT_GIT_BRANCH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT MNGL_CURRENT_GIT_BRANCH)
	set(MNGL_CURRENT_GIT_BRANCH "main")
endif()
set(MNGL_PACKAGE_BRANCH "${MNGL_CURRENT_GIT_BRANCH}" CACHE STRING
	"Git branch name used to namespace branch-only headers")
set(MNGL_PACKAGE_BASE_REF "origin/main" CACHE STRING "Git ref used to identify headers added outside main")
if(MNGL_PACKAGE_BRANCH STREQUAL "main")
	set(MNGL_PACKAGE_HEADER_NAMESPACE "MNGL")
else()
	set(MNGL_PACKAGE_HEADER_NAMESPACE "${MNGL_PACKAGE_BRANCH}")
	if(MNGL_PACKAGE_HEADER_NAMESPACE MATCHES "(^|/)\\.\\.?(/|$)" OR
		MNGL_PACKAGE_HEADER_NAMESPACE MATCHES "[<>:\"|?*;\\\\]")
		message(FATAL_ERROR "MNGL_PACKAGE_BRANCH must be a safe Git branch name")
	endif()
endif()

set(MNGL_IMGUI_SOURCES
	MNGL/imgui/imgui.cpp
	MNGL/imgui/imgui_draw.cpp
	MNGL/imgui/imgui_tables.cpp
	MNGL/imgui/imgui_widgets.cpp
	MNGL/imgui/backends/imgui_impl_sdl3.cpp
	MNGL/imgui/backends/imgui_impl_sdlgpu3.cpp
)

if(MSVC)
	# utilities.hpp uses MSVC linker pragmas for these bundled libraries.
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(MNGL_LIBRARY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs")
	else()
		set(MNGL_LIBRARY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs/x86")
	endif()
	add_library(mngl_imgui SHARED ${MNGL_IMGUI_SOURCES})
	target_compile_definitions(mngl_imgui
		PRIVATE "IMGUI_API=__declspec(dllexport)"
		INTERFACE "IMGUI_API=__declspec(dllimport)"
	)
	target_compile_options(mngl_imgui PRIVATE /utf-8)
	target_include_directories(mngl_imgui PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui>
		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:include/imgui>
	)
	target_link_directories(mngl_imgui PUBLIC
		$<BUILD_INTERFACE:${MNGL_LIBRARY_DIRECTORY}>
		$<INSTALL_INTERFACE:lib>
	)
	target_link_libraries(mngl_imgui PUBLIC SDL3.lib)

	add_executable(mngl_demo MNGL/MNGL.cpp)
	target_compile_features(mngl_demo PRIVATE cxx_std_23)
	target_include_directories(mngl_demo PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui"
	)
	# utilities.hpp uses MSVC linker pragmas for these bundled libraries.
	target_link_directories(mngl_demo PRIVATE "${MNGL_LIBRARY_DIRECTORY}")
else()
	find_package(SDL3 CONFIG REQUIRED)
	find_package(SDL3_image CONFIG REQUIRED)
	find_package(Freetype REQUIRED)

	add_library(mngl_imgui SHARED ${MNGL_IMGUI_SOURCES})
	target_include_directories(mngl_imgui PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui>
		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:include/imgui>
	)
	target_link_libraries(mngl_imgui PUBLIC SDL3::SDL3)

	add_executable(mngl_demo MNGL/MNGL.cpp)
	target_compile_features(mngl_demo PRIVATE cxx_std_23)
	target_include_directories(mngl_demo PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui"
	)
endif()

add_library(MNGL::ImGui ALIAS mngl_imgui)
set_target_properties(mngl_imgui PROPERTIES EXPORT_NAME ImGui)
target_compile_features(mngl_imgui PUBLIC cxx_std_23)

add_library(mngl INTERFACE)
add_library(MNGL::MNGL ALIAS mngl)
set_target_properties(mngl PROPERTIES EXPORT_NAME MNGL)
target_compile_features(mngl INTERFACE cxx_std_23)
target_include_directories(mngl INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	$<INSTALL_INTERFACE:include/MNGL>
	$<INSTALL_INTERFACE:include/${MNGL_PACKAGE_HEADER_NAMESPACE}>
)
if(MSVC)
	target_link_directories(mngl INTERFACE
		$<BUILD_INTERFACE:${MNGL_LIBRARY_DIRECTORY}>
		$<INSTALL_INTERFACE:lib>
	)
	target_link_libraries(mngl INTERFACE mngl_imgui SDL3_image.lib freetype.lib)
else()
	target_link_libraries(mngl INTERFACE
		mngl_imgui SDL3_image::SDL3_image Freetype::Freetype)
endif()
target_link_libraries(mngl_demo PRIVATE mngl)

option(MNGL_BUILD_MOD_EXAMPLE "Build the native mod host and example module" ON)
include(cmake/MNGLMods.cmake)
mngl_add_mod_targets("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

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

if(WIN32)
	add_custom_command(TARGET mngl_demo POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"$<TARGET_FILE:mngl_imgui>"
			"$<TARGET_FILE_DIR:mngl_demo>"
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

option(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE "Create a Windows x64 MNGL development ZIP" OFF)
if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE)
	if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		message(FATAL_ERROR "MNGL_BUILD_WINDOWS_DEVEL_PACKAGE requires 64-bit MSVC on Windows")
	endif()

	include(GNUInstallDirs)
	include(CMakePackageConfigHelpers)
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
	set(MNGL_PACKAGE_VERSION "0.0.0" CACHE STRING "Version used for the Windows development package")
	string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" MNGL_CMAKE_CONFIG_VERSION "${MNGL_PACKAGE_VERSION}")
	if(NOT MNGL_CMAKE_CONFIG_VERSION)
		set(MNGL_CMAKE_CONFIG_VERSION "0.0.0")
	endif()

	install(TARGETS mngl EXPORT MNGLTargets)
	install(TARGETS mngl_imgui EXPORT MNGLTargets
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
	install(EXPORT MNGLTargets
		FILE MNGLTargets.cmake
		NAMESPACE MNGL::
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MNGL"
	)
	set(MNGL_BRANCH_ONLY_HEADERS)
	if(NOT MNGL_PACKAGE_BRANCH STREQUAL "main")
		execute_process(
			COMMAND git diff --name-only --diff-filter=A
				"${MNGL_PACKAGE_BASE_REF}...HEAD" --
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			RESULT_VARIABLE MNGL_BRANCH_DIFF_RESULT
			OUTPUT_VARIABLE MNGL_BRANCH_ADDED_PATHS
			ERROR_VARIABLE MNGL_BRANCH_DIFF_ERROR
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		if(NOT MNGL_BRANCH_DIFF_RESULT EQUAL 0)
			message(FATAL_ERROR
				"Could not compare ${MNGL_PACKAGE_BRANCH} with ${MNGL_PACKAGE_BASE_REF} to place branch-only headers: ${MNGL_BRANCH_DIFF_ERROR}")
		endif()
		string(REPLACE "\r\n" "\n" MNGL_BRANCH_ADDED_PATHS "${MNGL_BRANCH_ADDED_PATHS}")
		string(REPLACE "\n" ";" MNGL_BRANCH_ADDED_PATHS "${MNGL_BRANCH_ADDED_PATHS}")
		foreach(MNGL_ADDED_PATH IN LISTS MNGL_BRANCH_ADDED_PATHS)
			file(TO_CMAKE_PATH "${MNGL_ADDED_PATH}" MNGL_ADDED_PATH)
			if(MNGL_ADDED_PATH MATCHES "\\.(h|hpp)$")
				list(APPEND MNGL_BRANCH_ONLY_HEADERS "${MNGL_ADDED_PATH}")
			endif()
		endforeach()
	endif()

	function(mngl_install_header_tree source_directory)
		if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_directory}")
			return()
		endif()
		file(GLOB_RECURSE MNGL_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false
			"${CMAKE_CURRENT_SOURCE_DIR}/${source_directory}/*.h"
			"${CMAKE_CURRENT_SOURCE_DIR}/${source_directory}/*.hpp"
		)
		foreach(MNGL_HEADER IN LISTS MNGL_HEADERS)
			file(RELATIVE_PATH MNGL_HEADER_RELATIVE_PATH
				"${CMAKE_CURRENT_SOURCE_DIR}" "${MNGL_HEADER}")
			file(TO_CMAKE_PATH "${MNGL_HEADER_RELATIVE_PATH}" MNGL_HEADER_RELATIVE_PATH)
			if(MNGL_HEADER_RELATIVE_PATH MATCHES "^MNGL/imgui(/|$)")
				continue()
			endif()
			get_filename_component(MNGL_HEADER_DIRECTORY "${MNGL_HEADER_RELATIVE_PATH}" DIRECTORY)
			if(MNGL_HEADER_RELATIVE_PATH IN_LIST MNGL_BRANCH_ONLY_HEADERS)
				set(MNGL_HEADER_INSTALL_DIRECTORY
					"${CMAKE_INSTALL_INCLUDEDIR}/${MNGL_PACKAGE_HEADER_NAMESPACE}/${MNGL_HEADER_DIRECTORY}")
			else()
				set(MNGL_HEADER_INSTALL_DIRECTORY
					"${CMAKE_INSTALL_INCLUDEDIR}/${MNGL_HEADER_DIRECTORY}")
			endif()
			install(FILES "${MNGL_HEADER}" DESTINATION "${MNGL_HEADER_INSTALL_DIRECTORY}")
		endforeach()
	endfunction()

	mngl_install_header_tree("MNGL")
	set(MNGL_BRANCH_HEADER_DIRECTORIES "tree" CACHE STRING
		"Additional project header directories whose new headers are branch-namespaced")
	foreach(MNGL_BRANCH_HEADER_DIRECTORY IN LISTS MNGL_BRANCH_HEADER_DIRECTORIES)
		mngl_install_header_tree("${MNGL_BRANCH_HEADER_DIRECTORY}")
	endforeach()

	install(DIRECTORY
		"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3"
		"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_image"
		"${CMAKE_CURRENT_SOURCE_DIR}/include/glm"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
	)
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/imconfig.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/imgui.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/imstb_truetype.h"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/imgui"
	)
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/backends/imgui_impl_sdl3.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/backends/imgui_impl_sdlgpu3.h"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/imgui/backends"
	)
	install(FILES
		"${MNGL_LIBRARY_DIRECTORY}/SDL3.lib"
		"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.lib"
		"${MNGL_LIBRARY_DIRECTORY}/SDL3_test.lib"
		"${MNGL_LIBRARY_DIRECTORY}/freetype.lib"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
	install(DIRECTORY "${MNGL_LIBRARY_DIRECTORY}/"
		DESTINATION "${CMAKE_INSTALL_BINDIR}"
		FILES_MATCHING PATTERN "*.dll"
	)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/LICENSE.txt"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/MNGL"
	)

	configure_package_config_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/MNGLConfig.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/MNGLConfig.cmake"
		INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MNGL"
	)
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

	set(CPACK_GENERATOR "ZIP")
	set(CPACK_PACKAGE_NAME "mngl-devel-windows-x64")
	set(CPACK_PACKAGE_VERSION "${MNGL_PACKAGE_VERSION}")
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
	set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
	include(CPack)
endif()
