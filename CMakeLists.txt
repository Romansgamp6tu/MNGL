cmake_minimum_required(VERSION 3.24)

project(MNGL LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)

option(MNGL_CUSTOM_AUDIO_BACKEND
	"Disable all MNGL audio integration" OFF)
option(MNGL_CUSTOM_AUDIO_INTERFACE
	"Disable the MNGL audio facade and bundled SDL3_mixer integration" OFF)
option(MNGL_CUSTOM_MIXER_INTERFACE
	"Disable the MNGL audio facade while keeping SDL3_mixer available directly" OFF)
option(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE "Create a Windows x64 MNGL development ZIP" OFF)
option(MNGL_BUILD_LINUX_DEVEL_PACKAGE "Create a Linux x64 MNGL development tarball" OFF)
option(MNGL_BUILD_MACOS_DEVEL_PACKAGE "Create a macOS MNGL development tarball" OFF)

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
		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:include/imgui>
	)
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		# Avoid shadowing the standard <version> header with Dear ImGui's metadata file.
		target_compile_options(mngl_imgui PRIVATE
			-idirafter "${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui")
		target_compile_options(mngl_imgui INTERFACE
			"$<BUILD_INTERFACE:-idirafter>"
			"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui>")
	else()
		target_include_directories(mngl_imgui PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui>)
	endif()
	target_link_libraries(mngl_imgui PUBLIC SDL3::SDL3)

	add_executable(mngl_demo MNGL/MNGL.cpp)
	target_compile_features(mngl_demo PRIVATE cxx_std_23)
	target_include_directories(mngl_demo PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
	)
endif()

