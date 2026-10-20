function(mngl_add_mod_targets mngl_source_root mngl_binary_root)
	if(MNGL_BUILD_MOD_EXAMPLE)
		add_library(mngl_example_tree_mod MODULE
			"${mngl_source_root}/examples/mods/fast_tree_mod.cpp"
		)
		target_compile_features(mngl_example_tree_mod PRIVATE cxx_std_23)
		target_include_directories(mngl_example_tree_mod PRIVATE
			"${mngl_source_root}/MNGL"
			"${mngl_source_root}/examples/mods/shared"
			"${mngl_source_root}/include"
		)
		set_target_properties(mngl_example_tree_mod PROPERTIES
			PREFIX ""
			OUTPUT_NAME "example_tree_tools"
			LIBRARY_OUTPUT_DIRECTORY "${mngl_binary_root}/mod-demo/$<CONFIG>/module-build"
			RUNTIME_OUTPUT_DIRECTORY "${mngl_binary_root}/mod-demo/$<CONFIG>/module-build"
		)
		add_custom_command(TARGET mngl_example_tree_mod POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E make_directory
				"${mngl_binary_root}/mod-demo/$<CONFIG>/mods/example.tree-tools/bin"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"$<TARGET_FILE:mngl_example_tree_mod>"
				"${mngl_binary_root}/mod-demo/$<CONFIG>/mods/example.tree-tools/bin/$<TARGET_FILE_NAME:mngl_example_tree_mod>"
			VERBATIM
		)

		add_executable(mngl_mod_host "${mngl_source_root}/examples/mods/host/main.cpp")
		target_compile_features(mngl_mod_host PRIVATE cxx_std_23)
		target_include_directories(mngl_mod_host PRIVATE
			"${mngl_source_root}/MNGL"
			"${mngl_source_root}/examples/mods/shared"
			"${mngl_source_root}/include"
		)
		set_target_properties(mngl_mod_host PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${mngl_binary_root}/mod-demo/$<CONFIG>"
		)
		if(MSVC)
			if(CMAKE_SIZEOF_VOID_P EQUAL 8)
				set(mngl_sdl3_library_directory "${mngl_source_root}/libs")
			else()
				set(mngl_sdl3_library_directory "${mngl_source_root}/libs/x86")
			endif()
			target_link_directories(mngl_mod_host PRIVATE "${mngl_sdl3_library_directory}")
			target_link_libraries(mngl_mod_host PRIVATE SDL3.lib)
			add_custom_command(TARGET mngl_mod_host POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"${mngl_sdl3_library_directory}/SDL3.dll"
					"$<TARGET_FILE_DIR:mngl_mod_host>"
				VERBATIM
			)
		else()
			target_link_libraries(mngl_mod_host PRIVATE SDL3::SDL3)
		endif()
	endif()

	if(BUILD_TESTING AND MNGL_BUILD_MOD_EXAMPLE)
		add_library(mngl_test_dependency_mod MODULE
			"${mngl_source_root}/tests/mods/dependency_probe_mod.cpp"
		)
		target_compile_features(mngl_test_dependency_mod PRIVATE cxx_std_23)
		target_include_directories(mngl_test_dependency_mod PRIVATE
			"${mngl_source_root}/MNGL"
			"${mngl_source_root}/include"
		)
		set_target_properties(mngl_test_dependency_mod PROPERTIES
			PREFIX ""
			OUTPUT_NAME "mngl_test_dependency_mod"
		)

		add_library(mngl_test_conflict_mod MODULE
			"${mngl_source_root}/tests/mods/conflict_mod.cpp"
		)
		target_compile_features(mngl_test_conflict_mod PRIVATE cxx_std_23)
		target_include_directories(mngl_test_conflict_mod PRIVATE
			"${mngl_source_root}/MNGL"
			"${mngl_source_root}/examples/mods/shared"
			"${mngl_source_root}/include"
		)
		set_target_properties(mngl_test_conflict_mod PROPERTIES
			PREFIX ""
			OUTPUT_NAME "mngl_test_conflict_mod"
		)

		add_executable(mngl_mod_integration_test
			"${mngl_source_root}/tests/mods/integration.cpp"
		)
		target_compile_features(mngl_mod_integration_test PRIVATE cxx_std_23)
		target_include_directories(mngl_mod_integration_test PRIVATE
			"${mngl_source_root}/MNGL"
			"${mngl_source_root}/examples/mods/shared"
			"${mngl_source_root}/include"
		)
		add_dependencies(mngl_mod_integration_test
			mngl_example_tree_mod mngl_test_conflict_mod mngl_test_dependency_mod)
		if(MSVC)
			target_link_directories(mngl_mod_integration_test PRIVATE "${mngl_sdl3_library_directory}")
			target_link_libraries(mngl_mod_integration_test PRIVATE SDL3.lib)
			add_custom_command(TARGET mngl_mod_integration_test POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"${mngl_sdl3_library_directory}/SDL3.dll"
					"$<TARGET_FILE_DIR:mngl_mod_integration_test>"
				VERBATIM
			)
		else()
			target_link_libraries(mngl_mod_integration_test PRIVATE SDL3::SDL3)
		endif()
		add_test(NAME mngl_mod_integration
			COMMAND "$<TARGET_FILE:mngl_mod_integration_test>"
				"$<TARGET_FILE:mngl_example_tree_mod>"
				"$<TARGET_FILE:mngl_test_conflict_mod>"
				"$<TARGET_FILE:mngl_test_dependency_mod>"
		)
		set_tests_properties(mngl_mod_integration PROPERTIES TIMEOUT 30)
	endif()
endfunction()
