cmake_minimum_required(VERSION 3.24)

project(MNGL LANGUAGES CXX)

include(CTest)
find_package(OpenMP QUIET COMPONENTS CXX)

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
	target_compile_options(mngl_demo PRIVATE /bigobj)
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
if(OpenMP_CXX_FOUND)
	target_link_libraries(mngl_demo PRIVATE OpenMP::OpenMP_CXX)
endif()

option(MNGL_BUILD_MOD_EXAMPLE "Build the native mod host and example module" ON)
include(cmake/MNGLMods.cmake)
mngl_add_mod_targets("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

option(MNGL_BUILD_TREAP_TREE_MOD
	"Build and stage the Treap tree-algorithm mod for mngl_demo" ON)
if(MNGL_BUILD_TREAP_TREE_MOD)
	add_library(mngl_treap_tree_mod MODULE
		examples/mods/treap_tree_mod.cpp
	)
	target_compile_features(mngl_treap_tree_mod PRIVATE cxx_std_23)
	target_include_directories(mngl_treap_tree_mod PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_treap_tree_mod PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_treap_tree_mod PROPERTIES
		PREFIX ""
		OUTPUT_NAME "mngl_treap_tree_mod"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-treap-module/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-treap-module/$<CONFIG>"
	)

	add_executable(mngl_tree_mod_manifest_writer
		examples/mods/write_treap_manifest.cpp
	)
	target_compile_features(mngl_tree_mod_manifest_writer PRIVATE cxx_std_23)
	target_include_directories(mngl_tree_mod_manifest_writer PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_tree_mod_manifest_writer PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_tree_mod_manifest_writer PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-mod-tools/$<CONFIG>"
	)
	add_custom_target(mngl_treap_tree_mod_package
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.treap/bin"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/tree-treap-module/$<CONFIG>/mngl_treap_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.treap/bin/mngl_treap_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/treap_tree_commands.json"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.treap/commands.json"
		COMMAND "${CMAKE_CURRENT_BINARY_DIR}/tree-mod-tools/$<CONFIG>/mngl_tree_mod_manifest_writer${CMAKE_EXECUTABLE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.treap"
			"mngl_treap_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		DEPENDS mngl_treap_tree_mod mngl_tree_mod_manifest_writer
		VERBATIM
	)
	add_dependencies(mngl_demo mngl_treap_tree_mod_package)

	if(BUILD_TESTING)
		add_executable(mngl_treap_tree_mod_integration
			tests/mods/treap_tree_integration.cpp
		)
		target_compile_features(mngl_treap_tree_mod_integration PRIVATE cxx_std_23)
		target_include_directories(mngl_treap_tree_mod_integration PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}"
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
			"${CMAKE_CURRENT_SOURCE_DIR}/include"
		)
		if(MSVC)
			target_link_directories(mngl_treap_tree_mod_integration PRIVATE
				"${MNGL_LIBRARY_DIRECTORY}")
			target_link_libraries(mngl_treap_tree_mod_integration PRIVATE SDL3.lib)
		else()
			target_link_libraries(mngl_treap_tree_mod_integration PRIVATE SDL3::SDL3)
		endif()
		add_dependencies(mngl_treap_tree_mod_integration mngl_demo)
		add_test(NAME mngl_treap_tree_mod_integration
			COMMAND "$<TARGET_FILE:mngl_treap_tree_mod_integration>"
				"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.treap")
		set_tests_properties(mngl_treap_tree_mod_integration PROPERTIES TIMEOUT 30)
	endif()
endif()

option(MNGL_BUILD_SPLAY_TREE_MOD
	"Build and stage the Splay tree-algorithm mod for mngl_demo" ON)
