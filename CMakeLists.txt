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
target_include_directories(mngl_demo PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/MNGL"
	"${CMAKE_CURRENT_SOURCE_DIR}/MNGL/imgui"
)

if(MSVC)
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
