cmake_minimum_required(VERSION 3.24)

project(MNGL LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)

set(MNGL_METAL_SHADER_ASSET_DIR "" CACHE PATH
	"Directory with offline-generated MSL sidecars for built-in runtime shaders")

file(GLOB MNGL_RUNTIME_SHADER_ASSETS CONFIGURE_DEPENDS LIST_DIRECTORIES false
	"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders/*")
set(MNGL_RUNTIME_SHADER_ASSET_NAMES)
foreach(MNGL_RUNTIME_SHADER_ASSET IN LISTS MNGL_RUNTIME_SHADER_ASSETS)
	get_filename_component(MNGL_RUNTIME_SHADER_ASSET_NAME "${MNGL_RUNTIME_SHADER_ASSET}" NAME)
	list(APPEND MNGL_RUNTIME_SHADER_ASSET_NAMES "${MNGL_RUNTIME_SHADER_ASSET_NAME}")
endforeach()
string(REPLACE ";" "\n" MNGL_RUNTIME_SHADER_ASSET_MANIFEST_CONTENT
	"${MNGL_RUNTIME_SHADER_ASSET_NAMES}")
file(CONFIGURE
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mngl-runtime-shader-assets.txt"
	CONTENT "${MNGL_RUNTIME_SHADER_ASSET_MANIFEST_CONTENT}\n")
file(CONFIGURE
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mngl-stage-runtime-shaders.cmake"
	CONTENT [=[
cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED MNGL_SHADER_SOURCE OR NOT DEFINED MNGL_SHADER_DESTINATION OR NOT DEFINED MNGL_SHADER_MANIFEST)
	message(FATAL_ERROR "Runtime shader staging requires source, destination, and manifest paths")
endif()

file(MAKE_DIRECTORY "${MNGL_SHADER_DESTINATION}")
file(LOCK "${MNGL_SHADER_DESTINATION}/.mngl-runtime-shaders.lock" GUARD PROCESS TIMEOUT 60)
file(STRINGS "${MNGL_SHADER_MANIFEST}" source_assets)
set(current_assets ${source_assets})
if(DEFINED MNGL_METAL_SHADER_SOURCE AND IS_DIRECTORY "${MNGL_METAL_SHADER_SOURCE}")
	file(GLOB metal_shader_assets LIST_DIRECTORIES false "${MNGL_METAL_SHADER_SOURCE}/*.msl")
	foreach(metal_shader IN LISTS metal_shader_assets)
		get_filename_component(metal_shader_name "${metal_shader}" NAME)
		list(APPEND current_assets "${metal_shader_name}")
	endforeach()
endif()
list(REMOVE_DUPLICATES current_assets)
list(SORT current_assets)
set(current_manifest_content "")
foreach(asset IN LISTS current_assets)
	string(APPEND current_manifest_content "${asset}\n")
endforeach()
set(managed_manifest "${MNGL_SHADER_DESTINATION}/.mngl-runtime-shaders.manifest")
if(EXISTS "${managed_manifest}")
	file(STRINGS "${managed_manifest}" previously_managed_assets)
	foreach(asset IN LISTS previously_managed_assets)
		if(NOT asset IN_LIST current_assets)
			file(REMOVE "${MNGL_SHADER_DESTINATION}/${asset}")
		endif()
	endforeach()
endif()

foreach(asset IN LISTS current_assets)
	if(EXISTS "${MNGL_SHADER_SOURCE}/${asset}")
		file(COPY_FILE "${MNGL_SHADER_SOURCE}/${asset}" "${MNGL_SHADER_DESTINATION}/${asset}" ONLY_IF_DIFFERENT)
	elseif(DEFINED MNGL_METAL_SHADER_SOURCE AND EXISTS "${MNGL_METAL_SHADER_SOURCE}/${asset}")
		file(COPY_FILE "${MNGL_METAL_SHADER_SOURCE}/${asset}" "${MNGL_SHADER_DESTINATION}/${asset}" ONLY_IF_DIFFERENT)
	else()
		message(FATAL_ERROR "Runtime shader asset is missing: ${asset}")
	endif()
endforeach()
file(WRITE "${managed_manifest}" "${current_manifest_content}")
]=]
	@ONLY)