if(MNGL_BUILD_SPLAY_TREE_MOD)
	add_library(mngl_splay_tree_mod MODULE
		examples/mods/splay_tree_mod.cpp
	)
	target_compile_features(mngl_splay_tree_mod PRIVATE cxx_std_23)
	target_include_directories(mngl_splay_tree_mod PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_splay_tree_mod PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_splay_tree_mod PROPERTIES
		PREFIX ""
		OUTPUT_NAME "mngl_splay_tree_mod"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-splay-module/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-splay-module/$<CONFIG>"
	)

	add_executable(mngl_splay_mod_manifest_writer
		examples/mods/write_splay_manifest.cpp
	)
	target_compile_features(mngl_splay_mod_manifest_writer PRIVATE cxx_std_23)
	target_include_directories(mngl_splay_mod_manifest_writer PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_splay_mod_manifest_writer PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_splay_mod_manifest_writer PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/splay-mod-tools/$<CONFIG>"
	)
	add_custom_target(mngl_splay_tree_mod_package
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.splay/bin"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/tree-splay-module/$<CONFIG>/mngl_splay_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.splay/bin/mngl_splay_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/splay_tree_commands.json"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.splay/commands.json"
		COMMAND "${CMAKE_CURRENT_BINARY_DIR}/splay-mod-tools/$<CONFIG>/mngl_splay_mod_manifest_writer${CMAKE_EXECUTABLE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.splay"
			"mngl_splay_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		DEPENDS mngl_splay_tree_mod mngl_splay_mod_manifest_writer
		VERBATIM
	)
	add_dependencies(mngl_demo mngl_splay_tree_mod_package)

	if(BUILD_TESTING)
		add_executable(mngl_splay_tree_mod_integration
			tests/mods/splay_tree_integration.cpp
		)
		target_compile_features(mngl_splay_tree_mod_integration PRIVATE cxx_std_23)
		target_include_directories(mngl_splay_tree_mod_integration PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}"
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
			"${CMAKE_CURRENT_SOURCE_DIR}/include"
		)
		if(MSVC)
			target_link_directories(mngl_splay_tree_mod_integration PRIVATE
				"${MNGL_LIBRARY_DIRECTORY}")
			target_link_libraries(mngl_splay_tree_mod_integration PRIVATE SDL3.lib)
		else()
			target_link_libraries(mngl_splay_tree_mod_integration PRIVATE SDL3::SDL3)
		endif()
		add_dependencies(mngl_splay_tree_mod_integration mngl_demo)
		add_test(NAME mngl_splay_tree_mod_integration
			COMMAND "$<TARGET_FILE:mngl_splay_tree_mod_integration>"
				"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.splay")
		set_tests_properties(mngl_splay_tree_mod_integration PROPERTIES TIMEOUT 30)
	endif()
endif()

option(MNGL_BUILD_AVL_TREE_MOD
	"Build and stage the AVL tree-algorithm mod for mngl_demo" ON)
if(MNGL_BUILD_AVL_TREE_MOD)
	add_library(mngl_avl_tree_mod MODULE
		examples/mods/avl_tree_mod.cpp
	)
	target_compile_features(mngl_avl_tree_mod PRIVATE cxx_std_23)
	target_include_directories(mngl_avl_tree_mod PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_avl_tree_mod PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_avl_tree_mod PROPERTIES
		PREFIX ""
		OUTPUT_NAME "mngl_avl_tree_mod"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-avl-module/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-avl-module/$<CONFIG>"
	)

	add_executable(mngl_avl_mod_manifest_writer
		examples/mods/write_avl_manifest.cpp
	)
	target_compile_features(mngl_avl_mod_manifest_writer PRIVATE cxx_std_23)
	target_include_directories(mngl_avl_mod_manifest_writer PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_avl_mod_manifest_writer PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_avl_mod_manifest_writer PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/avl-mod-tools/$<CONFIG>"
	)
	add_custom_target(mngl_avl_tree_mod_package
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.avl/bin"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/tree-avl-module/$<CONFIG>/mngl_avl_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.avl/bin/mngl_avl_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/avl_tree_commands.json"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.avl/commands.json"
		COMMAND "${CMAKE_CURRENT_BINARY_DIR}/avl-mod-tools/$<CONFIG>/mngl_avl_mod_manifest_writer${CMAKE_EXECUTABLE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.avl"
			"mngl_avl_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		DEPENDS mngl_avl_tree_mod mngl_avl_mod_manifest_writer
		VERBATIM
	)
	add_dependencies(mngl_demo mngl_avl_tree_mod_package)
endif()

option(MNGL_BUILD_RED_BLACK_TREE_MOD
	"Build and stage the red-black tree-algorithm mod for mngl_demo" ON)
