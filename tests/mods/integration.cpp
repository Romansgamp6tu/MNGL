#include "tree_api.hpp"

#include <mod_loader.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using mngl_mod_demo::key_type;
	using mngl_mod_demo::tree_base;

	class bst_tree final : public tree_base
	{
	public:
		bool insert_impl(key_type key) override { return m_keys.insert(key).second; }

		[[nodiscard]] std::size_t size() const noexcept override { return m_keys.size(); }

	private:
		std::set<key_type> m_keys;
	};

	std::unique_ptr<tree_base> make_bst_tree()
	{
		return std::make_unique<bst_tree>();
	}

	void require(bool condition, std::string_view message)
	{
		if (!condition)
			throw std::runtime_error(std::string(message));
	}

	class temporary_directory
	{
	public:
		temporary_directory()
		{
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			m_path =
			    std::filesystem::temp_directory_path() / ("mngl-mod-integration-" + std::to_string(stamp));
			std::filesystem::create_directories(m_path);
		}

		~temporary_directory()
		{
			std::error_code error;
			std::filesystem::remove_all(m_path, error);
		}

		[[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

	private:
		std::filesystem::path m_path;
	};

	std::filesystem::path write_package(const std::filesystem::path& root, std::string_view directory,
	                                    std::string_view id, const std::filesystem::path& module,
	                                    std::string_view abi, std::string_view version = "1.0.0",
	                                    std::string_view dependencies = "{}")
	{
		const auto package = root / directory;
		const auto binary_directory = package / "bin";
		std::filesystem::create_directories(binary_directory);
		const auto binary = binary_directory / module.filename();
		std::filesystem::copy_file(module, binary, std::filesystem::copy_options::overwrite_existing);

		std::ofstream manifest(package / "mod.json", std::ios::binary | std::ios::trunc);
		require(static_cast<bool>(manifest), "could not create test package manifest");
		manifest << "{\n"
		         << "  \"schema_version\": 1,\n"
		         << "  \"id\": \"" << id << "\",\n"
		         << "  \"name\": \"MNGL integration test mod\",\n"
		         << "  \"version\": \"" << version << "\",\n"
		         << "  \"api_version\": \"1.0\",\n"
		         << "  \"binary_abi\": \"" << abi << "\",\n"
		         << "  \"binary\": \"bin/" << module.filename().string() << "\",\n"
		         << "  \"dependencies\": " << dependencies << "\n"
		         << "}\n";
		require(static_cast<bool>(manifest), "could not write test package manifest");
		return package;
	}

	void verify_builtin_function(mngl::mods::registry& registry)
	{
		auto& insert = registry.function<mngl_mod_demo::insert_signature>(mngl_mod_demo::insert_extension_id);
		auto tree = registry.classes<tree_base>(mngl_mod_demo::tree_class_id)
		                .create(mngl_mod_demo::bst_implementation_id);
		require(insert(*tree, -5), "built-in insertion of -5 failed");
		require(insert(*tree, 5), "built-in insertion of 5 should remain distinct");
		require(tree->size() == 2, "built-in behavior did not restore two distinct keys");
	}

	void verify_builtin_behavior(mngl::mods::registry& registry)
	{
		verify_builtin_function(registry);
		require(registry.property<std::string>(mngl_mod_demo::tree_label_property_id).get() == "Built-in BST",
		        "host property did not return to its default value");
		require(registry.property<float>(mngl_mod_demo::tree_node_gap_property_id).get() == 24.0f,
		        "host numeric property did not return to its default value");
	}
} // namespace

REGISTER_MODDABLE_CLASS(tree_base, "demo.tree");
REGISTER_MODDABLE_FUNCTION(tree_base, "demo.tree.insert", &tree_base::insert_impl);
REGISTER_MODDABLE_VALUE_PROPERTY(tree_base, std::string, "demo.tree.label",
                                 mngl::mods::property_scope::global, std::string("Built-in BST"));
REGISTER_MODDABLE_VALUE_PROPERTY(tree_base, float, "demo.tree.node-gap", mngl::mods::property_scope::global,
                                 24.0f);
REGISTER_MODDABLE_FACTORY(tree_base, "demo.tree", "demo.tree.bst", make_bst_tree);

int main(int argc, char** argv)
{
	try
	{
		require(argc == 4, "expected paths to the working, conflicting, and dependency probe modules");
		const std::filesystem::path example_module = argv[1];
		const std::filesystem::path conflict_module = argv[2];
		const std::filesystem::path dependency_module = argv[3];
		temporary_directory temp;
		mngl::mods::registry registry;
		registry.apply_declarations();
		mngl::mods::mod_loader loader(registry);

		verify_builtin_behavior(registry);

		const auto malformed_package = temp.path() / "malformed";
		std::filesystem::create_directories(malformed_package);
		{
			std::ofstream malformed(malformed_package / "mod.json", std::ios::binary);
			malformed << "{ this is not valid JSON";
		}
		const auto malformed_result = loader.load_package(malformed_package);
		require(!malformed_result, "malformed manifest was accepted");
		require(!malformed_result.message.empty(), "malformed manifest error was empty");
		std::cout << "PASS: malformed manifest rejected\n";

		const auto wrong_abi_package =
		    write_package(temp.path(), "wrong-abi", "test.wrong-abi", example_module, "wrong-abi-tag");
		const auto wrong_abi_result = loader.load_package(wrong_abi_package);
		require(!wrong_abi_result, "wrong ABI manifest was accepted");
		require(wrong_abi_result.message.find("binary_abi") != std::string::npos,
		        "wrong ABI rejection did not identify binary_abi");
		std::cout << "PASS: incompatible ABI rejected\n";

		{
			mngl::mods::registry dependency_registry;
			dependency_registry.apply_declarations();
			mngl::mods::mod_loader dependency_loader(dependency_registry);
			const auto dependency_root = temp.path() / "dependency-order";
			write_package(dependency_root, "a-consumer", "test.consumer", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "2.0.0", R"({"test.bridge":"=1.2.0"})");
			write_package(dependency_root, "m-bridge", "test.bridge", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.2.0", R"({"test.core":">=1.0.0 <2.0.0"})");
			write_package(dependency_root, "z-core", "test.core", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.4.3");

			const auto dependency_results = dependency_loader.load_all(dependency_root);
			require(dependency_results.size() == 3,
			        "dependency loader did not return one result per package");
			const std::vector<std::string> expected_load_order{"test.core", "test.bridge", "test.consumer"};
			std::vector<std::string> actual_load_order;
			for (const auto& result : dependency_results)
			{
				require(static_cast<bool>(result),
				        "dependency package failed to load: " + result.mod_id + ": " + result.message);
				actual_load_order.push_back(result.mod_id);
			}
			require(actual_load_order == expected_load_order,
			        "load_all did not return dependency-first load order");

			const auto provider_unload = dependency_loader.unload("test.core");
			require(provider_unload.state == mngl::mods::mod_unload_state::dependency_in_use,
			        "a provider was unloaded while a dependent mod was still loaded");
			require(provider_unload.message.find("test.bridge") != std::string::npos,
			        "provider unload error did not name its dependent");

			const auto dependency_unloads = dependency_loader.unload_all();
			require(dependency_unloads.size() == 3,
			        "unload_all did not return one result per loaded dependency mod");
			const std::vector<std::string> expected_unload_order{"test.consumer", "test.bridge", "test.core"};
			for (std::size_t i = 0; i < dependency_unloads.size(); ++i)
			{
				require(dependency_unloads[i].state == mngl::mods::mod_unload_state::unloaded,
				        "dependency-aware unload_all did not unload a mod");
				require(dependency_unloads[i].mod_id == expected_unload_order[i],
				        "unload_all did not return dependent-first unload order");
			}
			std::cout << "PASS: dependencies loaded in order and providers stayed in use\n";
		}

		{
			mngl::mods::registry dependency_registry;
			dependency_registry.apply_declarations();
			mngl::mods::mod_loader dependency_loader(dependency_registry);
			const auto dependency_root = temp.path() / "dependency-errors";
			write_package(dependency_root, "a-missing", "test.needs-missing", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.0.0", R"({"test.not-installed":"*"})");
			write_package(dependency_root, "b-needs-two", "test.needs-two", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.0.0",
			              R"({"test.version-core":">=2.0.0 <3.0.0"})");
			write_package(dependency_root, "z-version-core", "test.version-core", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.4.3");

			const auto dependency_results = dependency_loader.load_all(dependency_root);
			const auto missing =
			    std::find_if(dependency_results.begin(), dependency_results.end(),
				             [](const auto& result) { return result.mod_id == "test.needs-missing"; });
			require(missing != dependency_results.end() && !*missing,
			        "a mod with a missing dependency was not rejected");
			require(missing->message.find("test.not-installed") != std::string::npos,
			        "missing dependency error did not name the required mod");
			const auto incompatible =
			    std::find_if(dependency_results.begin(), dependency_results.end(),
				             [](const auto& result) { return result.mod_id == "test.needs-two"; });
			require(incompatible != dependency_results.end() && !*incompatible,
			        "a mod with an incompatible dependency version was not rejected");
			require(incompatible->message.find("1.4.3") != std::string::npos &&
			            incompatible->message.find(">=2.0.0 <3.0.0") != std::string::npos,
			        "dependency version error did not include actual and required versions");
			require(dependency_loader.is_loaded("test.version-core"),
			        "valid independent dependency package should still load");
			(void)dependency_loader.unload_all();
			std::cout << "PASS: missing and incompatible dependency versions rejected\n";
		}

		{
			mngl::mods::registry dependency_registry;
			dependency_registry.apply_declarations();
			mngl::mods::mod_loader dependency_loader(dependency_registry);
			const auto dependency_root = temp.path() / "dependency-cycle";
			write_package(dependency_root, "a-cycle", "test.cycle-a", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.0.0", R"({"test.cycle-b":"*"})");
			write_package(dependency_root, "b-cycle", "test.cycle-b", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.0.0", R"({"test.cycle-a":"*"})");
			write_package(dependency_root, "c-blocked", "test.cycle-dependent", dependency_module,
			              mngl::mods::native_binary_abi_tag(), "1.0.0", R"({"test.cycle-a":"*"})");

			const auto cycle_results = dependency_loader.load_all(dependency_root);
			require(cycle_results.size() == 3,
			        "cycle detection did not return an error for each blocked package");
			for (const auto& result : cycle_results)
			{
				require(!result, "a package in a dependency cycle was loaded");
				require(result.message.find("test.cycle-a") != std::string::npos &&
				            result.message.find("test.cycle-b") != std::string::npos,
				        "dependency cycle error did not name the cycle members");
				if (result.mod_id == "test.cycle-dependent")
					require(result.message.find("depends on the cycle") != std::string::npos,
					        "mod blocked by a cycle was reported as a cycle member");
				else
					require(result.message.find("dependency cycle detected") != std::string::npos,
					        "dependency cycle error was not explicit");
			}
			std::cout << "PASS: dependency cycle and blocked dependent reported clearly\n";
		}

		{
			const auto settings_package = write_package(temp.path(), "settings-package", "example.tree-tools",
			                                            example_module, mngl::mods::native_binary_abi_tag());
			const auto settings_path = temp.path() / "mod-settings.ini";
			const std::string custom_label = "Configured\tTree\nTools";
			{
				mngl::mods::registry settings_registry;
				settings_registry.apply_declarations();
				mngl::mods::mod_loader settings_loader(settings_registry);
				require(static_cast<bool>(settings_loader.load_package(settings_package)),
				        "settings probe mod did not load");
				require(settings_registry.settings().size() == 2,
				        "host did not enumerate both string and numeric mod settings");
				require(settings_registry.set_setting("example.tree-tools",
				                                      mngl_mod_demo::tree_label_property_id, custom_label),
				        "host could not change the string mod setting");
				require(settings_registry.set_setting("example.tree-tools",
				                                      mngl_mod_demo::tree_node_gap_property_id, "41.25"),
				        "host could not change the floating-point mod setting");
				std::string setting_error;
				require(!settings_registry.set_setting("example.tree-tools",
				                                       mngl_mod_demo::tree_node_gap_property_id,
				                                       "not-a-number", &setting_error),
				        "malformed numeric mod setting was accepted");
				require(settings_registry.property<float>(mngl_mod_demo::tree_node_gap_property_id).get() ==
				            41.25f,
				        "rejected numeric setting changed the current value");
				settings_registry.save_settings(settings_path);
				require(settings_loader.unload("example.tree-tools").state ==
				            mngl::mods::mod_unload_state::unloaded,
				        "settings probe mod did not unload after saving");
			}

			{
				mngl::mods::registry restored_registry;
				restored_registry.apply_declarations();
				mngl::mods::mod_loader restored_loader(restored_registry);
				require(static_cast<bool>(restored_loader.load_package(settings_package)),
				        "settings probe mod did not reload");
				const auto load_issues = restored_registry.load_settings(settings_path);
				require(load_issues.empty(), "valid saved settings produced diagnostics");
				require(
				    restored_registry.property<std::string>(mngl_mod_demo::tree_label_property_id).get() ==
				        custom_label,
				    "string mod setting did not persist across reload");
				require(restored_registry.property<float>(mngl_mod_demo::tree_node_gap_property_id).get() ==
				            41.25f,
				        "numeric mod setting did not persist across reload");
				const auto restored_settings = restored_registry.settings();
				require(restored_settings.size() == 2 && restored_settings[0].mod_id == "example.tree-tools",
				        "host settings interface did not expose restored values");
				(void)restored_loader.unload_all();
			}
			std::cout << "PASS: string and numeric mod settings survive a save and reload\n";
		}

		const auto example_package = write_package(temp.path(), "working", "example.tree-tools",
		                                           example_module, mngl::mods::native_binary_abi_tag());
		const auto loaded = loader.load_package(example_package);
		require(static_cast<bool>(loaded), "valid example module did not load: " + loaded.message);
		require(loader.is_loaded("example.tree-tools"), "loaded module is absent from loader");
		require(registry.property<std::string>(mngl_mod_demo::tree_label_property_id).get() ==
		            "Tree tools mod active",
		        "module property override was not visible");
		{
			auto& insert =
			    registry.function<mngl_mod_demo::insert_signature>(mngl_mod_demo::insert_extension_id);
			auto tree = registry.classes<tree_base>(mngl_mod_demo::tree_class_id)
			                .create(mngl_mod_demo::bst_implementation_id);
			require(insert(*tree, -5), "modded insertion of -5 failed");
			require(!insert(*tree, 5), "modded replacement did not normalize the key");
			require(tree->size() == 1, "modded replacement produced the wrong key count");
		}
		std::cout << "PASS: function replacement and property override applied\n";

		std::unique_ptr<tree_base> live_mod_object;
		live_mod_object = registry.classes<tree_base>(mngl_mod_demo::tree_class_id)
		                      .create(mngl_mod_demo::counting_implementation_id);

		const auto conflict_package = write_package(temp.path(), "conflict", "test.conflict", conflict_module,
		                                            mngl::mods::native_binary_abi_tag());
		const auto conflict_result = loader.load_package(conflict_package);
		require(static_cast<bool>(conflict_result),
		        "conflict probe module did not load: " + conflict_result.message);
		const auto diagnostics = registry.diagnostics();
		const auto conflict_diagnostic =
		    std::find_if(diagnostics.begin(), diagnostics.end(),
			             [](const mngl::mods::diagnostic& item)
			             {
			                 return item.code == mngl::mods::error_code::registration_conflict &&
							        item.extension_id == mngl_mod_demo::insert_extension_id;
		                 });
		require(conflict_diagnostic != diagnostics.end(),
		        "conflicting replacement did not produce a diagnostic");
		verify_builtin_function(registry);
		require(registry.property<std::string>(mngl_mod_demo::tree_label_property_id).get() ==
		            "Tree tools mod active",
		        "conflict unexpectedly removed the first module's property override");
		std::cout << "PASS: conflicting replacement diagnosed and disabled\n";

		const auto conflict_unloaded = loader.unload("test.conflict");
		require(conflict_unloaded.state == mngl::mods::mod_unload_state::unloaded,
		        "conflict probe module did not unload");

		const auto deferred = loader.unload("example.tree-tools");
		require(deferred.state == mngl::mods::mod_unload_state::deferred,
		        "module unload was not deferred while a mod object lived");
		require(deferred.live_objects == 1, "deferred unload did not report the live mod-created object");
		require(loader.collect_deferred() == 0, "module unloaded while a mod-created object was still alive");
		verify_builtin_behavior(registry);
		require(!registry.diagnostics().empty(), "conflict diagnostic disappeared unexpectedly");
		std::cout << "PASS: unload deferred while mod-created object lived; host fallback restored\n";

		live_mod_object.reset();
		require(loader.collect_deferred() == 1,
		        "module did not unload after its final mod-created object was destroyed");
		require(!loader.is_loaded("example.tree-tools"), "module remained loaded after collection");
		verify_builtin_behavior(registry);
		std::cout << "PASS: deferred module unload completed and host behavior restored\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