function(mngl_stage_runtime_shaders target)
	set(shader_destination "$<TARGET_FILE_DIR:${target}>/shaders")
	add_custom_target("${target}_runtime_shaders"
		COMMAND "${CMAKE_COMMAND}"
			"-DMNGL_SHADER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders"
			"-DMNGL_METAL_SHADER_SOURCE=${MNGL_METAL_SHADER_ASSET_DIR}"
			"-DMNGL_SHADER_DESTINATION=${shader_destination}"
			"-DMNGL_SHADER_MANIFEST=${CMAKE_CURRENT_BINARY_DIR}/mngl-runtime-shader-assets.txt"
			-P "${CMAKE_CURRENT_BINARY_DIR}/mngl-stage-runtime-shaders.cmake"
		DEPENDS ${MNGL_RUNTIME_SHADER_ASSETS}
			"${CMAKE_CURRENT_BINARY_DIR}/mngl-runtime-shader-assets.txt"
			"${CMAKE_CURRENT_BINARY_DIR}/mngl-stage-runtime-shaders.cmake"
		COMMENT "Staging runtime shaders for ${target}"
		VERBATIM
	)
	add_dependencies("${target}" "${target}_runtime_shaders")
endfunction()

option(MNGL_CUSTOM_AUDIO_BACKEND
	"Disable all MNGL audio integration" OFF)
option(MNGL_CUSTOM_AUDIO_INTERFACE
	"Disable the MNGL audio facade and bundled SDL3_mixer integration" OFF)
option(MNGL_CUSTOM_MIXER_INTERFACE
	"Disable the MNGL audio facade while keeping SDL3_mixer available directly" OFF)
option(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE "Create a Windows x64 MNGL development ZIP" OFF)
option(MNGL_BUILD_LINUX_DEVEL_PACKAGE "Create a Linux x64 MNGL development tarball" OFF)
option(MNGL_BUILD_MACOS_DEVEL_PACKAGE "Create a macOS MNGL development tarball" OFF)
option(MNGL_BUILD_2D_BENCHMARK "Build the interactive 2D profiling scene" OFF)
option(MNGL_USE_SYSTEM_DEPENDENCIES
	"Use CMake packages for SDL3, SDL3_image, FreeType, GLM, and SDL3_mixer" OFF)
option(MNGL_BUILD_SHARED_LIBS "Build the Dear ImGui library as a shared library" ON)
option(MNGL_BUILD_DEMO "Build the MNGL demo application" ON)
if(MNGL_BUILD_SHARED_LIBS)
	set(MNGL_IMGUI_LIBRARY_TYPE SHARED)
else()
	set(MNGL_IMGUI_LIBRARY_TYPE STATIC)
endif()

