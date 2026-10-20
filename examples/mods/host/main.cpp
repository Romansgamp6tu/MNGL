#include "tree_api.hpp"

#include <mod_loader.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using mngl_mod_demo::tree_base;
    using mngl_mod_demo::key_type;

    class bst_tree final : public tree_base
    {
    public:
        bool insert_impl(key_type key) override
        {
            return m_keys.insert(key).second;
        }

        [[nodiscard]] std::size_t size() const noexcept override
        {
            return m_keys.size();
        }

    private:
        std::set<key_type> m_keys;
    };

    std::unique_ptr<tree_base> make_bst_tree()
    {
        return std::make_unique<bst_tree>();
    }

    std::filesystem::path executable_directory(const char* executable)
    {
        std::error_code error;
        const auto absolute = std::filesystem::absolute(executable, error);
        if (error)
            throw std::runtime_error("cannot resolve host executable path: " + error.message());
        return absolute.parent_path();
    }

    std::string json_escape(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (const char character : value)
        {
            if (character == '"' || character == '\\')
                result.push_back('\\');
            result.push_back(character);
        }
        return result;
    }

    std::filesystem::path find_example_binary(const std::filesystem::path& package)
    {
        const auto binary_directory = package / "bin";
        if (!std::filesystem::is_directory(binary_directory))
            return {};

        std::vector<std::filesystem::path> candidates;
        for (const auto& entry : std::filesystem::directory_iterator(binary_directory))
        {
            if (!entry.is_regular_file())
                continue;
            const auto extension = entry.path().extension();
#if defined(_WIN32)
            if (extension == ".dll")
#elif defined(__APPLE__)
            if (extension == ".dylib" || extension == ".so")
#else
            if (extension == ".so")
#endif
                candidates.push_back(entry.path());
        }
        std::sort(candidates.begin(), candidates.end());
        if (candidates.empty())
            return {};
        if (candidates.size() != 1)
            throw std::runtime_error("expected exactly one example mod binary in " +
                binary_directory.string());
        return candidates.front();
    }

    bool write_example_manifest(const std::filesystem::path& package)
    {
        const auto binary = find_example_binary(package);
        if (binary.empty())
            return false;
        const auto relative_binary = std::filesystem::path("bin") / binary.filename();
        const auto manifest_path = package / "mod.json";
        std::ofstream manifest(manifest_path, std::ios::binary | std::ios::trunc);
        if (!manifest)
            throw std::runtime_error("cannot create example mod manifest: " + manifest_path.string());

        manifest << "{\n"
            << "  \"schema_version\": 1,\n"
            << "  \"id\": \"example.tree-tools\",\n"
            << "  \"name\": \"MNGL tree tools example\",\n"
            << "  \"version\": \"1.0.0\",\n"
            << "  \"api_version\": \"1.0\",\n"
            << "  \"binary_abi\": \""
            << json_escape(mngl::mods::native_binary_abi_tag()) << "\",\n"
            << "  \"binary\": \""
            << json_escape(relative_binary.generic_string()) << "\"\n"
            << "}\n";
        if (!manifest)
            throw std::runtime_error("failed while writing example mod manifest: " +
                manifest_path.string());
        return true;
    }

    void show_tree_behavior(mngl::mods::registry& registry, std::string_view phase)
    {
        auto& insert = registry.function<mngl_mod_demo::insert_signature>(
            mngl_mod_demo::insert_extension_id);
        auto& trees = registry.classes<tree_base>(mngl_mod_demo::tree_class_id);
        std::cout << "\n[" << phase << "] Tree label: "
            << registry.property<std::string>(mngl_mod_demo::tree_label_property_id).get()
            << '\n';
        auto tree = trees.create(mngl_mod_demo::bst_implementation_id);
        for (const key_type key : { -5, 5 })
            std::cout << "BST insert(" << key << ") = " << std::boolalpha
                << insert(*tree, key) << '\n';
        std::cout << "BST unique keys: " << tree->size() << '\n';
    }

    void print_usage(const char* executable)
    {
        std::cout << "Usage: " << executable << " [mods-directory]\n"
            << "       " << executable << " --print-abi\n";
    }
}

REGISTER_MODDABLE_CLASS(mngl_mod_demo::tree_base, "demo.tree");
REGISTER_MODDABLE_FUNCTION(mngl_mod_demo::tree_base, "demo.tree.insert",
    &mngl_mod_demo::tree_base::insert_impl);
REGISTER_MODDABLE_VALUE_PROPERTY(mngl_mod_demo::tree_base, std::string,
    "demo.tree.label", mngl::mods::property_scope::global,
    std::string("Built-in BST"));
REGISTER_MODDABLE_FACTORY(mngl_mod_demo::tree_base, "demo.tree", "demo.tree.bst",
    make_bst_tree);

int main(int argc, char** argv)
{
    try
    {
        if (argc > 1 && std::string_view(argv[1]) == "--print-abi")
        {
            std::cout << mngl::mods::native_binary_abi_tag() << '\n';
            return 0;
        }
        if (argc > 2)
        {
            print_usage(argv[0]);
            return 2;
        }

        const auto host_directory = executable_directory(argv[0]);
        const bool use_built_in_example = argc == 1;
        const auto mods_directory = use_built_in_example
            ? host_directory / "mods"
            : std::filesystem::path(argv[1]);
        if (use_built_in_example)
        {
            std::filesystem::create_directories(mods_directory);
            if (!write_example_manifest(mods_directory / "example.tree-tools"))
                std::cout << "Example module is not built yet; build target "
                    << "mngl_example_tree_mod to stage it beside this host.\n";
        }

        mngl::mods::registry registry;
        registry.apply_declarations();
        mngl::mods::mod_loader loader(registry);

        show_tree_behavior(registry, "before loading mods");

        bool example_loaded = false;
        const auto load_results = loader.load_all(mods_directory);
        if (load_results.empty())
            std::cout << "No mod packages found in " << mods_directory.string() << '\n';
        for (const auto& result : load_results)
        {
            if (result)
            {
                std::cout << "Loaded " << result.mod_id << ": " << result.message << '\n';
                example_loaded = example_loaded || result.mod_id == "example.tree-tools";
            }
            else
            {
                std::cerr << "Rejected "
                    << (result.mod_id.empty() ? "<unknown>" : result.mod_id)
                    << ": " << result.message << '\n';
            }
        }

        if (example_loaded)
        {
            show_tree_behavior(registry, "after loading example.tree-tools");

            {
                auto& insert = registry.function<mngl_mod_demo::insert_signature>(
                    mngl_mod_demo::insert_extension_id);
                auto& trees = registry.classes<tree_base>(mngl_mod_demo::tree_class_id);
                auto tree = trees.create(mngl_mod_demo::counting_implementation_id);
                (void)insert(*tree, 4);
                (void)insert(*tree, 4);
                std::cout << "Mod-provided counting tree unique keys: "
                    << tree->size() << '\n';
            }

            const auto unloaded = loader.unload("example.tree-tools");
            std::cout << "Unload example.tree-tools: " << unloaded.message << '\n';
            const auto collected = loader.collect_deferred();
            if (collected != 0)
                std::cout << "Collected " << collected << " deferred module(s).\n";

            show_tree_behavior(registry, "after unloading example.tree-tools");
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "mngl_mod_host: " << error.what() << '\n';
        return 1;
    }
}