set(MNGL_HAS_SDL3_MIXER FALSE)
if(NOT MNGL_CUSTOM_AUDIO_BACKEND AND NOT MNGL_CUSTOM_AUDIO_INTERFACE)
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_mixer/CMakeLists.txt")
		message(FATAL_ERROR
			"SDL3_mixer submodule is missing. Clone MNGL with --recurse-submodules or run git submodule update --init --recursive.")
	endif()
	if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE OR MNGL_BUILD_LINUX_DEVEL_PACKAGE OR MNGL_BUILD_MACOS_DEVEL_PACKAGE)
		set(SDLMIXER_INSTALL ON CACHE BOOL "Install SDL3_mixer with MNGL development packages" FORCE)
	endif()

	if(MSVC)
		if(NOT TARGET SDL3::Headers)
			add_library(SDL3::Headers INTERFACE IMPORTED GLOBAL)
			set_target_properties(SDL3::Headers PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include")
		endif()
		if(NOT TARGET SDL3::SDL3-shared)
			add_library(SDL3::SDL3-shared SHARED IMPORTED GLOBAL)
			set_target_properties(SDL3::SDL3-shared PROPERTIES
				IMPORTED_IMPLIB "${MNGL_LIBRARY_DIRECTORY}/SDL3.lib"
				IMPORTED_LOCATION "${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include")
		endif()
	endif()

	if(NOT DEFINED SDLMIXER_VENDORED)
		set(SDLMIXER_VENDORED ON)
	endif()
	if(NOT DEFINED SDLMIXER_DEPS_SHARED)
		set(SDLMIXER_DEPS_SHARED OFF)
	endif()
	if(NOT DEFINED SDLMIXER_INSTALL)
		set(SDLMIXER_INSTALL ON)
	endif()
	if(NOT DEFINED SDLMIXER_OPUS)
		set(SDLMIXER_OPUS OFF)
	endif()
	if(NOT DEFINED SDLMIXER_FLAC_LIBFLAC)
		set(SDLMIXER_FLAC_LIBFLAC OFF)
	endif()
	if(NOT DEFINED SDLMIXER_MP3_MPG123)
		set(SDLMIXER_MP3_MPG123 OFF)
	endif()
	if(NOT DEFINED SDLMIXER_VORBIS_VORBISFILE)
		set(SDLMIXER_VORBIS_VORBISFILE OFF)
	endif()
	if(NOT DEFINED SDLMIXER_GME)
		set(SDLMIXER_GME OFF)
	endif()
	if(NOT DEFINED SDLMIXER_MOD)
		set(SDLMIXER_MOD OFF)
	endif()
	if(NOT DEFINED SDLMIXER_MIDI)
		set(SDLMIXER_MIDI OFF)
	endif()
	if(NOT DEFINED SDLMIXER_WAVPACK)
		set(SDLMIXER_WAVPACK OFF)
	endif()
	if(NOT DEFINED SDLMIXER_TESTS)
		set(SDLMIXER_TESTS OFF)
	endif()
	if(NOT DEFINED SDLMIXER_EXAMPLES)
		set(SDLMIXER_EXAMPLES OFF)
	endif()

	if(DEFINED BUILD_SHARED_LIBS)
		set(MNGL_RESTORE_BUILD_SHARED_LIBS TRUE)
		set(MNGL_SAVED_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
	endif()
	set(BUILD_SHARED_LIBS ON)
	# Keep SDL3_mixer's install rules in the root runtime/development packages.
	add_subdirectory(third_party/SDL_mixer)
	if(MNGL_RESTORE_BUILD_SHARED_LIBS)
		set(BUILD_SHARED_LIBS "${MNGL_SAVED_BUILD_SHARED_LIBS}")
	else()
		unset(BUILD_SHARED_LIBS)
	endif()
	set(MNGL_HAS_SDL3_MIXER TRUE)
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
if(MNGL_CUSTOM_AUDIO_BACKEND)
	target_compile_definitions(mngl INTERFACE MNGL_CUSTOM_AUDIO_BACKEND)
endif()
if(MNGL_CUSTOM_AUDIO_INTERFACE)
	target_compile_definitions(mngl INTERFACE MNGL_CUSTOM_AUDIO_INTERFACE)
endif()
if(MNGL_CUSTOM_MIXER_INTERFACE)
	target_compile_definitions(mngl INTERFACE MNGL_CUSTOM_MIXER_INTERFACE)
endif()
if(NOT MNGL_CUSTOM_AUDIO_BACKEND AND NOT MNGL_CUSTOM_AUDIO_INTERFACE)
	target_include_directories(mngl INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_mixer/include>
	)
endif()
if(MSVC)
	target_link_directories(mngl INTERFACE
		$<BUILD_INTERFACE:${MNGL_LIBRARY_DIRECTORY}>
		$<INSTALL_INTERFACE:lib>
	)
	target_compile_options(mngl INTERFACE /bigobj)
	target_link_libraries(mngl INTERFACE mngl_imgui SDL3_image.lib freetype.lib)
else()
	target_link_libraries(mngl INTERFACE
		mngl_imgui SDL3_image::SDL3_image Freetype::Freetype)
endif()
if(APPLE)
	# The macOS runtime archive stores dynamic dependencies in ../lib from bin/.
	target_link_options(mngl INTERFACE "-Wl,-rpath,@executable_path/../lib")
endif()
if(MNGL_HAS_SDL3_MIXER)
	target_link_libraries(mngl INTERFACE SDL3_mixer::SDL3_mixer)
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_mixer/src/dr_libs/LICENSE"
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/SDL3_mixer/optional"
		RENAME "LICENSE.dr_libs.txt"
		COMPONENT library
	)
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/licenses/stb_vorbis.txt"
		DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/SDL3_mixer/optional"
		RENAME "LICENSE.stb_vorbis.txt"
		COMPONENT library
	)
endif()
target_link_libraries(mngl_demo PRIVATE mngl)