if(MNGL_BUILD_RED_BLACK_TREE_MOD)
	add_library(mngl_red_black_tree_mod MODULE
		examples/mods/red_black_tree_mod.cpp
	)
	target_compile_features(mngl_red_black_tree_mod PRIVATE cxx_std_23)
	target_include_directories(mngl_red_black_tree_mod PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_red_black_tree_mod PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_red_black_tree_mod PROPERTIES
		PREFIX ""
		OUTPUT_NAME "mngl_red_black_tree_mod"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-red-black-module/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-red-black-module/$<CONFIG>"
	)

	add_executable(mngl_red_black_mod_manifest_writer
		examples/mods/write_red_black_manifest.cpp
	)
	target_compile_features(mngl_red_black_mod_manifest_writer PRIVATE cxx_std_23)
	target_include_directories(mngl_red_black_mod_manifest_writer PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_red_black_mod_manifest_writer PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_red_black_mod_manifest_writer PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/red-black-mod-tools/$<CONFIG>"
	)
	add_custom_target(mngl_red_black_tree_mod_package
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.red-black/bin"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/tree-red-black-module/$<CONFIG>/mngl_red_black_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.red-black/bin/mngl_red_black_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/red_black_tree_commands.json"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.red-black/commands.json"
		COMMAND "${CMAKE_CURRENT_BINARY_DIR}/red-black-mod-tools/$<CONFIG>/mngl_red_black_mod_manifest_writer${CMAKE_EXECUTABLE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.red-black"
			"mngl_red_black_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		DEPENDS mngl_red_black_tree_mod mngl_red_black_mod_manifest_writer
		VERBATIM
	)
	add_dependencies(mngl_demo mngl_red_black_tree_mod_package)
endif()

option(MNGL_BUILD_B_PLUS_TREE_MOD
	"Build and stage the B+ tree-algorithm mod for mngl_demo" ON)
if(MNGL_BUILD_B_PLUS_TREE_MOD)
	add_library(mngl_b_plus_tree_mod MODULE
		examples/mods/b_plus_tree_mod.cpp
	)
	target_compile_features(mngl_b_plus_tree_mod PRIVATE cxx_std_23)
	target_include_directories(mngl_b_plus_tree_mod PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_b_plus_tree_mod PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_b_plus_tree_mod PROPERTIES
		PREFIX ""
		OUTPUT_NAME "mngl_b_plus_tree_mod"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-b-plus-module/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-b-plus-module/$<CONFIG>"
	)

	add_executable(mngl_b_plus_mod_manifest_writer
		examples/mods/write_b_plus_manifest.cpp
	)
	target_compile_features(mngl_b_plus_mod_manifest_writer PRIVATE cxx_std_23)
	target_include_directories(mngl_b_plus_mod_manifest_writer PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_b_plus_mod_manifest_writer PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_b_plus_mod_manifest_writer PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/b-plus-mod-tools/$<CONFIG>"
	)
	add_custom_target(mngl_b_plus_tree_mod_package
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.b-plus/bin"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/tree-b-plus-module/$<CONFIG>/mngl_b_plus_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.b-plus/bin/mngl_b_plus_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/b_plus_tree_commands.json"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.b-plus/commands.json"
		COMMAND "${CMAKE_CURRENT_BINARY_DIR}/b-plus-mod-tools/$<CONFIG>/mngl_b_plus_mod_manifest_writer${CMAKE_EXECUTABLE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.b-plus"
			"mngl_b_plus_tree_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		DEPENDS mngl_b_plus_tree_mod mngl_b_plus_mod_manifest_writer
		VERBATIM
	)
	add_dependencies(mngl_demo mngl_b_plus_tree_mod_package)
endif()

option(MNGL_BUILD_BITWISE_TRIE_MOD
	"Build and stage the Bitwise Trie tree-algorithm mod for mngl_demo" ON)
