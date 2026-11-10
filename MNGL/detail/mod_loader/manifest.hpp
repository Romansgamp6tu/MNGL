#pragma once

#include "api.hpp"

namespace mngl::mods
{
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
					if (first <= 0x7f)
					{
						++i;
						continue;
					}

					std::size_t count{};
					std::uint32_t codepoint{};
					if ((first & 0xe0) == 0xc0)
					{
						count = 2;
						codepoint = first & 0x1f;
					}
					else if ((first & 0xf0) == 0xe0)
					{
						count = 3;
						codepoint = first & 0x0f;
					}
					else if ((first & 0xf8) == 0xf0)
					{
						count = 4;
						codepoint = first & 0x07;
					}
					else
						return false;
					if (i + count > value.size())
						return false;
					for (std::size_t j = 1; j < count; ++j)
					{
						const auto next = static_cast<unsigned char>(value[i + j]);
						if ((next & 0xc0) != 0x80)
							return false;
						codepoint = (codepoint << 6) | (next & 0x3f);
					}
					if ((count == 2 && codepoint < 0x80) || (count == 3 && codepoint < 0x800) ||
					    (count == 4 && codepoint < 0x10000) || codepoint > 0x10ffff ||
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
						if (m_position != m_source.size())
							fail("unexpected data after JSON object");
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
						if (take('}'))
							break;
						expect(',');
					}
					skip_space();
					if (m_position != m_source.size())
						fail("unexpected data after JSON object");
					return values;
				}

			private:
				[[noreturn]] void fail(std::string_view message) const
				{
					throw std::runtime_error("invalid mod.json at byte " + std::to_string(m_position) + ": " +
					                         std::string(message));
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
					if (!take(expected))
						fail(std::string("expected '") + expected + "'");
				}

				std::uint32_t parse_hex4()
				{
					if (m_position + 4 > m_source.size())
						fail("incomplete Unicode escape");
					std::uint32_t value{};
					for (int i = 0; i < 4; ++i)
					{
						const char c = m_source[m_position++];
						value <<= 4;
						if (c >= '0' && c <= '9')
							value |= static_cast<std::uint32_t>(c - '0');
						else if (c >= 'a' && c <= 'f')
							value |= static_cast<std::uint32_t>(c - 'a' + 10);
						else if (c >= 'A' && c <= 'F')
							value |= static_cast<std::uint32_t>(c - 'A' + 10);
						else
							fail("invalid Unicode escape");
					}
					return value;
				}

				dependency_map parse_string_map()
				{
					expect('{');
					skip_space();
					dependency_map values;
					if (take('}'))
						return values;

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
						if (take('}'))
							break;
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
							if (!valid_utf8(output))
								fail("string is not valid UTF-8");
							return output;
						}
						if (c < 0x20)
							fail("unescaped control character in string");
						if (c != '\\')
						{
							output.push_back(static_cast<char>(c));
							continue;
						}
						if (m_position == m_source.size())
							fail("incomplete string escape");
						const char escape = m_source[m_position++];
						switch (escape)
						{
						case '"':
							output.push_back('"');
							break;
						case '\\':
							output.push_back('\\');
							break;
						case '/':
							output.push_back('/');
							break;
						case 'b':
							output.push_back('\b');
							break;
						case 'f':
							output.push_back('\f');
							break;
						case 'n':
							output.push_back('\n');
							break;
						case 'r':
							output.push_back('\r');
							break;
						case 't':
							output.push_back('\t');
							break;
						case 'u':
						{
							auto codepoint = parse_hex4();
							if (codepoint >= 0xd800 && codepoint <= 0xdbff)
							{
								if (m_position + 2 > m_source.size() || m_source[m_position] != '\\' ||
								    m_source[m_position + 1] != 'u')
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
						default:
							fail("unknown string escape");
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
					while (m_position < m_source.size() && m_source[m_position] >= '0' &&
					       m_source[m_position] <= '9')
						++m_position;
					if (start == m_position)
						fail("manifest fields must be strings or integers");
					if (m_position - start > 1 && m_source[start] == '0')
						fail("integer has a leading zero");
					std::uint64_t value{};
					const auto parsed =
					    std::from_chars(m_source.data() + start, m_source.data() + m_position, value);
					if (parsed.ec != std::errc{} || parsed.ptr != m_source.data() + m_position)
						fail("integer is out of range");
					return value;
				}

				std::string_view m_source;
				std::size_t m_position{};
			};
		} // namespace manifest_json

		inline api_version parse_api_version(std::string_view text)
		{
			const auto dot = text.find('.');
			if (dot == std::string_view::npos || text.find('.', dot + 1) != std::string_view::npos) [[unlikely]]
				throw std::runtime_error("api_version must have MAJOR.MINOR format");
			api_version version;
			const auto major = std::from_chars(text.data(), text.data() + dot, version.major);
			const auto minor =
			    std::from_chars(text.data() + dot + 1, text.data() + text.size(), version.minor);
			if (major.ec != std::errc{} || major.ptr != text.data() + dot || minor.ec != std::errc{} ||
			    minor.ptr != text.data() + text.size()) [[unlikely]]
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
			std::uint32_t* components[] = {&version.major, &version.minor, &version.patch};
			std::size_t start = 0;
			for (int part = 0; part < 3; ++part)
			{
				const auto dot = text.find('.', start);
				const auto end = part == 2 ? text.size() : dot;
				if (end == std::string_view::npos || end == start ||
				    (part == 2 && dot != std::string_view::npos)) [[unlikely]]
					throw std::runtime_error("version must have MAJOR.MINOR.PATCH format");
				const auto parsed =
				    std::from_chars(text.data() + start, text.data() + end, *components[part]);
				if (parsed.ec != std::errc{} || parsed.ptr != text.data() + end ||
				    (end - start > 1 && text[start] == '0')) [[unlikely]]
					throw std::runtime_error("version must contain canonical unsigned decimal numbers");
				start = end + 1;
			}
			if (start != text.size() + 1) [[unlikely]]
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
			if (version_text.empty()) [[unlikely]]
				throw std::runtime_error("dependency version range has an empty version clause");
			return {relation, parse_semantic_version(version_text)};
		}

		template <class Callback>
		inline void for_each_version_clause(std::string_view range, Callback&& callback)
		{
			if (range == "*")
				return;
			std::size_t start = 0;
			std::size_t count = 0;
			while (start < range.size())
			{
				while (start < range.size() && std::isspace(static_cast<unsigned char>(range[start])))
					++start;
				if (start == range.size())
					break;
				auto end = start;
				while (end < range.size() && !std::isspace(static_cast<unsigned char>(range[end])))
					++end;
				callback(parse_version_clause(range.substr(start, end - start)));
				start = end;
				if (++count > 64) [[unlikely]]
					throw std::runtime_error("dependency version range has more than 64 clauses");
			}
			if (count == 0) [[unlikely]]
				throw std::runtime_error("dependency version range cannot be empty");
		}

		inline int compare_versions(const semantic_version& left, const semantic_version& right) noexcept
		{
			if (left.major != right.major)
				return left.major < right.major ? -1 : 1;
			if (left.minor != right.minor)
				return left.minor < right.minor ? -1 : 1;
			if (left.patch != right.patch)
				return left.patch < right.patch ? -1 : 1;
			return 0;
		}

		inline bool version_satisfies(std::string_view actual, std::string_view range)
		{
			if (range == "*")
				return true;
			const auto version = parse_semantic_version(actual);
			bool matches = true;
			for_each_version_clause(range,
			                        [&](const version_clause& clause)
			                        {
				                        const auto order = compare_versions(version, clause.version);
				                        switch (clause.relation)
				                        {
				                        case version_relation::equal:
					                        matches = matches && order == 0;
					                        break;
				                        case version_relation::less:
					                        matches = matches && order < 0;
					                        break;
				                        case version_relation::less_equal:
					                        matches = matches && order <= 0;
					                        break;
				                        case version_relation::greater:
					                        matches = matches && order > 0;
					                        break;
				                        case version_relation::greater_equal:
					                        matches = matches && order >= 0;
					                        break;
				                        }
			                        });
			return matches;
		}

		inline std::string required_string(const std::map<std::string, manifest_json::scalar>& values,
		                                   std::string_view key)
		{
			const auto found = values.find(std::string(key));
			if (found == values.end()) [[unlikely]]
				throw std::runtime_error("mod.json is missing required field '" + std::string(key) + "'");
			const auto* text = std::get_if<std::string>(&found->second);
			if (!text) [[unlikely]]
				throw std::runtime_error("mod.json field '" + std::string(key) + "' must be a string");
			if (text->empty() || text->find('\0') != std::string::npos) [[unlikely]]
				throw std::runtime_error("mod.json field '" + std::string(key) +
				                         "' cannot be empty or contain NUL");
			return *text;
		}

		inline std::uint32_t required_uint(const std::map<std::string, manifest_json::scalar>& values,
		                                   std::string_view key)
		{
			const auto found = values.find(std::string(key));
			if (found == values.end()) [[unlikely]]
				throw std::runtime_error("mod.json is missing required field '" + std::string(key) + "'");
			const auto* value = std::get_if<std::uint64_t>(&found->second);
			if (!value || *value > std::numeric_limits<std::uint32_t>::max()) [[unlikely]]
				throw std::runtime_error("mod.json field '" + std::string(key) +
				                         "' must be a 32-bit unsigned integer");
			return static_cast<std::uint32_t>(*value);
		}

		class manifest_parse_error : public std::runtime_error
		{
		public:
			manifest_parse_error(std::string mod_id, std::string message)
			    : std::runtime_error(std::move(message)), m_mod_id(std::move(mod_id))
			{
			}
			[[nodiscard]] const std::string& mod_id() const noexcept { return m_mod_id; }

		private:
			std::string m_mod_id;
		};

		inline mod_manifest parse_manifest_file(const std::filesystem::path& package_directory)
		{
			const auto package = std::filesystem::canonical(package_directory);
			if (!std::filesystem::is_directory(package)) [[unlikely]]
				throw std::runtime_error("mod package path is not a directory");
			const auto manifest_path = package / "mod.json";
			if (!std::filesystem::is_regular_file(manifest_path)) [[unlikely]]
				throw std::runtime_error("package does not contain mod.json");
			const auto size = std::filesystem::file_size(manifest_path);
			constexpr std::uintmax_t max_manifest_size = 1024 * 1024;
			if (size > max_manifest_size) [[unlikely]]
				throw std::runtime_error("mod.json exceeds the 1 MiB size limit");

			std::ifstream input(manifest_path, std::ios::binary);
			if (!input) [[unlikely]]
				throw std::runtime_error("cannot open mod.json");
			std::string source(static_cast<std::size_t>(size), '\0');
			if (!source.empty())
			{
				input.read(source.data(), static_cast<std::streamsize>(source.size()));
				if (!input || static_cast<std::size_t>(input.gcount()) != source.size()) [[unlikely]]
					throw std::runtime_error("cannot read complete mod.json");
			}

			auto values = manifest_json::parser(source).parse();
			static const std::unordered_set<std::string> allowed{
			    "schema_version", "id",         "name",   "version",
			    "api_version",    "binary_abi", "binary", "dependencies"};
			mod_manifest manifest;
			try
			{
				manifest.id = required_string(values, "id");
				for (const auto& [key, _] : values)
					if (!allowed.contains(key)) [[unlikely]]
						throw std::runtime_error("unknown mod.json field '" + key + "'");
				manifest.schema_version = required_uint(values, "schema_version");
				manifest.name = required_string(values, "name");
				manifest.version = required_string(values, "version");
				manifest.required_api = parse_api_version(required_string(values, "api_version"));
				manifest.binary_abi = required_string(values, "binary_abi");
				const auto binary = required_string(values, "binary");

				if (manifest.schema_version != 1) [[unlikely]]
					throw std::runtime_error("unsupported manifest schema_version (supported: 1)");
				require_id(manifest.id, "mod_id");
				if (std::all_of(manifest.name.begin(), manifest.name.end(),
				                [](unsigned char c) { return std::isspace(c) != 0; })) [[unlikely]]
					throw std::runtime_error("mod name cannot contain only whitespace");
				validate_mod_version(manifest.version);
				if (const auto dependencies = values.find("dependencies"); dependencies != values.end())
				{
					const auto* dependency_values =
					    std::get_if<manifest_json::dependency_map>(&dependencies->second);
					if (!dependency_values) [[unlikely]]
						throw std::runtime_error(
						    "mod.json field 'dependencies' must be an object of mod IDs to version ranges");
					for (const auto& [dependency_id, version_range] : *dependency_values)
					{
						require_id(dependency_id, "dependency mod_id");
						if (dependency_id == manifest.id) [[unlikely]]
							throw std::runtime_error("a mod cannot depend on itself");
						for_each_version_clause(version_range, [](const version_clause&) {});
						manifest.dependencies.push_back({dependency_id, version_range});
					}
				}
				if (binary.find('\\') != std::string::npos) [[unlikely]]
					throw std::runtime_error("binary path must use forward slashes");

				const auto relative_binary = std::filesystem::u8path(binary);
				if (relative_binary.empty() || relative_binary.is_absolute() ||
				    relative_binary.has_root_name() || relative_binary.has_root_directory()) [[unlikely]]
					throw std::runtime_error("binary path must be relative to the mod package");
				for (const auto& component : relative_binary)
					if (component == ".." || component == ".") [[unlikely]]
						throw std::runtime_error("binary path cannot contain '.' or '..' components");

				const auto binary_path = std::filesystem::canonical(package / relative_binary);
				const auto relative_to_package = binary_path.lexically_relative(package);
				if (relative_to_package.empty() || relative_to_package.is_absolute() ||
				    *relative_to_package.begin() == std::filesystem::path("..")) [[unlikely]]
					throw std::runtime_error("binary path resolves outside the mod package");
				if (!std::filesystem::is_regular_file(binary_path)) [[unlikely]]
					throw std::runtime_error("mod binary is not a regular file");
				manifest.binary_path = binary_path;
				return manifest;
			}
			catch (const std::exception& error)
			{
				if (!manifest.id.empty()) [[unlikely]]
					throw manifest_parse_error(manifest.id, error.what());
				throw;
			}
		}

		inline std::string path_to_utf8(const std::filesystem::path& path)
		{
			const auto encoded = path.u8string();
			return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
		}

		using mod_load_entrypoint = bool(MNGL_MOD_CALL*)(mod_context&);
		using mod_unload_entrypoint = void(MNGL_MOD_CALL*)();

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
			return reinterpret_cast<mod_load_entrypoint>(SDL_LoadFunction(library, "mngl_mod_load_v1"));
		}

		inline mod_unload_entrypoint load_mod_unload_entrypoint(SDL_SharedObject* library)
		{
			return reinterpret_cast<mod_unload_entrypoint>(SDL_LoadFunction(library, "mngl_mod_unload_v1"));
		}
	} // namespace detail

} // namespace mngl::mods
