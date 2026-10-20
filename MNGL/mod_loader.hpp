#pragma once

#include "mods.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_loadso.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#ifndef MNGL_MOD_EXPORT
#if defined(_WIN32)
#define MNGL_MOD_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define MNGL_MOD_EXPORT __attribute__((visibility("default")))
#else
#define MNGL_MOD_EXPORT
#endif
#endif

#ifndef MNGL_MOD_CALL
#define MNGL_MOD_CALL SDLCALL
#endif

namespace mngl::mods
{
    struct api_version
    {
        std::uint32_t major{ 1 };
        std::uint32_t minor{};
        friend bool operator==(const api_version&, const api_version&) = default;
    };

    struct mod_dependency
    {
        std::string id;
        std::string version_range;
    };

    struct mod_manifest
    {
        std::uint32_t schema_version{};
        std::string id;
        std::string name;
        std::string version;
        std::vector<mod_dependency> dependencies;
        api_version required_api;
        std::string binary_abi;
        std::filesystem::path binary_path;
    };

    [[nodiscard]] inline std::string native_binary_abi_tag();

    struct mod_loader_options
    {
        api_version host_api{ 1, 0 };
        std::string binary_abi{ native_binary_abi_tag() };
    };

    enum class mod_load_state
    {
        loaded,
        rejected
    };