if(MNGL_BUILD_BITWISE_TRIE_MOD)
	add_library(mngl_bitwise_trie_mod MODULE
		examples/mods/bitwise_trie_mod.cpp
	)
	target_compile_features(mngl_bitwise_trie_mod PRIVATE cxx_std_23)
	target_include_directories(mngl_bitwise_trie_mod PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_bitwise_trie_mod PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_bitwise_trie_mod PROPERTIES
		PREFIX ""
		OUTPUT_NAME "mngl_bitwise_trie_mod"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-bitwise-trie-module/$<CONFIG>"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tree-bitwise-trie-module/$<CONFIG>"
	)

	add_executable(mngl_bitwise_trie_mod_manifest_writer
		examples/mods/write_bitwise_trie_manifest.cpp
	)
	target_compile_features(mngl_bitwise_trie_mod_manifest_writer PRIVATE cxx_std_23)
	target_include_directories(mngl_bitwise_trie_mod_manifest_writer PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	if(MSVC)
		target_compile_options(mngl_bitwise_trie_mod_manifest_writer PRIVATE /utf-8)
	endif()
	set_target_properties(mngl_bitwise_trie_mod_manifest_writer PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bitwise-trie-mod-tools/$<CONFIG>"
	)
	add_custom_target(mngl_bitwise_trie_mod_package
		COMMAND "${CMAKE_COMMAND}" -E make_directory
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.bitwise-trie/bin"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_BINARY_DIR}/tree-bitwise-trie-module/$<CONFIG>/mngl_bitwise_trie_mod${CMAKE_SHARED_MODULE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.bitwise-trie/bin/mngl_bitwise_trie_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/bitwise_trie_commands.json"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.bitwise-trie/commands.json"
		COMMAND "${CMAKE_CURRENT_BINARY_DIR}/bitwise-trie-mod-tools/$<CONFIG>/mngl_bitwise_trie_mod_manifest_writer${CMAKE_EXECUTABLE_SUFFIX}"
			"$<TARGET_FILE_DIR:mngl_demo>/mods/mngl.tree-practicum.bitwise-trie"
			"mngl_bitwise_trie_mod${CMAKE_SHARED_MODULE_SUFFIX}"
		DEPENDS mngl_bitwise_trie_mod mngl_bitwise_trie_mod_manifest_writer
		VERBATIM
	)
	add_dependencies(mngl_demo mngl_bitwise_trie_mod_package)
endif()

add_custom_command(TARGET mngl_demo POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory
		"$<TARGET_FILE_DIR:mngl_demo>/data"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/data/tree_commands.json"
		"$<TARGET_FILE_DIR:mngl_demo>/data/tree_commands.json"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders"
		"$<TARGET_FILE_DIR:mngl_demo>/shaders"
	COMMAND "${CMAKE_COMMAND}" -E make_directory
		"$<TARGET_FILE_DIR:mngl_demo>/imgui/misc/fonts"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/misc/fonts/Roboto-Medium.ttf"
		"$<TARGET_FILE_DIR:mngl_demo>/imgui/misc/fonts/Roboto-Medium.ttf"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/misc/fonts/Cousine-Regular.ttf"
		"$<TARGET_FILE_DIR:mngl_demo>/imgui/misc/fonts/Cousine-Regular.ttf"
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

option(MNGL_BUILD_TESTS "Build tree coordinate regression checks" OFF)
if(MNGL_BUILD_TESTS)
	add_executable(mngl_tree_precision tests/tree_precision.cpp)
	target_compile_features(mngl_tree_precision PRIVATE cxx_std_23)
	target_include_directories(mngl_tree_precision PRIVATE include MNGL)
	target_compile_definitions(mngl_tree_precision PRIVATE
		MNGL_TEST_FONT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui/misc/fonts/Roboto-Medium.ttf")
	target_link_libraries(mngl_tree_precision PRIVATE mngl)
	if(OpenMP_CXX_FOUND)
		target_link_libraries(mngl_tree_precision PRIVATE OpenMP::OpenMP_CXX)
	endif()
	if(MSVC)
		target_compile_options(mngl_tree_precision PRIVATE /bigobj /utf-8)
		add_custom_command(TARGET mngl_tree_precision POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_imgui>" "$<TARGET_FILE_DIR:mngl_tree_precision>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3.dll" "$<TARGET_FILE_DIR:mngl_tree_precision>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/SDL3_image.dll" "$<TARGET_FILE_DIR:mngl_tree_precision>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${MNGL_LIBRARY_DIRECTORY}/freetype.dll" "$<TARGET_FILE_DIR:mngl_tree_precision>"
			VERBATIM)
	endif()
	add_custom_command(TARGET mngl_tree_precision POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/shaders"
			"$<TARGET_FILE_DIR:mngl_tree_precision>/shaders"
		VERBATIM)
	add_test(NAME tree_precision COMMAND mngl_tree_precision)
endif()

if(BUILD_TESTING)
	add_executable(mngl_python_bridge_protocol_test
		tests/python_bridge_protocol.cpp
	)
	target_compile_features(mngl_python_bridge_protocol_test PRIVATE cxx_std_23)
	target_include_directories(mngl_python_bridge_protocol_test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
	)
	if(MSVC)
		target_compile_options(mngl_python_bridge_protocol_test PRIVATE /utf-8)
	endif()
	add_test(NAME mngl_python_bridge_protocol
		COMMAND mngl_python_bridge_protocol_test)

	find_package(Python3 COMPONENTS Interpreter QUIET)
	if(Python3_Interpreter_FOUND)
		add_test(NAME mngl_python_gui_client
			COMMAND "${Python3_EXECUTABLE}" -m unittest discover
				-s "${CMAKE_CURRENT_SOURCE_DIR}/python/tests"
				-v)
		set_tests_properties(mngl_python_gui_client PROPERTIES
			ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python"
			TIMEOUT 20)
	endif()
endif()

option(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE "Create a Windows x64 MNGL development ZIP" OFF)
option(MNGL_BUILD_LINUX_DEVEL_PACKAGE "Create a Linux x64 MNGL development tarball" OFF)
option(MNGL_BUILD_DEVEL_PACKAGE "Create a Linux x64 MNGL Debian development package" OFF)
if((MNGL_BUILD_WINDOWS_DEVEL_PACKAGE AND MNGL_BUILD_LINUX_DEVEL_PACKAGE) OR
	(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE AND MNGL_BUILD_DEVEL_PACKAGE) OR
	(MNGL_BUILD_LINUX_DEVEL_PACKAGE AND MNGL_BUILD_DEVEL_PACKAGE))
	message(FATAL_ERROR "Build only one MNGL development package per CMake build directory")
endif()

if(MNGL_BUILD_WINDOWS_DEVEL_PACKAGE OR MNGL_BUILD_LINUX_DEVEL_PACKAGE OR MNGL_BUILD_DEVEL_PACKAGE)
	include(GNUInstallDirs)
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
	else()
		if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
			message(FATAL_ERROR "MNGL_BUILD_DEVEL_PACKAGE requires 64-bit Linux")
		endif()
		find_package(glm CONFIG REQUIRED)
		target_link_libraries(mngl INTERFACE glm::glm)
		set(MNGL_DEVEL_PACKAGE_NAME "libmngl-dev")
		set(MNGL_DEVEL_PACKAGE_GENERATOR "DEB")
		set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
		set(MNGL_CONFIG_FIND_DEPENDENCIES [=[include(CMakeFindDependencyMacro)
find_dependency(SDL3 CONFIG REQUIRED)
find_dependency(SDL3_image CONFIG REQUIRED)
find_dependency(Freetype REQUIRED)
find_dependency(glm CONFIG REQUIRED)]=])
	endif()
	if(MNGL_BUILD_DEVEL_PACKAGE)
		set(CPACK_PACKAGE_CONTACT "MNGL maintainers")
		set(CPACK_PACKAGE_VENDOR "MNGL")
		set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++23 MNGL development headers and CMake targets")
		set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "C++23 MNGL development headers, CMake targets, and Dear ImGui integration")
		set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
		set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
		set(CPACK_DEBIAN_PACKAGE_SECTION "libdevel")
		set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfreetype-dev, libglm-dev, libsdl3-dev, libsdl3-image-dev")
		set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
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

	if(MNGL_BUILD_DEVEL_PACKAGE)
		install(DIRECTORY
			"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3"
			"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_image"
			DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
		)
	else()
		install(DIRECTORY
			"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3"
			"${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_image"
			"${CMAKE_CURRENT_SOURCE_DIR}/include/glm"
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
	if(NOT MNGL_BUILD_DEVEL_PACKAGE)
		set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
	endif()
	set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
	include(CPack)
endif()