if(BUILD_TESTING)
	find_package(Threads REQUIRED)
	include(cmake/MNGLHeaderChecks.cmake)
	add_library(mngl_public_headers OBJECT tests/public_headers.cpp)
	target_link_libraries(mngl_public_headers PRIVATE mngl)
	add_library(mngl_public_headers_no_audio OBJECT tests/public_headers.cpp)
	target_link_libraries(mngl_public_headers_no_audio PRIVATE mngl)
	target_compile_definitions(mngl_public_headers_no_audio PRIVATE MNGL_CUSTOM_AUDIO_BACKEND)
	add_executable(mngl_core_api_test tests/core_api_test.cpp)
	target_link_libraries(mngl_core_api_test PRIVATE mngl Threads::Threads)
	add_test(NAME mngl_core_api_test COMMAND mngl_core_api_test)
	set_tests_properties(mngl_core_api_test PROPERTIES
		TIMEOUT 20
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	if(MNGL_HAS_SDL3_MIXER)
		add_executable(mngl_audio_runtime_smoke tests/audio_runtime_smoke.cpp)
		target_link_libraries(mngl_audio_runtime_smoke PRIVATE mngl)
		add_test(NAME mngl_audio_runtime_smoke COMMAND mngl_audio_runtime_smoke)
		set_tests_properties(mngl_audio_runtime_smoke PROPERTIES
			TIMEOUT 20
			ENVIRONMENT "SDL_AUDIODRIVER=dummy")
	endif()
	add_executable(mngl_gpu_runtime_smoke tests/gpu_runtime_smoke.cpp)
	target_link_libraries(mngl_gpu_runtime_smoke PRIVATE mngl)
	add_test(NAME mngl_gpu_runtime_smoke COMMAND mngl_gpu_runtime_smoke)
	set_tests_properties(mngl_gpu_runtime_smoke PROPERTIES
		TIMEOUT 30
		SKIP_RETURN_CODE 77)
	if(MSVC)
		target_compile_options(mngl_public_headers PRIVATE /utf-8)
		target_compile_options(mngl_public_headers_no_audio PRIVATE /utf-8)
		target_compile_options(mngl_core_api_test PRIVATE /utf-8)
		add_custom_command(TARGET mngl_core_api_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>" "${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll" "${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_core_api_test>"
			VERBATIM)
		if(MNGL_HAS_SDL3_MIXER)
			add_custom_command(TARGET mngl_core_api_test POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"$<TARGET_FILE:SDL3_mixer-shared>" "$<TARGET_FILE_DIR:mngl_core_api_test>"
				VERBATIM)
			add_custom_command(TARGET mngl_audio_runtime_smoke POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"$<TARGET_FILE:mngl_imgui>" "${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
					"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll" "${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
					"$<TARGET_FILE:SDL3_mixer-shared>" "$<TARGET_FILE_DIR:mngl_audio_runtime_smoke>"
				VERBATIM)
			add_custom_command(TARGET mngl_gpu_runtime_smoke POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"$<TARGET_FILE:SDL3_mixer-shared>" "$<TARGET_FILE_DIR:mngl_gpu_runtime_smoke>"
				VERBATIM)
		endif()
		add_custom_command(TARGET mngl_gpu_runtime_smoke POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>" "${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll" "${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_gpu_runtime_smoke>"
			VERBATIM)
	endif()
	add_executable(mngl_input_compat_test tests/input_compat_test.cpp)
	target_compile_features(mngl_input_compat_test PRIVATE cxx_std_23)
	target_link_libraries(mngl_input_compat_test PRIVATE mngl)
	add_test(NAME mngl_input_compat_test COMMAND mngl_input_compat_test)
	set_tests_properties(mngl_input_compat_test PROPERTIES
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	if(WIN32 AND MSVC)
		add_custom_command(TARGET mngl_input_compat_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>"
				"$<TARGET_FILE_DIR:mngl_input_compat_test>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"$<TARGET_FILE_DIR:mngl_input_compat_test>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
				"$<TARGET_FILE_DIR:mngl_input_compat_test>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_input_compat_test>"
			VERBATIM
		)
	endif()
	if(APPLE)
		add_executable(mngl_shader_compile_smoke tests/shader_compile_smoke.cpp)
		target_link_libraries(mngl_shader_compile_smoke PRIVATE mngl)
		target_compile_definitions(mngl_shader_compile_smoke PRIVATE
			"MNGL_SHADER_SOURCE_DIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders\"")
		add_test(NAME mngl_shader_compile_smoke COMMAND mngl_shader_compile_smoke)
		set_tests_properties(mngl_shader_compile_smoke PROPERTIES TIMEOUT 60)
	endif()
endif()

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
	if(MNGL_HAS_SDL3_MIXER)
		add_custom_command(TARGET mngl_demo POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:SDL3_mixer-shared>" "$<TARGET_FILE_DIR:mngl_demo>"
			VERBATIM)
	endif()
endif()

set(MNGL_DEVEL_PACKAGE_OPTIONS
	MNGL_BUILD_WINDOWS_DEVEL_PACKAGE
	MNGL_BUILD_LINUX_DEVEL_PACKAGE
	MNGL_BUILD_MACOS_DEVEL_PACKAGE
)
set(MNGL_ENABLED_DEVEL_PACKAGE_COUNT 0)
foreach(MNGL_DEVEL_PACKAGE_OPTION IN LISTS MNGL_DEVEL_PACKAGE_OPTIONS)
	if(${MNGL_DEVEL_PACKAGE_OPTION})
		math(EXPR MNGL_ENABLED_DEVEL_PACKAGE_COUNT "${MNGL_ENABLED_DEVEL_PACKAGE_COUNT} + 1")
	endif()
endforeach()
if(MNGL_ENABLED_DEVEL_PACKAGE_COUNT GREATER 1)
	message(FATAL_ERROR "Build only one MNGL development package per CMake build directory")
endif()

if(MNGL_ENABLED_DEVEL_PACKAGE_COUNT GREATER 0)
	include(CMakePackageConfigHelpers)
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
	set(MNGL_PACKAGE_VERSION "0.0.0" CACHE STRING "Version used for the MNGL development package")
	string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" MNGL_CMAKE_CONFIG_VERSION "${MNGL_PACKAGE_VERSION}")
	if(NOT MNGL_CMAKE_CONFIG_VERSION)
		set(MNGL_CMAKE_CONFIG_VERSION "0.0.0")
	endif()

	set(MNGL_CONFIG_FIND_DEPENDENCIES "")
	if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE)
		if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
			message(FATAL_ERROR "MNGL_BUILD_WINDOWS_DEVEL_PACKAGE requires 64-bit MSVC on Windows")
		endif()
		set(MNGL_DEVEL_PACKAGE_NAME "mngl-devel-windows-x64")
		set(MNGL_DEVEL_PACKAGE_GENERATOR "ZIP")
		if(MNGL_HAS_SDL3_MIXER)
			set(MNGL_CONFIG_FIND_DEPENDENCIES [=[include(CMakeFindDependencyMacro)
if(NOT TARGET SDL3::Headers)
	add_library(SDL3::Headers INTERFACE IMPORTED)
	set_property(TARGET SDL3::Headers PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/include")
endif()
if(NOT TARGET SDL3::SDL3-shared)
	add_library(SDL3::SDL3-shared SHARED IMPORTED)
	set_target_properties(SDL3::SDL3-shared PROPERTIES
		IMPORTED_IMPLIB "${PACKAGE_PREFIX_DIR}/lib/SDL3.lib"
		IMPORTED_LOCATION "${PACKAGE_PREFIX_DIR}/bin/SDL3.dll"
		INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_PREFIX_DIR}/include")
endif()
find_dependency(SDL3_mixer CONFIG REQUIRED)]=])
		endif()
	elseif(MNGL_BUILD_LINUX_DEVEL_PACKAGE)
		if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
			message(FATAL_ERROR "MNGL_BUILD_LINUX_DEVEL_PACKAGE requires 64-bit Linux")
		endif()
		set(MNGL_DEVEL_PACKAGE_NAME "mngl-devel-linux-x64")
		set(MNGL_DEVEL_PACKAGE_GENERATOR "TGZ")
		set(CPACK_PACKAGING_INSTALL_PREFIX "/")
		set(MNGL_CONFIG_FIND_DEPENDENCIES [=[include(CMakeFindDependencyMacro)
find_dependency(SDL3 CONFIG REQUIRED)
find_dependency(SDL3_image CONFIG REQUIRED)
find_dependency(Freetype REQUIRED)]=])
		if(MNGL_HAS_SDL3_MIXER)
			string(APPEND MNGL_CONFIG_FIND_DEPENDENCIES "\nfind_dependency(SDL3_mixer CONFIG REQUIRED)")
		endif()
	else()
		if(NOT APPLE OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
			message(FATAL_ERROR "MNGL_BUILD_MACOS_DEVEL_PACKAGE requires 64-bit macOS")
		endif()
		set(MNGL_PACKAGE_ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
		if(NOT MNGL_PACKAGE_ARCHITECTURE)
			set(MNGL_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
		endif()
		if(MNGL_PACKAGE_ARCHITECTURE MATCHES ";")
			message(FATAL_ERROR "MNGL macOS development packages must target one architecture per build")
		endif()
		if(MNGL_PACKAGE_ARCHITECTURE MATCHES "arm64|aarch64")
			set(MNGL_PACKAGE_ARCHITECTURE "arm64")
		elseif(MNGL_PACKAGE_ARCHITECTURE MATCHES "x86_64|AMD64")
			set(MNGL_PACKAGE_ARCHITECTURE "x64")
		else()
			message(FATAL_ERROR
				"Unsupported macOS package architecture: ${MNGL_PACKAGE_ARCHITECTURE}. Use arm64 or x86_64.")
		endif()
		set(MNGL_DEVEL_PACKAGE_NAME "mngl-devel-macos-${MNGL_PACKAGE_ARCHITECTURE}")
		set(MNGL_DEVEL_PACKAGE_GENERATOR "TGZ")
		set(CPACK_PACKAGING_INSTALL_PREFIX "/")
		set(MNGL_CONFIG_FIND_DEPENDENCIES [=[include(CMakeFindDependencyMacro)
find_dependency(SDL3 CONFIG REQUIRED)
find_dependency(SDL3_image CONFIG REQUIRED)
find_dependency(Freetype REQUIRED)]=])
		if(MNGL_HAS_SDL3_MIXER)
			string(APPEND MNGL_CONFIG_FIND_DEPENDENCIES "\nfind_dependency(SDL3_mixer CONFIG REQUIRED)")
		endif()
	endif()

	install(TARGETS mngl EXPORT MNGLTargets)
	install(TARGETS mngl_imgui EXPORT MNGLTargets
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
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
			if(MNGL_ADDED_PATH MATCHES "\\.(h|hpp|inc)$")
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
			"${CMAKE_CURRENT_SOURCE_DIR}/${source_directory}/*.inc"
		)
		foreach(MNGL_HEADER IN LISTS MNGL_HEADERS)
			file(RELATIVE_PATH MNGL_HEADER_RELATIVE_PATH
				"${CMAKE_CURRENT_SOURCE_DIR}" "${MNGL_HEADER}")
			file(TO_CMAKE_PATH "${MNGL_HEADER_RELATIVE_PATH}" MNGL_HEADER_RELATIVE_PATH)
			if(MNGL_HEADER_RELATIVE_PATH STREQUAL "MNGL/detail/audio/sdl3_mixer_backend.hpp"
				AND (MNGL_CUSTOM_AUDIO_BACKEND OR MNGL_CUSTOM_AUDIO_INTERFACE OR MNGL_CUSTOM_MIXER_INTERFACE))
				continue()
			endif()
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
	if(NOT MNGL_CUSTOM_AUDIO_BACKEND AND NOT MNGL_CUSTOM_AUDIO_INTERFACE)
		install(DIRECTORY
			"${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_mixer/include/SDL3_mixer"
			DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
		)
	endif()
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
	if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE)
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
	endif()
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

	set(CPACK_GENERATOR "${MNGL_DEVEL_PACKAGE_GENERATOR}")
	set(CPACK_PACKAGE_NAME "${MNGL_DEVEL_PACKAGE_NAME}")
	set(CPACK_PACKAGE_VERSION "${MNGL_PACKAGE_VERSION}")
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
	set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
	include(CPack)
endif()