    struct mod_load_result
    {
        mod_load_state state{ mod_load_state::rejected };
        std::string mod_id;
        std::string message;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return state == mod_load_state::loaded;
        }
    };

    enum class mod_unload_state
    {
        unloaded,
        deferred,
        dependency_in_use,
        not_loaded
    };

    struct mod_unload_result
    {
        mod_unload_state state{ mod_unload_state::not_loaded };
        std::string mod_id;
        std::size_t active_calls{};
        std::size_t live_objects{};
        std::string message;
    };

    [[nodiscard]] inline std::string native_binary_abi_tag()
    {
#if defined(_WIN32)
        constexpr std::string_view operating_system = "windows";
#elif defined(__APPLE__)
        constexpr std::string_view operating_system = "macos";
#elif defined(__linux__)
        constexpr std::string_view operating_system = "linux";
#elif defined(__FreeBSD__)
        constexpr std::string_view operating_system = "freebsd";
#else
        constexpr std::string_view operating_system = "unknown-os";
#endif
#if defined(_MSC_VER)
#if defined(_M_X64)
        constexpr std::string_view architecture = "x64";
#elif defined(_M_IX86)
        constexpr std::string_view architecture = "x86";
#elif defined(_M_ARM64)
        constexpr std::string_view architecture = "arm64";
#elif defined(_M_ARM)
        constexpr std::string_view architecture = "arm";
#else
        constexpr std::string_view architecture = "unknown-arch";
#endif
#if defined(_DLL)
        constexpr std::string_view crt = "md";
#else
        constexpr std::string_view crt = "mt";
#endif
#if defined(_DEBUG)
        constexpr std::string_view config = "debug";
#else
        constexpr std::string_view config = "release";
#endif
#if defined(_ITERATOR_DEBUG_LEVEL)
        constexpr int iterator_debug_level = _ITERATOR_DEBUG_LEVEL;
#else
        constexpr int iterator_debug_level = 0;
#endif
        return "msvc-" + std::to_string(_MSC_VER) + "-" +
            std::string(architecture) + "-" + std::string(crt) + "-it" +
            std::to_string(iterator_debug_level) + "-" + std::string(config) + "-" +
            std::string(operating_system);
#elif defined(__clang__)
#if defined(__x86_64__) || defined(_M_X64)
        constexpr std::string_view architecture = "x64";
#elif defined(__i386__) || defined(_M_IX86)
        constexpr std::string_view architecture = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
        constexpr std::string_view architecture = "arm64";
#else
        constexpr std::string_view architecture = "unknown-arch";
#endif
        std::string standard_library = "stdlib-unknown";
#if defined(_LIBCPP_VERSION)
        standard_library = "libcxx" + std::to_string(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
        standard_library = "libstdcxx" + std::to_string(__GLIBCXX__);
#endif
        return "clang-" + std::to_string(__clang_major__) + "." +
            std::to_string(__clang_minor__) + "-" + std::string(architecture) +
            "-cxx" + std::to_string(__cplusplus) + "-" + std::string(operating_system) +
            "-" + standard_library;
#elif defined(__GNUC__)
#if defined(__x86_64__)
        constexpr std::string_view architecture = "x64";
#elif defined(__i386__)
        constexpr std::string_view architecture = "x86";
#elif defined(__aarch64__)
        constexpr std::string_view architecture = "arm64";
#elif defined(__arm__)
        constexpr std::string_view architecture = "arm";
#else
        constexpr std::string_view architecture = "unknown-arch";
#endif
#if defined(_GLIBCXX_USE_CXX11_ABI)
        constexpr int libstdcxx_abi = _GLIBCXX_USE_CXX11_ABI;
#else
        constexpr int libstdcxx_abi = 1;
#endif
        std::string standard_library = "libstdcxx-unknown";
#if defined(__GLIBCXX__)
        standard_library = "libstdcxx" + std::to_string(__GLIBCXX__);
#endif
        return "gcc-" + std::to_string(__GNUC__) + "." +
            std::to_string(__GNUC_MINOR__) + "-" + std::string(architecture) +
            "-cxx" + std::to_string(__cplusplus) + "-glibcxx" +
            std::to_string(libstdcxx_abi) + "-" + std::string(operating_system) +
            "-" + standard_library;
#else
        return "unknown-native-abi";
#endif
    }

    namespace detail
    {
        namespace manifest_json
        {
            using dependency_map = std::map<std::string, std::string>;
            using scalar = std::variant<std::string, std::uint64_t, dependency_map>;

            inline bool valid_utf8(std::string_view value) noexcept
            {
                std::size_t i = 0;
                while (i < value.size())
                {
                    const auto first = static_cast<unsigned char>(value[i]);
                    if (first <= 0x7f) { ++i; continue; }

                    std::size_t count{};
                    std::uint32_t codepoint{};
                    if ((first & 0xe0) == 0xc0) { count = 2; codepoint = first & 0x1f; }
                    else if ((first & 0xf0) == 0xe0) { count = 3; codepoint = first & 0x0f; }
                    else if ((first & 0xf8) == 0xf0) { count = 4; codepoint = first & 0x07; }
                    else return false;
                    if (i + count > value.size()) return false;
                    for (std::size_t j = 1; j < count; ++j)
                    {
                        const auto next = static_cast<unsigned char>(value[i + j]);
                        if ((next & 0xc0) != 0x80) return false;
                        codepoint = (codepoint << 6) | (next & 0x3f);
                    }
                    if ((count == 2 && codepoint < 0x80) ||
                        (count == 3 && codepoint < 0x800) ||
                        (count == 4 && codepoint < 0x10000) ||
                        codepoint > 0x10ffff ||
                        (codepoint >= 0xd800 && codepoint <= 0xdfff))
                        return false;
                    i += count;
                }
                return true;
            }

            inline void append_utf8(std::string& output, std::uint32_t codepoint)
            {
                if (codepoint <= 0x7f)
                    output.push_back(static_cast<char>(codepoint));
                else if (codepoint <= 0x7ff)
                {
                    output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
                    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
                }
                else if (codepoint <= 0xffff)
                {
                    output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
                    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
                }
                else
                {
                    output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
                    output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
                    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
                }
            }

            class parser
            {
            public:
                explicit parser(std::string_view source) : m_source(source) {}

                std::map<std::string, scalar> parse()
                {
                    skip_space();
                    expect('{');
                    skip_space();
                    std::map<std::string, scalar> values;
                    if (take('}'))
                    {
                        skip_space();
                        if (m_position != m_source.size()) fail("unexpected data after JSON object");
                        return values;
                    }

                    while (true)
                    {
                        skip_space();
                        auto key = parse_string();
                        skip_space();
                        expect(':');
                        skip_space();
                        auto value = parse_scalar();
                        if (!values.emplace(std::move(key), std::move(value)).second)
                            fail("duplicate JSON key");
                        skip_space();
                        if (take('}')) break;
                        expect(',');
                    }
                    skip_space();
                    if (m_position != m_source.size()) fail("unexpected data after JSON object");
                    return values;
                }

            private:
                [[noreturn]] void fail(std::string_view message) const
                {
                    throw std::runtime_error("invalid mod.json at byte " +
                        std::to_string(m_position) + ": " + std::string(message));
                }

                void skip_space()
                {
                    while (m_position < m_source.size() &&
                        (m_source[m_position] == ' ' || m_source[m_position] == '\t' ||
                         m_source[m_position] == '\r' || m_source[m_position] == '\n'))
                        ++m_position;
                }

                bool take(char expected)
                {
                    if (m_position < m_source.size() && m_source[m_position] == expected)
                    {
                        ++m_position;
                        return true;
                    }
                    return false;
                }

                void expect(char expected)
                {
                    if (!take(expected)) fail(std::string("expected '") + expected + "'");
                }

                std::uint32_t parse_hex4()
                {
                    if (m_position + 4 > m_source.size()) fail("incomplete Unicode escape");
                    std::uint32_t value{};
                    for (int i = 0; i < 4; ++i)
                    {
                        const char c = m_source[m_position++];
                        value <<= 4;
                        if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
                        else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
                        else fail("invalid Unicode escape");
                    }
                    return value;
                }

                dependency_map parse_string_map()
                {
                    expect('{');
                    skip_space();
                    dependency_map values;
                    if (take('}')) return values;

                    while (true)
                    {
                        skip_space();
                        auto key = parse_string();
                        skip_space();
                        expect(':');
                        skip_space();
                        auto value = parse_string();
                        if (!values.emplace(std::move(key), std::move(value)).second)
                            fail("duplicate key in dependencies object");
                        skip_space();
                        if (take('}')) break;
                        expect(',');
                    }
                    return values;
                }

                std::string parse_string()
                {
                    expect('"');
                    std::string output;
                    while (m_position < m_source.size())
                    {
                        const unsigned char c = static_cast<unsigned char>(m_source[m_position++]);
                        if (c == '"')
                        {
                            if (!valid_utf8(output)) fail("string is not valid UTF-8");
                            return output;
                        }
                        if (c < 0x20) fail("unescaped control character in string");
                        if (c != '\\')
                        {
                            output.push_back(static_cast<char>(c));
                            continue;
                        }
                        if (m_position == m_source.size()) fail("incomplete string escape");
                        const char escape = m_source[m_position++];
                        switch (escape)
                        {
                        case '"': output.push_back('"'); break;
                        case '\\': output.push_back('\\'); break;
                        case '/': output.push_back('/'); break;
                        case 'b': output.push_back('\b'); break;
                        case 'f': output.push_back('\f'); break;
                        case 'n': output.push_back('\n'); break;
                        case 'r': output.push_back('\r'); break;
                        case 't': output.push_back('\t'); break;
                        case 'u':
                        {
                            auto codepoint = parse_hex4();
                            if (codepoint >= 0xd800 && codepoint <= 0xdbff)
                            {
                                if (m_position + 2 > m_source.size() ||
                                    m_source[m_position] != '\\' || m_source[m_position + 1] != 'u')
                                    fail("missing low surrogate");
                                m_position += 2;
                                const auto low = parse_hex4();
                                if (low < 0xdc00 || low > 0xdfff)
                                    fail("invalid low surrogate");
                                codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                            }
                            else if (codepoint >= 0xdc00 && codepoint <= 0xdfff)
                                fail("unexpected low surrogate");
                            append_utf8(output, codepoint);
                            break;
                        }
                        default: fail("unknown string escape");
                        }
                    }
                    fail("unterminated string");
                }

                scalar parse_scalar()
                {
                    if (m_position < m_source.size() && m_source[m_position] == '"')
                        return parse_string();
                    if (m_position < m_source.size() && m_source[m_position] == '{')
                        return parse_string_map();
                    const auto start = m_position;
                    if (m_position < m_source.size() && m_source[m_position] == '-')
                        fail("negative values are not supported in manifest fields");
                    while (m_position < m_source.size() &&
                        m_source[m_position] >= '0' && m_source[m_position] <= '9')
                        ++m_position;
                    if (start == m_position) fail("manifest fields must be strings or integers");
                    if (m_position - start > 1 && m_source[start] == '0')
                        fail("integer has a leading zero");
                    std::uint64_t value{};
                    const auto parsed = std::from_chars(m_source.data() + start,
                        m_source.data() + m_position, value);
                    if (parsed.ec != std::errc{} || parsed.ptr != m_source.data() + m_position)
                        fail("integer is out of range");
                    return value;
                }

                std::string_view m_source;
                std::size_t m_position{};
            };
        }

        inline api_version parse_api_version(std::string_view text)
        {
            const auto dot = text.find('.');
            if (dot == std::string_view::npos || text.find('.', dot + 1) != std::string_view::npos)
                throw std::runtime_error("api_version must have MAJOR.MINOR format");
            api_version version;
            const auto major = std::from_chars(text.data(), text.data() + dot, version.major);
            const auto minor = std::from_chars(text.data() + dot + 1,
                text.data() + text.size(), version.minor);
            if (major.ec != std::errc{} || major.ptr != text.data() + dot ||
                minor.ec != std::errc{} || minor.ptr != text.data() + text.size())
                throw std::runtime_error("api_version must contain unsigned decimal numbers");
            return version;
        }

        struct semantic_version
        {
            std::uint32_t major{};
            std::uint32_t minor{};
            std::uint32_t patch{};
        };

        inline semantic_version parse_semantic_version(std::string_view text)
        {
            semantic_version version;
            std::uint32_t* components[] = { &version.major, &version.minor, &version.patch };
            std::size_t start = 0;
            for (int part = 0; part < 3; ++part)
            {
                const auto dot = text.find('.', start);
                const auto end = part == 2 ? text.size() : dot;
                if (end == std::string_view::npos || end == start ||
                    (part == 2 && dot != std::string_view::npos))
                    throw std::runtime_error("version must have MAJOR.MINOR.PATCH format");
                const auto parsed = std::from_chars(text.data() + start,
                    text.data() + end, *components[part]);
                if (parsed.ec != std::errc{} || parsed.ptr != text.data() + end ||
                    (end - start > 1 && text[start] == '0'))
                    throw std::runtime_error("version must contain canonical unsigned decimal numbers");
                start = end + 1;
            }
            if (start != text.size() + 1)
                throw std::runtime_error("version must have MAJOR.MINOR.PATCH format");
            return version;
        }

        inline void validate_mod_version(std::string_view text)
        {
            (void)parse_semantic_version(text);
        }

        enum class version_relation
        {
            equal,
            less,
            less_equal,
            greater,
            greater_equal
        };

        struct version_clause
        {
            version_relation relation{};
            semantic_version version;
        };

        inline version_clause parse_version_clause(std::string_view token)
        {
            version_relation relation = version_relation::equal;
            std::size_t prefix_length = 0;
            if (token.starts_with(">="))
            {
                relation = version_relation::greater_equal;
                prefix_length = 2;
            }
            else if (token.starts_with("<="))
            {
                relation = version_relation::less_equal;
                prefix_length = 2;
            }
            else if (token.starts_with(">"))
            {
                relation = version_relation::greater;
                prefix_length = 1;
            }
            else if (token.starts_with("<"))
            {
                relation = version_relation::less;
                prefix_length = 1;
            }
            else if (token.starts_with("="))
            {
                relation = version_relation::equal;
                prefix_length = 1;
            }

            const auto version_text = token.substr(prefix_length);
            if (version_text.empty())
                throw std::runtime_error("dependency version range has an empty version clause");
            return { relation, parse_semantic_version(version_text) };
        }

        template<class Callback>
        inline void for_each_version_clause(std::string_view range, Callback&& callback)
        {
            if (range == "*") return;
            std::size_t start = 0;
            std::size_t count = 0;
            while (start < range.size())
            {
                while (start < range.size() && std::isspace(static_cast<unsigned char>(range[start])))
                    ++start;
                if (start == range.size()) break;
                auto end = start;
                while (end < range.size() && !std::isspace(static_cast<unsigned char>(range[end])))
                    ++end;
                callback(parse_version_clause(range.substr(start, end - start)));
                start = end;
                if (++count > 64)
                    throw std::runtime_error("dependency version range has more than 64 clauses");
            }
            if (count == 0)
                throw std::runtime_error("dependency version range cannot be empty");
        }

        inline int compare_versions(const semantic_version& left,
            const semantic_version& right) noexcept
        {
            if (left.major != right.major) return left.major < right.major ? -1 : 1;
            if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
            if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
            return 0;
        }

        inline bool version_satisfies(std::string_view actual,
            std::string_view range)
        {
            if (range == "*") return true;
            const auto version = parse_semantic_version(actual);
            bool matches = true;
            for_each_version_clause(range, [&](const version_clause& clause) {
                const auto order = compare_versions(version, clause.version);
                switch (clause.relation)
                {
                case version_relation::equal: matches = matches && order == 0; break;
                case version_relation::less: matches = matches && order < 0; break;
                case version_relation::less_equal: matches = matches && order <= 0; break;
                case version_relation::greater: matches = matches && order > 0; break;
                case version_relation::greater_equal: matches = matches && order >= 0; break;
                }
            });
            return matches;
        }

        inline std::string required_string(
            const std::map<std::string, manifest_json::scalar>& values, std::string_view key)
        {
            const auto found = values.find(std::string(key));
            if (found == values.end())
                throw std::runtime_error("mod.json is missing required field '" + std::string(key) + "'");
            const auto* text = std::get_if<std::string>(&found->second);
            if (!text)
                throw std::runtime_error("mod.json field '" + std::string(key) + "' must be a string");
            if (text->empty() || text->find('\0') != std::string::npos)
                throw std::runtime_error("mod.json field '" + std::string(key) + "' cannot be empty or contain NUL");
            return *text;
        }

        inline std::uint32_t required_uint(
            const std::map<std::string, manifest_json::scalar>& values, std::string_view key)
        {
            const auto found = values.find(std::string(key));
            if (found == values.end())
                throw std::runtime_error("mod.json is missing required field '" + std::string(key) + "'");
            const auto* value = std::get_if<std::uint64_t>(&found->second);
            if (!value || *value > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("mod.json field '" + std::string(key) + "' must be a 32-bit unsigned integer");
            return static_cast<std::uint32_t>(*value);
        }

        class manifest_parse_error : public std::runtime_error
        {
        public:
            manifest_parse_error(std::string mod_id, std::string message)
                : std::runtime_error(std::move(message)), m_mod_id(std::move(mod_id)) {}
            [[nodiscard]] const std::string& mod_id() const noexcept { return m_mod_id; }
        private:
            std::string m_mod_id;
        };

        inline mod_manifest parse_manifest_file(const std::filesystem::path& package_directory)
        {
            const auto package = std::filesystem::canonical(package_directory);
            if (!std::filesystem::is_directory(package))
                throw std::runtime_error("mod package path is not a directory");
            const auto manifest_path = package / "mod.json";
            if (!std::filesystem::is_regular_file(manifest_path))
                throw std::runtime_error("package does not contain mod.json");
            const auto size = std::filesystem::file_size(manifest_path);
            constexpr std::uintmax_t max_manifest_size = 1024 * 1024;
            if (size > max_manifest_size)
                throw std::runtime_error("mod.json exceeds the 1 MiB size limit");

            std::ifstream input(manifest_path, std::ios::binary);
            if (!input)
                throw std::runtime_error("cannot open mod.json");
            std::string source(static_cast<std::size_t>(size), '\0');
            if (!source.empty())
            {
                input.read(source.data(), static_cast<std::streamsize>(source.size()));
                if (!input || static_cast<std::size_t>(input.gcount()) != source.size())
                    throw std::runtime_error("cannot read complete mod.json");
            }

            auto values = manifest_json::parser(source).parse();
            static const std::unordered_set<std::string> allowed{
                "schema_version", "id", "name", "version", "api_version", "binary_abi",
                "binary", "dependencies"
            };
            mod_manifest manifest;
            try
            {
                manifest.id = required_string(values, "id");
                for (const auto& [key, _] : values)
                    if (!allowed.contains(key))
                        throw std::runtime_error("unknown mod.json field '" + key + "'");
                manifest.schema_version = required_uint(values, "schema_version");
                manifest.name = required_string(values, "name");
                manifest.version = required_string(values, "version");
                manifest.required_api = parse_api_version(required_string(values, "api_version"));
                manifest.binary_abi = required_string(values, "binary_abi");
                const auto binary = required_string(values, "binary");

                if (manifest.schema_version != 1)
                    throw std::runtime_error("unsupported manifest schema_version (supported: 1)");
                require_id(manifest.id, "mod_id");
                if (std::all_of(manifest.name.begin(), manifest.name.end(), [](unsigned char c) {
                    return std::isspace(c) != 0;
                }))
                    throw std::runtime_error("mod name cannot contain only whitespace");
                validate_mod_version(manifest.version);
                if (const auto dependencies = values.find("dependencies");
                    dependencies != values.end())
                {
                    const auto* dependency_values =
                        std::get_if<manifest_json::dependency_map>(&dependencies->second);
                    if (!dependency_values)
                        throw std::runtime_error(
                            "mod.json field 'dependencies' must be an object of mod IDs to version ranges");
                    for (const auto& [dependency_id, version_range] : *dependency_values)
                    {
                        require_id(dependency_id, "dependency mod_id");
                        if (dependency_id == manifest.id)
                            throw std::runtime_error("a mod cannot depend on itself");
                        for_each_version_clause(version_range,
                            [](const version_clause&) {});
                        manifest.dependencies.push_back({ dependency_id, version_range });
                    }
                }
                if (binary.find('\\') != std::string::npos)
                    throw std::runtime_error("binary path must use forward slashes");

                const auto relative_binary = std::filesystem::u8path(binary);
                if (relative_binary.empty() || relative_binary.is_absolute() ||
                    relative_binary.has_root_name() || relative_binary.has_root_directory())
                    throw std::runtime_error("binary path must be relative to the mod package");
                for (const auto& component : relative_binary)
                    if (component == ".." || component == ".")
                        throw std::runtime_error("binary path cannot contain '.' or '..' components");

                const auto binary_path = std::filesystem::canonical(package / relative_binary);
                const auto relative_to_package = binary_path.lexically_relative(package);
                if (relative_to_package.empty() || relative_to_package.is_absolute() ||
                    *relative_to_package.begin() == std::filesystem::path(".."))
                    throw std::runtime_error("binary path resolves outside the mod package");
                if (!std::filesystem::is_regular_file(binary_path))
                    throw std::runtime_error("mod binary is not a regular file");
                manifest.binary_path = binary_path;
                return manifest;
            }
            catch (const std::exception& error)
            {
                if (!manifest.id.empty())
                    throw manifest_parse_error(manifest.id, error.what());
                throw;
            }
        }

        inline std::string path_to_utf8(const std::filesystem::path& path)
        {
            const auto encoded = path.u8string();
            return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        }

        using mod_load_entrypoint = bool (MNGL_MOD_CALL *)(mod_context&);
        using mod_unload_entrypoint = void (MNGL_MOD_CALL *)();

        struct shared_object_deleter
        {
            void operator()(SDL_SharedObject* library) const noexcept
            {
                if (library)
                    SDL_UnloadObject(library);
            }
        };

        inline mod_load_entrypoint load_mod_load_entrypoint(SDL_SharedObject* library)
        {
            return reinterpret_cast<mod_load_entrypoint>(
                SDL_LoadFunction(library, "mngl_mod_load_v1"));
        }

        inline mod_unload_entrypoint load_mod_unload_entrypoint(SDL_SharedObject* library)
        {
            return reinterpret_cast<mod_unload_entrypoint>(
                SDL_LoadFunction(library, "mngl_mod_unload_v1"));
        }
    }

    class mod_loader
    {
    public:
        explicit mod_loader(registry& owner, mod_loader_options options = {})
            : m_registry(owner), m_options(std::move(options))
        {
            if (m_options.binary_abi.empty())
                throw std::invalid_argument("mod loader ABI tag cannot be empty");
        }

        mod_loader(const mod_loader&) = delete;
        mod_loader& operator=(const mod_loader&) = delete;

        ~mod_loader() { (void)unload_all(); }

        [[nodiscard]] mod_load_result load_package(const std::filesystem::path& package_directory)
        {
            (void)collect_deferred();
            mod_manifest manifest;
            try
            {
                manifest = detail::parse_manifest_file(package_directory);
                validate_manifest(manifest);
            }
            catch (const detail::manifest_parse_error& error)
            {
                return { mod_load_state::rejected, error.mod_id(), error.what() };
            }
            catch (const std::exception& error)
            {
                return { mod_load_state::rejected, manifest.id, error.what() };
            }

            {
                std::lock_guard lock(m_mutex);
                if (m_loaded.contains(manifest.id) || m_pending.contains(manifest.id) ||
                    m_loading.contains(manifest.id))
                    return { mod_load_state::rejected, manifest.id,
                        "a mod with this ID is already loaded or loading" };
                if (const auto dependency_error = dependency_error_locked(manifest);
                    !dependency_error.empty())
                    return { mod_load_state::rejected, manifest.id, dependency_error };
                m_loading.insert(manifest.id);
                m_loading_manifests.emplace(manifest.id, manifest);
            }

            const auto reserved_id = manifest.id;
            mod_load_result result;
            try
            {
                result = load_reserved(std::move(manifest));
            }
            catch (const std::exception& error)
            {
                result = { mod_load_state::rejected, reserved_id, error.what() };
            }
            catch (...)
            {
                result = { mod_load_state::rejected, reserved_id,
                    "unknown error while loading mod" };
            }
            {
                std::lock_guard lock(m_mutex);
                m_loading.erase(reserved_id);
                m_loading_manifests.erase(reserved_id);
            }
            (void)collect_deferred();
            return result;
        }

        [[nodiscard]] std::vector<mod_load_result> load_all(
            const std::filesystem::path& mods_directory)
        {
            std::vector<mod_load_result> results;
            try
            {
                if (!std::filesystem::is_directory(mods_directory))
                    return { { mod_load_state::rejected, {},
                        "mods path is not a directory" } };
                std::vector<std::filesystem::path> packages;
                for (const auto& entry : std::filesystem::directory_iterator(mods_directory))
                    if (entry.is_directory() &&
                        std::filesystem::is_regular_file(entry.path() / "mod.json"))
                        packages.push_back(entry.path());
                std::sort(packages.begin(), packages.end());

                struct package_candidate
                {
                    std::filesystem::path package;
                    mod_manifest manifest;
                };
                std::vector<package_candidate> candidates;
                candidates.reserve(packages.size());
                results.reserve(packages.size());
                for (const auto& package : packages)
                {
                    mod_manifest manifest;
                    try
                    {
                        manifest = detail::parse_manifest_file(package);
                        validate_manifest(manifest);
                        candidates.push_back({ package, std::move(manifest) });
                    }
                    catch (const detail::manifest_parse_error& error)
                    {
                        results.push_back({ mod_load_state::rejected, error.mod_id(), error.what() });
                    }
                    catch (const std::exception& error)
                    {
                        results.push_back({ mod_load_state::rejected, manifest.id, error.what() });
                    }
                }

                std::map<std::string, std::vector<std::size_t>> candidates_by_id;
                for (std::size_t i = 0; i < candidates.size(); ++i)
                    candidates_by_id[candidates[i].manifest.id].push_back(i);

                std::unordered_set<std::string> unavailable_ids;
                {
                    std::lock_guard lock(m_mutex);
                    for (const auto& [id, _] : m_loaded) unavailable_ids.insert(id);
                    for (const auto& [id, _] : m_pending) unavailable_ids.insert(id);
                    unavailable_ids.insert(m_loading.begin(), m_loading.end());
                }

                std::map<std::string, std::size_t> graph_indices;
                for (const auto& [id, indices] : candidates_by_id)
                {
                    if (indices.size() > 1)
                    {
                        for (const auto index : indices)
                            results.push_back({ mod_load_state::rejected, id,
                                "multiple packages declare the same mod ID" });
                    }
                    else if (unavailable_ids.contains(id))
                    {
                        results.push_back({ mod_load_state::rejected, id,
                            "a mod with this ID is already loaded, loading, or unloading" });
                    }
                    else
                    {
                        graph_indices.emplace(id, indices.front());
                    }
                }

                std::map<std::string, std::vector<std::string>> dependency_graph;
                std::map<std::string, std::size_t> indegree;
                std::map<std::string, std::vector<std::string>> dependents;
                for (const auto& [id, index] : graph_indices)
                {
                    indegree.emplace(id, 0);
                    auto& dependencies = dependency_graph[id];
                    for (const auto& dependency : candidates[index].manifest.dependencies)
                    {
                        dependencies.push_back(dependency.id);
                        if (graph_indices.contains(dependency.id))
                        {
                            ++indegree[id];
                            dependents[dependency.id].push_back(id);
                        }
                    }
                }

                std::set<std::string> ready;
                for (const auto& [id, degree] : indegree)
                    if (degree == 0) ready.insert(id);

                std::vector<std::string> order;
                order.reserve(graph_indices.size());
                while (!ready.empty())
                {
                    const auto id = *ready.begin();
                    ready.erase(ready.begin());
                    order.push_back(id);
                    if (const auto found = dependents.find(id); found != dependents.end())
                    {
                        for (const auto& dependent : found->second)
                            if (--indegree[dependent] == 0)
                                ready.insert(dependent);
                    }
                }

                if (order.size() != graph_indices.size())
                {
                    std::set<std::string> unresolved;
                    for (const auto& [id, _] : graph_indices)
                        if (indegree[id] != 0) unresolved.insert(id);
                    const auto cycle = find_dependency_cycle(dependency_graph, unresolved);
                    const auto detail = cycle.empty()
                        ? std::string("dependency cycle detected among unresolved packages")
                        : "dependency cycle detected: " + cycle;
                    for (const auto& id : unresolved)
                        results.push_back({ mod_load_state::rejected, id,
                            detail + "; this mod was not loaded" });
                }

                for (const auto& id : order)
                {
                    const auto index = graph_indices.at(id);
                    results.push_back(load_package(candidates[index].package));
                }
            }
            catch (const std::exception& error)
            {
                results.push_back({ mod_load_state::rejected, {}, error.what() });
            }
            return results;
        }

        [[nodiscard]] mod_unload_result unload(std::string_view mod_id)
        {
            loaded_module module;
            bool was_pending = false;
            {
                std::lock_guard lock(m_mutex);
                const auto found = m_loaded.find(std::string(mod_id));
                if (found == m_loaded.end())
                {
                    was_pending = m_pending.contains(std::string(mod_id));
                    if (!was_pending)
                        return { mod_unload_state::not_loaded, std::string(mod_id), 0, 0,
                            "mod is not loaded" };
                }
                else
                {
                    if (const auto dependent = blocking_dependent_locked(mod_id);
                        !dependent.empty())
                        return { mod_unload_state::dependency_in_use,
                            std::string(mod_id), 0, 0,
                            "cannot unload while dependent mod '" + dependent +
                                "' is loaded, loading, or pending unload" };
                    const auto id = found->first;
                    m_pending.emplace(id, found->second);
                    module = std::move(found->second);
                    m_loaded.erase(found);
                }
            }

            if (!was_pending)
            {
                module.lifetime->stop_accepting();
                m_registry.remove_mod(module.manifest.id);
                const auto id = module.manifest.id;
                std::shared_ptr<detail::module_lifetime> lifetime = module.lifetime;
                const auto unload_entry = module.unload;
                lifetime->request_shutdown(unload_entry ? std::function<void()>([unload_entry] {
                    unload_entry();
                }) : std::function<void()>{});
                module.lifetime.reset();
                lifetime.reset();
                (void)collect_deferred();
                return pending_result(id);
            }

            (void)collect_deferred();
            {
                std::lock_guard lock(m_mutex);
                if (!m_pending.contains(std::string(mod_id)))
                    return { mod_unload_state::unloaded, std::string(mod_id), 0, 0,
                        "deferred unload completed" };
            }
            return pending_result(std::string(mod_id));
        }

        [[nodiscard]] std::vector<mod_unload_result> unload_all()
        {
            std::vector<std::string> ids;
            std::map<std::string, std::vector<std::string>> dependencies;
            {
                std::lock_guard lock(m_mutex);
                ids.reserve(m_loaded.size());
                for (const auto& [id, module] : m_loaded)
                {
                    ids.push_back(id);
                    auto& required = dependencies[id];
                    for (const auto& dependency : module.manifest.dependencies)
                        required.push_back(dependency.id);
                }
            }

            std::set<std::string> remaining(ids.begin(), ids.end());
            ids.clear();
            while (!remaining.empty())
            {
                std::vector<std::string> leaves;
                for (const auto& candidate : remaining)
                {
                    const bool has_loaded_dependent = std::any_of(
                        remaining.begin(), remaining.end(), [&](const std::string& other) {
                            if (other == candidate) return false;
                            const auto found = dependencies.find(other);
                            return found != dependencies.end() &&
                                std::find(found->second.begin(), found->second.end(), candidate) !=
                                    found->second.end();
                        });
                    if (!has_loaded_dependent) leaves.push_back(candidate);
                }
                if (leaves.empty())
                {
                    ids.insert(ids.end(), remaining.begin(), remaining.end());
                    break;
                }
                for (const auto& leaf : leaves)
                {
                    ids.push_back(leaf);
                    remaining.erase(leaf);
                }
            }

            std::vector<mod_unload_result> results;
            results.reserve(ids.size());
            for (const auto& id : ids)
                results.push_back(unload(id));
            (void)collect_deferred();
            return results;
        }

        // Call after destroying mod-created objects to complete any deferred native unloads.
        [[nodiscard]] std::size_t collect_deferred()
        {
            std::vector<loaded_module> ready;
            {
                std::lock_guard lock(m_mutex);
                for (auto it = m_pending.begin(); it != m_pending.end();)
                {
                    const auto [active_calls, live_objects] = it->second.lifetime->outstanding();
                    if (active_calls == 0 && live_objects == 0 &&
                        it->second.lifetime.use_count() == 1)
                    {
                        ready.push_back(std::move(it->second));
                        it = m_pending.erase(it);
                    }
                    else
                        ++it;
                }
            }

            for (auto& module : ready)
            {
                module.lifetime->finish_shutdown();
                module.lifetime->close_library();
            }
            return ready.size();
        }

        [[nodiscard]] bool is_loaded(std::string_view mod_id) const
        {
            std::lock_guard lock(m_mutex);
            return m_loaded.contains(std::string(mod_id));
        }

        [[nodiscard]] std::vector<mod_manifest> loaded_mods() const
        {
            std::lock_guard lock(m_mutex);
            std::vector<mod_manifest> result;
            result.reserve(m_loaded.size());
            for (const auto& [_, module] : m_loaded) result.push_back(module.manifest);
            std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
                return left.id < right.id;
            });
            return result;
        }

    private:
        struct loaded_module
        {
            mod_manifest manifest;
            std::shared_ptr<detail::module_lifetime> lifetime;
            detail::mod_unload_entrypoint unload{};
        };

        [[nodiscard]] mod_unload_result pending_result(const std::string& mod_id) const
        {
            std::lock_guard lock(m_mutex);
            const auto found = m_pending.find(mod_id);
            if (found == m_pending.end())
                return { mod_unload_state::unloaded, mod_id, 0, 0,
                    "deferred unload completed" };
            const auto [active_calls, live_objects] = found->second.lifetime->outstanding();
            return { mod_unload_state::deferred, mod_id, active_calls, live_objects,
                "registrations removed; call collect_deferred after active calls and mod objects end" };
        }

        struct rollback_guard
        {
            mod_loader* loader{};
            mod_manifest manifest;
            std::shared_ptr<detail::module_lifetime> lifetime;
            detail::mod_unload_entrypoint unload{};
            bool active{ true };

            ~rollback_guard()
            {
                if (active && loader && lifetime)
                    loader->cleanup_failed_load(manifest, lifetime, unload);
            }
        };

        void cleanup_failed_load(const mod_manifest& manifest,
            const std::shared_ptr<detail::module_lifetime>& lifetime,
            detail::mod_unload_entrypoint unload) noexcept
        {
            lifetime->stop_accepting();
            try { m_registry.remove_mod(manifest.id); }
            catch (...) {}

            try
            {
                {
                    std::lock_guard lock(m_mutex);
                    m_pending.emplace(manifest.id, loaded_module{
                        manifest, lifetime, unload });
                }
                lifetime->request_shutdown(unload ? std::function<void()>([unload] {
                    unload();
                }) : std::function<void()>{});
            }
            catch (...) {}
        }

        void validate_manifest(const mod_manifest& manifest) const
        {
            if (manifest.required_api.major != m_options.host_api.major)
                throw std::runtime_error("mod API major version does not match host");
            if (manifest.required_api.minor > m_options.host_api.minor)
                throw std::runtime_error("mod requires a newer MNGL mod API minor version");
            if (manifest.binary_abi != m_options.binary_abi)
                throw std::runtime_error("mod binary_abi does not match host ABI tag '" +
                    m_options.binary_abi + "'");
        }

        [[nodiscard]] std::string dependency_error_locked(
            const mod_manifest& manifest) const
        {
            for (const auto& dependency : manifest.dependencies)
            {
                const auto found = m_loaded.find(dependency.id);
                if (found == m_loaded.end())
                {
                    if (m_pending.contains(dependency.id))
                        return "required dependency '" + dependency.id + "' is unloading";
                    if (m_loading.contains(dependency.id))
                        return "required dependency '" + dependency.id + "' is still loading";
                    return "required dependency '" + dependency.id + "' is not loaded";
                }
                if (!detail::version_satisfies(
                        found->second.manifest.version, dependency.version_range))
                    return "required dependency '" + dependency.id + "' version " +
                        found->second.manifest.version + " does not satisfy '" +
                        dependency.version_range + "'";
            }
            return {};
        }

        [[nodiscard]] std::string blocking_dependent_locked(
            std::string_view dependency_id) const
        {
            const auto depends_on = [dependency_id](const mod_manifest& manifest) {
                return std::any_of(manifest.dependencies.begin(), manifest.dependencies.end(),
                    [dependency_id](const mod_dependency& dependency) {
                        return dependency.id == dependency_id;
                    });
            };
            for (const auto& [id, module] : m_loaded)
                if (id != dependency_id && depends_on(module.manifest)) return id;
            for (const auto& [id, module] : m_pending)
                if (id != dependency_id && depends_on(module.manifest)) return id;
            for (const auto& [id, manifest] : m_loading_manifests)
                if (id != dependency_id && depends_on(manifest)) return id;
            return {};
        }

        [[nodiscard]] static std::string find_dependency_cycle(
            const std::map<std::string, std::vector<std::string>>& graph,
            const std::set<std::string>& unresolved)
        {
            struct frame
            {
                std::string id;
                std::size_t next_dependency{};
            };
            std::unordered_map<std::string, unsigned char> state;
            std::vector<std::string> path;
            for (const auto& start : unresolved)
            {
                if (state[start] != 0) continue;
                state[start] = 1;
                path.push_back(start);
                std::vector<frame> stack{ frame{ start, 0 } };
                while (!stack.empty())
                {
                    auto& current = stack.back();
                    const auto edges = graph.find(current.id);
                    if (edges == graph.end() ||
                        current.next_dependency >= edges->second.size())
                    {
                        state[current.id] = 2;
                        stack.pop_back();
                        path.pop_back();
                        continue;
                    }

                    const auto dependency = edges->second[current.next_dependency++];
                    if (!unresolved.contains(dependency)) continue;
                    if (state[dependency] == 0)
                    {
                        state[dependency] = 1;
                        path.push_back(dependency);
                        stack.push_back(frame{ dependency, 0 });
                    }
                    else if (state[dependency] == 1)
                    {
                        const auto first = std::find(path.begin(), path.end(), dependency);
                        std::string cycle;
                        for (auto current_id = first; current_id != path.end(); ++current_id)
                        {
                            if (!cycle.empty()) cycle += " -> ";
                            cycle += *current_id;
                        }
                        if (!cycle.empty()) cycle += " -> ";
                        cycle += dependency;
                        return cycle;
                    }
                }
            }
            return {};
        }

        mod_load_result load_reserved(mod_manifest manifest)
        {
            const auto encoded_path = detail::path_to_utf8(manifest.binary_path);
            std::unique_ptr<SDL_SharedObject, detail::shared_object_deleter> library(
                SDL_LoadObject(encoded_path.c_str()));
            if (!library)
                return { mod_load_state::rejected, manifest.id,
                    std::string("cannot load mod binary: ") + SDL_GetError() };

            const auto load = detail::load_mod_load_entrypoint(library.get());
            const auto unload = detail::load_mod_unload_entrypoint(library.get());
            if (!load)
                return { mod_load_state::rejected, manifest.id,
                    "mod binary does not export mngl_mod_load_v1" };

            auto* raw_library = library.get();
            auto lifetime = std::make_shared<detail::module_lifetime>([raw_library] {
                SDL_UnloadObject(raw_library);
            });
            library.release();
            rollback_guard rollback{ this, manifest, lifetime, unload, true };

            bool initialized = false;
            try
            {
                auto context = m_registry.for_mod(manifest.id, lifetime);
                initialized = load(context);
            }
            catch (const std::exception& error)
            {
                return { mod_load_state::rejected, manifest.id,
                    std::string("mod initialization threw: ") + error.what() };
            }
            catch (...)
            {
                return { mod_load_state::rejected, manifest.id,
                    "mod initialization threw an unknown exception" };
            }
            if (!initialized)
                return { mod_load_state::rejected, manifest.id,
                    "mngl_mod_load_v1 returned false" };

            {
                std::lock_guard lock(m_mutex);
                m_loaded.emplace(manifest.id, loaded_module{
                    manifest, lifetime, unload });
            }
            rollback.active = false;
            return { mod_load_state::loaded, manifest.id, "mod loaded" };
        }

        registry& m_registry;
        mod_loader_options m_options;
        mutable std::mutex m_mutex;
        std::unordered_map<std::string, loaded_module> m_loaded;
        std::unordered_map<std::string, loaded_module> m_pending;
        std::unordered_set<std::string> m_loading;
        std::unordered_map<std::string, mod_manifest> m_loading_manifests;
    };
}