execute_process(
	COMMAND git -c "safe.directory=${CMAKE_CURRENT_SOURCE_DIR}" branch --show-current
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

set(MNGL_BRANCH_ONLY_HEADERS)
if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE OR MNGL_BUILD_LINUX_DEVEL_PACKAGE OR MNGL_BUILD_MACOS_DEVEL_PACKAGE)
	if(NOT MNGL_PACKAGE_BRANCH STREQUAL "main")
		execute_process(
			COMMAND git -c "safe.directory=${CMAKE_CURRENT_SOURCE_DIR}" diff --name-only --diff-filter=A
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
	
endif()

set(MNGL_IMGUI_SOURCES
	MNGL/imgui/imgui.cpp
	MNGL/imgui/imgui_draw.cpp
	MNGL/imgui/imgui_tables.cpp
	MNGL/imgui/imgui_widgets.cpp
	MNGL/imgui/backends/imgui_impl_sdl3.cpp
	MNGL/imgui/backends/imgui_impl_sdlgpu3.cpp
)

if(MSVC AND NOT MNGL_USE_SYSTEM_DEPENDENCIES)
	# utilities.hpp uses MSVC linker pragmas for these bundled libraries.
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(MNGL_LIBRARY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs")
	else()
		set(MNGL_LIBRARY_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs/x86")
	endif()
	file(GLOB MNGL_WINDOWS_CODEC_DLLS CONFIGURE_DEPENDS
		"${MNGL_LIBRARY_DIRECTORY}/*.dll")
	list(REMOVE_ITEM MNGL_WINDOWS_CODEC_DLLS
		"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
		"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
		"${MNGL_LIBRARY_DIRECTORY}/freetype.dll")
	add_library(mngl_imgui ${MNGL_IMGUI_LIBRARY_TYPE} ${MNGL_IMGUI_SOURCES})
	if(MNGL_BUILD_SHARED_LIBS)
		target_compile_definitions(mngl_imgui
			PRIVATE "IMGUI_API=__declspec(dllexport)"
			INTERFACE "IMGUI_API=__declspec(dllimport)")
	endif()
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

	if(MNGL_BUILD_DEMO)
		add_executable(mngl_demo MNGL/MNGL.cpp)
		target_compile_features(mngl_demo PRIVATE cxx_std_23)
		target_include_directories(mngl_demo PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}/include"
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui"
		)
		target_link_directories(mngl_demo PRIVATE "${MNGL_LIBRARY_DIRECTORY}")
	endif()
else()
	if(MNGL_USE_SYSTEM_DEPENDENCIES)
		find_package(SDL3 3.4.18 CONFIG REQUIRED)
		find_package(SDL3_image 3.4.8 CONFIG REQUIRED)
		find_package(glm 1.0.3 CONFIG REQUIRED)
	else()
		find_package(SDL3 CONFIG REQUIRED)
		find_package(SDL3_image CONFIG REQUIRED)
	endif()
	find_package(Freetype REQUIRED)

	add_library(mngl_imgui ${MNGL_IMGUI_LIBRARY_TYPE} ${MNGL_IMGUI_SOURCES})
	if(MSVC)
		target_compile_options(mngl_imgui PRIVATE /utf-8)
		if(MNGL_BUILD_SHARED_LIBS)
			target_compile_definitions(mngl_imgui
				PRIVATE "IMGUI_API=__declspec(dllexport)"
				INTERFACE "IMGUI_API=__declspec(dllimport)")
		endif()
	endif()
	target_include_directories(mngl_imgui PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/MNGL>
		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:include/imgui>
	)
	if(NOT MNGL_USE_SYSTEM_DEPENDENCIES)
		target_include_directories(mngl_imgui PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
	endif()
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

	if(MNGL_BUILD_DEMO)
		add_executable(mngl_demo MNGL/MNGL.cpp)
		target_compile_features(mngl_demo PRIVATE cxx_std_23)
		target_include_directories(mngl_demo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/MNGL")
		if(NOT MNGL_USE_SYSTEM_DEPENDENCIES)
			target_include_directories(mngl_demo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
		endif()
	endif()
endif()

function(mngl_copy_windows_codec_dlls target)
	if(MSVC AND NOT MNGL_USE_SYSTEM_DEPENDENCIES AND MNGL_WINDOWS_CODEC_DLLS)
		add_custom_command(TARGET "${target}" POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				${MNGL_WINDOWS_CODEC_DLLS}
				"$<TARGET_FILE_DIR:${target}>"
			COMMAND_EXPAND_LISTS
			VERBATIM)
	endif()
endfunction()

function(mngl_copy_windows_runtime_dlls target)
	if(MSVC AND NOT MNGL_USE_SYSTEM_DEPENDENCIES)
		add_custom_command(TARGET "${target}" POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
				"${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:${target}>"
			VERBATIM)
		mngl_copy_windows_codec_dlls("${target}")
	endif()
endfunction()

function(mngl_copy_default_2d_shaders target)
	add_custom_command(TARGET "${target}" POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders"
			"$<TARGET_FILE_DIR:${target}>/shaders"
		VERBATIM)
endfunction()

set(MNGL_HAS_SDL3_MIXER FALSE)
if(NOT MNGL_CUSTOM_AUDIO_BACKEND AND NOT MNGL_CUSTOM_AUDIO_INTERFACE)
	if(MNGL_USE_SYSTEM_DEPENDENCIES)
		find_package(SDL3_mixer 3.2.4 CONFIG REQUIRED)
		set(MNGL_HAS_SDL3_MIXER TRUE)
	else()
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
	$<INSTALL_INTERFACE:include>
	$<INSTALL_INTERFACE:include/MNGL>
)
if(NOT MNGL_USE_SYSTEM_DEPENDENCIES)
	target_include_directories(mngl INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
endif()
if(MNGL_BRANCH_ONLY_HEADERS)
	target_include_directories(mngl INTERFACE
		$<INSTALL_INTERFACE:include/${MNGL_PACKAGE_HEADER_NAMESPACE}>
	)
endif()
if(MNGL_CUSTOM_AUDIO_BACKEND)
	target_compile_definitions(mngl INTERFACE MNGL_CUSTOM_AUDIO_BACKEND)
endif()
if(MNGL_CUSTOM_AUDIO_INTERFACE)
	target_compile_definitions(mngl INTERFACE MNGL_CUSTOM_AUDIO_INTERFACE)
endif()
if(MNGL_CUSTOM_MIXER_INTERFACE)
	target_compile_definitions(mngl INTERFACE MNGL_CUSTOM_MIXER_INTERFACE)
endif()
if(NOT MNGL_USE_SYSTEM_DEPENDENCIES AND NOT MNGL_CUSTOM_AUDIO_BACKEND AND NOT MNGL_CUSTOM_AUDIO_INTERFACE)
	target_include_directories(mngl INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_mixer/include>)
endif()
if(MSVC AND NOT MNGL_USE_SYSTEM_DEPENDENCIES)
	target_link_directories(mngl INTERFACE
		$<BUILD_INTERFACE:${MNGL_LIBRARY_DIRECTORY}>
		$<INSTALL_INTERFACE:lib>
	)
	target_compile_options(mngl INTERFACE /bigobj)
	target_link_libraries(mngl INTERFACE mngl_imgui SDL3_image.lib freetype.lib)
else()
	target_link_libraries(mngl INTERFACE mngl_imgui SDL3_image::SDL3_image Freetype::Freetype)
	if(MSVC)
		target_compile_options(mngl INTERFACE /bigobj)
	endif()
	if(MNGL_USE_SYSTEM_DEPENDENCIES)
		target_link_libraries(mngl INTERFACE glm::glm)
	endif()
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
		CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "13")
	# libstdc++ keeps std::stacktrace's implementation in a separate archive.
	# GCC 13.0–13.2 use the old archive name; GCC 13.3+ use libstdc++exp.
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.3")
		target_link_libraries(mngl INTERFACE stdc++_libbacktrace)
	else()
		target_link_libraries(mngl INTERFACE stdc++exp)
	endif()
endif()
if(APPLE)
	# The macOS runtime archive stores dynamic dependencies in ../lib from bin/.
	target_link_options(mngl INTERFACE "-Wl,-rpath,@executable_path/../lib")
endif()
if(MNGL_HAS_SDL3_MIXER)
	target_link_libraries(mngl INTERFACE SDL3_mixer::SDL3_mixer)
	if(NOT MNGL_USE_SYSTEM_DEPENDENCIES)
		install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_mixer/src/dr_libs/LICENSE"
			DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/SDL3_mixer/optional"
			RENAME "LICENSE.dr_libs.txt" COMPONENT library)
		install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/licenses/stb_vorbis.txt"
			DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/SDL3_mixer/optional"
			RENAME "LICENSE.stb_vorbis.txt" COMPONENT library)
	endif()
endif()
if(MNGL_BUILD_DEMO)
	target_link_libraries(mngl_demo PRIVATE mngl)
endif()

if(MNGL_BUILD_2D_BENCHMARK)
	add_executable(mngl_2d_benchmark examples/2d-benchmark/main.cpp)
	target_link_libraries(mngl_2d_benchmark PRIVATE mngl)
	if(MSVC)
		target_compile_options(mngl_2d_benchmark PRIVATE /utf-8)
	endif()
	mngl_stage_runtime_shaders(mngl_2d_benchmark)
	if(WIN32)
		mngl_copy_windows_codec_dlls(mngl_2d_benchmark)
		add_custom_command(TARGET mngl_2d_benchmark POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
				"${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_2d_benchmark>"
			VERBATIM
		)
		if(MNGL_HAS_SDL3_MIXER)
			add_custom_command(TARGET mngl_2d_benchmark POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"$<TARGET_FILE:SDL3_mixer-shared>" "$<TARGET_FILE_DIR:mngl_2d_benchmark>"
				VERBATIM)
		endif()
	endif()
endif()

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
	add_executable(mngl_event_bus_test tests/event_bus_test.cpp)
	target_link_libraries(mngl_event_bus_test PRIVATE mngl)
	add_test(NAME mngl_event_bus_test COMMAND mngl_event_bus_test)
	set_tests_properties(mngl_event_bus_test PROPERTIES
		TIMEOUT 20
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	add_executable(mngl_async_io_close_retry_test tests/async_io_close_retry_test.cpp)
	target_compile_features(mngl_async_io_close_retry_test PRIVATE cxx_std_23)
	target_compile_definitions(mngl_async_io_close_retry_test PRIVATE MNGL_ASYNC_IO_TESTING=1)
	target_link_libraries(mngl_async_io_close_retry_test PRIVATE mngl)
	add_test(NAME mngl_async_io_close_retry_test COMMAND mngl_async_io_close_retry_test)
	set_tests_properties(mngl_async_io_close_retry_test PROPERTIES
		TIMEOUT 20
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	add_executable(mngl_hit_test_spatial_index_test tests/hit_test_spatial_index_test.cpp)
	target_compile_features(mngl_hit_test_spatial_index_test PRIVATE cxx_std_23)
	target_link_libraries(mngl_hit_test_spatial_index_test PRIVATE mngl Threads::Threads)
	add_test(NAME mngl_hit_test_spatial_index_test COMMAND mngl_hit_test_spatial_index_test)
	set_tests_properties(mngl_hit_test_spatial_index_test PROPERTIES TIMEOUT 20)
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
	add_executable(mngl_metal_shader_assets_smoke tests/metal_shader_assets_smoke.cpp)
	target_link_libraries(mngl_metal_shader_assets_smoke PRIVATE mngl)
	if(APPLE)
		mngl_stage_runtime_shaders(mngl_metal_shader_assets_smoke)
		add_test(NAME mngl_metal_shader_assets_smoke COMMAND mngl_metal_shader_assets_smoke)
		set_tests_properties(mngl_metal_shader_assets_smoke PROPERTIES TIMEOUT 20)
	endif()
	add_executable(mngl_svg_test tests/svg_rendering_test.cpp)
	target_link_libraries(mngl_svg_test PRIVATE mngl)
	add_test(NAME mngl_svg_test COMMAND mngl_svg_test)
	set_tests_properties(mngl_svg_test PROPERTIES TIMEOUT 20)
	add_executable(mngl_svg_upload_lifecycle_test tests/svg_upload_lifecycle_test.cpp)
	target_link_libraries(mngl_svg_upload_lifecycle_test PRIVATE mngl)
	target_compile_definitions(mngl_svg_upload_lifecycle_test PRIVATE
		"MNGL_TEST_FONT_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/misc/fonts/ProggyClean.ttf\"")
	mngl_stage_runtime_shaders(mngl_svg_upload_lifecycle_test)
	add_test(NAME mngl_svg_upload_lifecycle_test COMMAND mngl_svg_upload_lifecycle_test)
	set_tests_properties(mngl_svg_upload_lifecycle_test PROPERTIES
		TIMEOUT 30
		SKIP_RETURN_CODE 77)
	add_executable(mngl_svg_stroke_overlap_test tests/svg_stroke_overlap_test.cpp)
	target_link_libraries(mngl_svg_stroke_overlap_test PRIVATE mngl)
	mngl_stage_runtime_shaders(mngl_svg_stroke_overlap_test)
	add_test(NAME mngl_svg_stroke_overlap_test COMMAND mngl_svg_stroke_overlap_test)
	set_tests_properties(mngl_svg_stroke_overlap_test PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
	add_executable(mngl_svg_runtime_test tests/svg_runtime_test.cpp)
	target_link_libraries(mngl_svg_runtime_test PRIVATE mngl)
	mngl_stage_runtime_shaders(mngl_svg_runtime_test)
	target_compile_features(mngl_svg_runtime_test PRIVATE cxx_std_23)
	target_compile_definitions(mngl_svg_runtime_test PRIVATE
		"MNGL_SVG_TEST_ASSET_DIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/assets\"")
	foreach(svg_backend IN ITEMS direct3d12 vulkan)
		string(REPLACE "-" "_" svg_backend_target "${svg_backend}")
		add_test(NAME "mngl_svg_runtime_${svg_backend_target}" COMMAND mngl_svg_runtime_test)
		set_tests_properties("mngl_svg_runtime_${svg_backend_target}" PROPERTIES
			TIMEOUT 60
			SKIP_RETURN_CODE 77
			ENVIRONMENT "SDL_GPU_DRIVER=${svg_backend}")
	endforeach()
	if(NOT WIN32)
		add_test(NAME mngl_svg_runtime_default COMMAND mngl_svg_runtime_test)
		set_tests_properties(mngl_svg_runtime_default PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)
	endif()
	if(MSVC)
		target_compile_options(mngl_public_headers PRIVATE /utf-8)
		target_compile_options(mngl_public_headers_no_audio PRIVATE /utf-8)
		target_compile_options(mngl_core_api_test PRIVATE /utf-8)
		target_compile_options(mngl_async_io_close_retry_test PRIVATE /utf-8)
		target_compile_options(mngl_hit_test_spatial_index_test PRIVATE /utf-8)
		target_compile_options(mngl_svg_test PRIVATE /utf-8)
		target_compile_options(mngl_svg_upload_lifecycle_test PRIVATE /utf-8)
		target_compile_options(mngl_svg_stroke_overlap_test PRIVATE /utf-8)
		target_compile_options(mngl_svg_runtime_test PRIVATE /utf-8)
		add_custom_command(TARGET mngl_svg_runtime_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>" "${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll" "${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_svg_runtime_test>"
			VERBATIM)
		add_custom_command(TARGET mngl_svg_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
				"$<TARGET_FILE_DIR:mngl_svg_test>"
			VERBATIM)
		add_custom_command(TARGET mngl_svg_upload_lifecycle_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
				"$<TARGET_FILE_DIR:mngl_svg_upload_lifecycle_test>"
			VERBATIM)
		add_custom_command(TARGET mngl_svg_stroke_overlap_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll"
				"$<TARGET_FILE_DIR:mngl_svg_stroke_overlap_test>"
			VERBATIM)
		add_custom_command(TARGET mngl_core_api_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>" "${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll" "${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_core_api_test>"
			VERBATIM)
		add_custom_command(TARGET mngl_async_io_close_retry_test POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>" "${MNGL_LIBRARY_DIRECTORY}/SDL3.dll"
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll" "${MNGL_LIBRARY_DIRECTORY}/freetype.dll"
				"$<TARGET_FILE_DIR:mngl_async_io_close_retry_test>"
			VERBATIM)
		mngl_copy_windows_codec_dlls(mngl_core_api_test)
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
			mngl_copy_windows_codec_dlls(mngl_audio_runtime_smoke)
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
		mngl_copy_windows_codec_dlls(mngl_gpu_runtime_smoke)
	endif()
	add_executable(mngl_input_compat_test tests/input_compat_test.cpp)
	target_compile_features(mngl_input_compat_test PRIVATE cxx_std_23)
	target_link_libraries(mngl_input_compat_test PRIVATE mngl)
	add_test(NAME mngl_input_compat_test COMMAND mngl_input_compat_test)
	set_tests_properties(mngl_input_compat_test PROPERTIES
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	if(WIN32 AND MSVC)
		mngl_copy_windows_codec_dlls(mngl_input_compat_test)
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
	add_executable(mngl_event_regression_test tests/event_regression_test.cpp)
	target_link_libraries(mngl_event_regression_test PRIVATE mngl)
	add_test(NAME mngl_event_regression_test COMMAND mngl_event_regression_test)
	set_tests_properties(mngl_event_regression_test PROPERTIES
		TIMEOUT 30
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	add_executable(mngl_line_intersection_test tests/line_intersection_test.cpp)
	target_link_libraries(mngl_line_intersection_test PRIVATE mngl)
	add_test(NAME mngl_line_intersection_test COMMAND mngl_line_intersection_test)
	add_executable(mngl_color_api_test tests/color_api_test.cpp)
	target_link_libraries(mngl_color_api_test PRIVATE mngl)
	add_test(NAME mngl_color_api_test COMMAND mngl_color_api_test)
	add_executable(mngl_clipboard_provider_cache_test tests/clipboard_provider_cache_test.cpp)
	target_link_libraries(mngl_clipboard_provider_cache_test PRIVATE mngl)
	add_test(NAME mngl_clipboard_provider_cache_test COMMAND mngl_clipboard_provider_cache_test)
	add_executable(mngl_gpu_swapchain_lifetime_test tests/gpu_swapchain_lifetime_test.cpp)
	target_link_libraries(mngl_gpu_swapchain_lifetime_test PRIVATE mngl)
	add_test(NAME mngl_gpu_swapchain_lifetime_test COMMAND mngl_gpu_swapchain_lifetime_test)
	set_tests_properties(mngl_gpu_swapchain_lifetime_test PROPERTIES
		TIMEOUT 30
		SKIP_RETURN_CODE 77)
	add_executable(mngl_frame_clock_regression_test tests/frame_clock_regression_test.cpp)
	target_link_libraries(mngl_frame_clock_regression_test PRIVATE mngl)
	add_test(NAME mngl_frame_clock_regression_test COMMAND mngl_frame_clock_regression_test)
	set_tests_properties(mngl_frame_clock_regression_test PROPERTIES
		TIMEOUT 30
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	add_executable(mngl_input_hotplug_window_regression_test tests/input_hotplug_window_regression_test.cpp)
	target_link_libraries(mngl_input_hotplug_window_regression_test PRIVATE mngl)
	add_test(NAME mngl_input_hotplug_window_regression_test COMMAND mngl_input_hotplug_window_regression_test)
	set_tests_properties(mngl_input_hotplug_window_regression_test PROPERTIES
		TIMEOUT 40
		ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
	add_executable(mngl_shader_reflection_regression_test tests/shader_reflection_regression_test.cpp)
	target_link_libraries(mngl_shader_reflection_regression_test PRIVATE mngl)
	add_test(NAME mngl_shader_reflection_regression_test COMMAND mngl_shader_reflection_regression_test)
	set_tests_properties(mngl_shader_reflection_regression_test PROPERTIES TIMEOUT 30)
	add_executable(mngl_two_d_shader_format_selection_test tests/two_d_shader_format_selection_test.cpp)
	target_link_libraries(mngl_two_d_shader_format_selection_test PRIVATE mngl)
	add_test(NAME mngl_two_d_shader_format_selection_test COMMAND mngl_two_d_shader_format_selection_test)
	add_executable(mngl_two_d_vulkan_shader_smoke_test tests/two_d_vulkan_shader_smoke_test.cpp)
	target_link_libraries(mngl_two_d_vulkan_shader_smoke_test PRIVATE mngl)
	mngl_copy_default_2d_shaders(mngl_two_d_vulkan_shader_smoke_test)
	add_test(NAME mngl_two_d_vulkan_shader_smoke_test COMMAND mngl_two_d_vulkan_shader_smoke_test)
	set_tests_properties(mngl_two_d_vulkan_shader_smoke_test PROPERTIES
		TIMEOUT 60
		SKIP_RETURN_CODE 77)
	add_executable(mngl_imgui_offscreen_mouse_routing_test tests/imgui_offscreen_mouse_routing_test.cpp)
	target_link_libraries(mngl_imgui_offscreen_mouse_routing_test PRIVATE mngl)
	add_test(NAME mngl_imgui_offscreen_mouse_routing_test COMMAND mngl_imgui_offscreen_mouse_routing_test)
	set_tests_properties(mngl_imgui_offscreen_mouse_routing_test PROPERTIES TIMEOUT 15)
	add_executable(mngl_imgui_swapchain_format_regression_test tests/imgui_swapchain_format_regression_test.cpp)
	target_link_libraries(mngl_imgui_swapchain_format_regression_test PRIVATE mngl)
	mngl_copy_default_2d_shaders(mngl_imgui_swapchain_format_regression_test)
	add_test(NAME mngl_imgui_swapchain_format_regression_test COMMAND mngl_imgui_swapchain_format_regression_test)
	set_tests_properties(mngl_imgui_swapchain_format_regression_test PROPERTIES
		TIMEOUT 60
		SKIP_RETURN_CODE 77)
	if(WIN32 AND MSVC)
		foreach(MNGL_TEST_TARGET IN ITEMS
				mngl_event_regression_test
				mngl_line_intersection_test
				mngl_color_api_test
				mngl_clipboard_provider_cache_test
				mngl_gpu_swapchain_lifetime_test
				mngl_frame_clock_regression_test
				mngl_input_hotplug_window_regression_test
				mngl_shader_reflection_regression_test
				mngl_two_d_shader_format_selection_test
				mngl_two_d_vulkan_shader_smoke_test
				mngl_imgui_offscreen_mouse_routing_test
				mngl_imgui_swapchain_format_regression_test)
			mngl_copy_windows_runtime_dlls(${MNGL_TEST_TARGET})
		endforeach()
		add_executable(mngl_sdl_image_codec_deployment_test tests/sdl_image_codec_deployment_test.cpp)
		target_link_libraries(mngl_sdl_image_codec_deployment_test PRIVATE mngl)
		add_test(NAME mngl_sdl_image_codec_deployment_test COMMAND mngl_sdl_image_codec_deployment_test)
		set_tests_properties(mngl_sdl_image_codec_deployment_test PROPERTIES TIMEOUT 20)
		mngl_copy_windows_runtime_dlls(mngl_sdl_image_codec_deployment_test)
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

if(MNGL_BUILD_DEMO)
	add_custom_command(TARGET mngl_demo POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"$<TARGET_FILE_DIR:mngl_demo>/imgui/misc/fonts"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/misc/fonts/Roboto-Medium.ttf"
			"$<TARGET_FILE_DIR:mngl_demo>/imgui/misc/fonts/Roboto-Medium.ttf"
		VERBATIM
	)
	mngl_stage_runtime_shaders(mngl_demo)
	
	if(WIN32 AND NOT MNGL_USE_SYSTEM_DEPENDENCIES)
		mngl_copy_windows_codec_dlls(mngl_demo)
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
	if(MNGL_USE_SYSTEM_DEPENDENCIES)
		set(MNGL_CONFIG_FIND_DEPENDENCIES [=[include(CMakeFindDependencyMacro)
find_dependency(SDL3 3.4.18 CONFIG REQUIRED)
find_dependency(SDL3_image 3.4.8 CONFIG REQUIRED)
find_dependency(Freetype REQUIRED)
find_dependency(glm 1.0.3 CONFIG REQUIRED)]=])
		if(MNGL_HAS_SDL3_MIXER)
			string(APPEND MNGL_CONFIG_FIND_DEPENDENCIES "\nfind_dependency(SDL3_mixer 3.2.4 CONFIG REQUIRED)")
		endif()
	endif()
	if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE)
		if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
			message(FATAL_ERROR "MNGL_BUILD_WINDOWS_DEVEL_PACKAGE requires 64-bit MSVC on Windows")
		endif()
		set(MNGL_DEVEL_PACKAGE_NAME "mngl-devel-windows-x64")
		set(MNGL_DEVEL_PACKAGE_GENERATOR "ZIP")
		if(MNGL_HAS_SDL3_MIXER AND NOT MNGL_USE_SYSTEM_DEPENDENCIES)
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
		if(NOT MNGL_USE_SYSTEM_DEPENDENCIES)
			set(MNGL_CONFIG_FIND_DEPENDENCIES [=[include(CMakeFindDependencyMacro)
find_dependency(SDL3 CONFIG REQUIRED)
find_dependency(SDL3_image CONFIG REQUIRED)
find_dependency(Freetype REQUIRED)]=])
			if(MNGL_HAS_SDL3_MIXER)
				string(APPEND MNGL_CONFIG_FIND_DEPENDENCIES "\nfind_dependency(SDL3_mixer CONFIG REQUIRED)")
			endif()
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
		if(NOT MNGL_USE_SYSTEM_DEPENDENCIES)
			set(MNGL_CONFIG_FIND_DEPENDENCIES [=[include(CMakeFindDependencyMacro)
find_dependency(SDL3 CONFIG REQUIRED)
find_dependency(SDL3_image CONFIG REQUIRED)
find_dependency(Freetype REQUIRED)]=])
			if(MNGL_HAS_SDL3_MIXER)
				string(APPEND MNGL_CONFIG_FIND_DEPENDENCIES "\nfind_dependency(SDL3_mixer CONFIG REQUIRED)")
			endif()
		endif()
	endif()

	install(TARGETS mngl EXPORT MNGLTargets)
	install(TARGETS mngl_imgui EXPORT MNGLTargets
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
	# Built-in shaders are loaded from application_directory()/shaders at runtime.
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders"
		DESTINATION "${CMAKE_INSTALL_BINDIR}"
		COMPONENT library
	)
	if(MNGL_METAL_SHADER_ASSET_DIR)
		install(DIRECTORY "${MNGL_METAL_SHADER_ASSET_DIR}/"
			DESTINATION "${CMAKE_INSTALL_BINDIR}/shaders"
			COMPONENT library
			FILES_MATCHING PATTERN "*.msl"
		)
	endif()
	install(EXPORT MNGLTargets
		FILE MNGLTargets.cmake
		NAMESPACE MNGL::
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MNGL"
	)

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

	if(NOT MNGL_USE_SYSTEM_DEPENDENCIES)
		install(DIRECTORY
			"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3"
			"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_image"
			"${CMAKE_CURRENT_SOURCE_DIR}/include/glm"
			DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	endif()
	if(NOT MNGL_USE_SYSTEM_DEPENDENCIES AND NOT MNGL_CUSTOM_AUDIO_BACKEND AND NOT MNGL_CUSTOM_AUDIO_INTERFACE)
		install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL_mixer/include/SDL3_mixer"
			DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
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
	if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE AND NOT MNGL_USE_SYSTEM_DEPENDENCIES)
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
	install(FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/LICENSE.txt"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/MNGL"
	)

	configure_package_config_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/MNGLConfig.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/MNGLConfig.cmake"
		INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MNGL"
		PATH_VARS CMAKE_INSTALL_BINDIR
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
