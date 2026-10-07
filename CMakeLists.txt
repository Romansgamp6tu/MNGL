cmake_minimum_required(VERSION 3.24)

project(MNGL LANGUAGES CXX)

add_executable(mngl_demo MNGL/MNGL.cpp)

target_compile_features(mngl_demo PRIVATE cxx_std_23)
target_include_directories(mngl_demo PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
	# utilities.hpp uses MSVC linker pragmas for these bundled libraries.
	target_link_directories(mngl_demo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libs")

	if(WIN32)
		add_custom_command(TARGET mngl_demo POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL3.dll"
				"$<TARGET_FILE_DIR:mngl_demo>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL3_image.dll"
				"$<TARGET_FILE_DIR:mngl_demo>"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${CMAKE_CURRENT_SOURCE_DIR}/libs/freetype.dll"
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
