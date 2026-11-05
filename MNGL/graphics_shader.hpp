#pragma once

#include "device.hpp"
#include "utilities.hpp"

#include <vector>
#include <string>
#include <span>
#include <array>
#include <cstring>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_precision.hpp>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <limits>
#include <ranges>

namespace mngl
{
	class texture;

	namespace gpu
	{
		class buffer;
		class sampler;
	} // namespace gpu
} // namespace mngl

namespace mngl::gpu
{
	enum class shader_stage : std::uint8_t
	{
		vertex = SDL_GPU_SHADERSTAGE_VERTEX,
		fragment = SDL_GPU_SHADERSTAGE_FRAGMENT,
		compute = 2,
	};

	using graphics_shader_stage = shader_stage;

	enum class shader_format : std::uint32_t
	{
		invalid = SDL_GPU_SHADERFORMAT_INVALID,
		spirv = SDL_GPU_SHADERFORMAT_SPIRV,
		dxbc = SDL_GPU_SHADERFORMAT_DXBC,
		dxil = SDL_GPU_SHADERFORMAT_DXIL,
		msl = SDL_GPU_SHADERFORMAT_MSL,
		metallib = SDL_GPU_SHADERFORMAT_METALLIB,
	};

	constexpr bool valid_shader_format(shader_format format) noexcept
	{
		switch (format)
		{
		case shader_format::spirv:
		case shader_format::dxbc:
		case shader_format::dxil:
		case shader_format::msl:
		case shader_format::metallib:
			return true;
		default:
			return false;
		}
	}

	constexpr bool valid_shader_stage(shader_stage stage) noexcept
	{
		return stage == shader_stage::vertex || stage == shader_stage::fragment ||
		       stage == shader_stage::compute;
	}

	constexpr bool valid_graphics_shader_stage(shader_stage stage) noexcept
	{
		return stage == shader_stage::vertex || stage == shader_stage::fragment;
	}

	constexpr bool valid_compute_shader_stage(shader_stage stage) noexcept
	{
		return stage == shader_stage::compute;
	}

	struct shader_resource_layout
	{
		std::uint32_t samplers = 0;
		std::uint32_t storage_textures = 0;
		std::uint32_t storage_buffers = 0;
		std::uint32_t uniform_buffers = 0;
		std::uint32_t read_write_storage_textures = 0;
		std::uint32_t read_write_storage_buffers = 0;

		constexpr bool empty() const noexcept
		{
			return samplers == 0 && storage_textures == 0 && storage_buffers == 0 && uniform_buffers == 0 &&
			       read_write_storage_textures == 0 && read_write_storage_buffers == 0;
		}

		constexpr bool operator==(const shader_resource_layout&) const noexcept = default;
	};

	struct shader_bytecode
	{
		shader_format format = shader_format::invalid;
		std::vector<std::uint8_t> bytes;

		void assign(shader_format fmt, std::vector<std::uint8_t> data)
		{
			format = fmt;
			bytes = std::move(data);
		}

		void clear() noexcept
		{
			format = shader_format::invalid;
			bytes.clear();
		}

		bool valid() const noexcept { return valid_shader_format(format) && !bytes.empty(); }

		explicit operator bool() const noexcept { return valid(); }
	};

	// Невладеющий view для быстрой передачи в SDL_GPUShaderCreateInfo
	struct graphics_shader_create_view
	{
		shader_stage stage = shader_stage::vertex;
		shader_format format = shader_format::invalid;
		std::span<const std::uint8_t> code;
		const char* entrypoint = "main";
		shader_resource_layout resources{};

		bool valid() const noexcept
		{
			return valid_graphics_shader_stage(stage) && valid_shader_format(format) && !code.empty() &&
			       entrypoint != nullptr && entrypoint[0] != '\0' &&
			       resources.read_write_storage_textures == 0 && resources.read_write_storage_buffers == 0;
		}

		SDL_GPUShaderCreateInfo to_sdl() const noexcept
		{
			SDL_GPUShaderCreateInfo info{};
			info.format = static_cast<SDL_GPUShaderFormat>(format);
			info.stage = static_cast<SDL_GPUShaderStage>(stage);
			info.code_size = code.size();
			info.code = code.data();
			info.entrypoint = entrypoint;
			info.num_samplers = resources.samplers;
			info.num_storage_textures = resources.storage_textures;
			info.num_storage_buffers = resources.storage_buffers;
			info.num_uniform_buffers = resources.uniform_buffers;
			return info;
		}
	};

	struct shader_uniform_field_layout
	{
		std::string name;
		std::uint32_t byte_offset = 0;
		std::uint32_t storage_size = 0;
		std::uint32_t element_size = 0;
		std::uint32_t array_stride = 0;
		std::uint32_t array_count = 1;
		std::uint8_t scalar_size = 0;
		std::uint8_t rows = 1;
		std::uint8_t columns = 1;
		bool is_array = false;
		bool is_matrix = false;
		bool is_boolean = false;
		bool is_signed = false;
		bool is_floating_point = false;
		bool row_major = false;
	};

	enum class shader_resource_kind : std::uint8_t
	{
		sampled_texture,
		storage_texture,
		storage_buffer,
		read_write_storage_texture,
		read_write_storage_buffer
	};

	struct shader_resource_field_layout
	{
		std::string name;
		shader_resource_kind kind = shader_resource_kind::sampled_texture;
		std::uint32_t slot = 0;
	};

	struct shader_resource_assignment
	{
		const ::mngl::texture* texture = nullptr;
		const sampler* sampler_state = nullptr;
		const buffer* storage_buffer = nullptr;
	};

	struct texture_binding
	{
		// Explicit non-owning pair; assigning a texture directly uses the renderer's default sampler.
		const ::mngl::texture* texture = nullptr;
		const sampler* sampler_state = nullptr;

		texture_binding(const ::mngl::texture& source, const sampler& state) noexcept
		    : texture(&source), sampler_state(&state)
		{
		}
	};

	struct shader_asset;
	struct graphics_shader_asset;
	struct compute_shader_asset;
	class shader_loader;

	namespace detail
	{
		template <typename T> struct glm_vector_traits
		{
			static constexpr bool valid = false;
		};

		template <glm::length_t Length, typename Scalar, glm::qualifier Qualifier>
		struct glm_vector_traits<glm::vec<Length, Scalar, Qualifier>>
		{
			static constexpr bool valid = true;
			static constexpr std::uint8_t rows = static_cast<std::uint8_t>(Length);
			using scalar_type = Scalar;
		};

		template <typename T> struct glm_matrix_traits
		{
			static constexpr bool valid = false;
		};

		template <glm::length_t Columns, glm::length_t Rows, typename Scalar, glm::qualifier Qualifier>
		struct glm_matrix_traits<glm::mat<Columns, Rows, Scalar, Qualifier>>
		{
			static constexpr bool valid = true;
			static constexpr std::uint8_t rows = static_cast<std::uint8_t>(Rows);
			static constexpr std::uint8_t columns = static_cast<std::uint8_t>(Columns);
			using scalar_type = Scalar;
		};

		template <typename T> struct std_array_traits
		{
			static constexpr bool valid = false;
		};

		template <typename Scalar, std::size_t Length> struct std_array_traits<std::array<Scalar, Length>>
		{
			static constexpr bool valid = Length >= 2 && Length <= 4 && std::is_arithmetic_v<Scalar>;
			static constexpr std::uint8_t rows = static_cast<std::uint8_t>(Length);
			using scalar_type = Scalar;
		};

		class shader_uniform_storage
		{
			friend struct ::mngl::gpu::shader_asset;

			std::uint32_t block_size = 0;
			std::unordered_map<std::string, shader_uniform_field_layout> layout;
			std::unordered_map<std::string, std::vector<std::uint8_t>> values;
			std::vector<std::string> declaration_order;
			std::vector<shader_resource_field_layout> resource_layout;
			std::unordered_map<std::string, shader_resource_field_layout> resource_layout_by_name;
			std::unordered_map<std::string, shader_resource_assignment> resource_values;
		};
	} // namespace detail

	// CPU-side shader data shared by graphics and compute shader assets.
	struct shader_asset
	{
		class uniform_setter
		{
		public:
			template <typename T> uniform_setter& operator=(const T& value)
			{
				m_shader->set_uniform(m_name, value);
				return *this;
			}

			uniform_setter& operator=(const ::mngl::texture& value)
			{
				m_shader->set_texture(m_name, &value, nullptr);
				return *this;
			}

			uniform_setter& operator=(texture_binding value)
			{
				m_shader->set_texture(m_name, value.texture, value.sampler_state);
				return *this;
			}

			uniform_setter& operator=(const buffer& value)
			{
				m_shader->set_storage_buffer(m_name, value);
				return *this;
			}

		private:
			friend struct shader_asset;

			uniform_setter(shader_asset& shader, std::string_view name) : m_shader(&shader), m_name(name) {}

			shader_asset* m_shader;
			std::string m_name;
		};

		shader_stage stage = shader_stage::vertex;
		shader_bytecode code;
		std::string entrypoint = "main";
		shader_resource_layout resources{};
		std::string debug_name;
		detail::shader_uniform_storage m_uniform_storage;

		// The key must match a field reflected by shader_loader.
		[[nodiscard]] uniform_setter operator[](std::string_view name) { return uniform_setter(*this, name); }

		// Numeric indices are zero-based field positions in the reflected cbuffer declaration.
		[[nodiscard]] uniform_setter operator[](std::size_t index)
		{
			if (index >= m_uniform_storage.declaration_order.size())
			{
				throw std::out_of_range("Shader uniform index is outside the reflected cbuffer layout");
			}
			return uniform_setter(*this, m_uniform_storage.declaration_order[index]);
		}

		[[nodiscard]] std::vector<std::vector<std::uint8_t>> uniform_block_data() const
		{
			if (m_uniform_storage.block_size == 0)
			{
				return {};
			}

			std::vector<std::uint8_t> block(m_uniform_storage.block_size, 0);
			for (const auto& [name, value] : m_uniform_storage.values)
			{
				const auto& field = m_uniform_storage.layout.at(name);
				std::memcpy(block.data() + field.byte_offset, value.data(), value.size());
			}
			return {std::move(block)};
		}

		[[nodiscard]] const std::vector<shader_resource_field_layout>& resource_layout() const noexcept
		{
			return m_uniform_storage.resource_layout;
		}

		[[nodiscard]] const std::unordered_map<std::string, shader_resource_assignment>& resource_values()
		    const noexcept
		{
			return m_uniform_storage.resource_values;
		}

		[[nodiscard]] bool needs_default_sampler() const noexcept
		{
			for (const auto& field : m_uniform_storage.resource_layout)
			{
				if (field.kind != shader_resource_kind::sampled_texture)
				{
					continue;
				}
				const auto value = m_uniform_storage.resource_values.find(field.name);
				if (value == m_uniform_storage.resource_values.end() || !value->second.sampler_state)
				{
					return true;
				}
			}
			return false;
		}

		bool valid() const noexcept
		{
			return valid_shader_stage(stage) && code.valid() && !entrypoint.empty();
		}
		explicit operator bool() const noexcept { return valid(); }

	private:
		friend class shader_loader;

		void set_uniform_layout(std::uint32_t block_size,
		                        const std::vector<shader_uniform_field_layout>& fields)
		{
			if (block_size == 0 || block_size % 16 != 0)
			{
				throw std::invalid_argument(
				    "Reflected shader uniform block size must be a non-zero multiple of 16");
			}

			for (const auto& field : fields)
			{
				const bool valid_scalar_size = field.scalar_size == 1 || field.scalar_size == 2 ||
				                               field.scalar_size == 4 || field.scalar_size == 8;
				const std::uint32_t matrix_rows = field.row_major ? field.rows : field.columns;
				const std::uint32_t matrix_columns = field.row_major ? field.columns : field.rows;
				const std::uint32_t matrix_stride =
				    (matrix_columns * field.scalar_size + 15) & ~std::uint32_t(15);
				const std::uint32_t expected_element_size =
				    field.is_matrix ? matrix_rows * matrix_stride
					                : static_cast<std::uint32_t>(field.rows) * field.scalar_size;
				const std::uint32_t expected_array_stride =
				    field.is_array ? (field.is_matrix ? expected_element_size
					                                  : (expected_element_size + 15) & ~std::uint32_t(15))
					               : 0;
				const bool array_size_valid =
				    !field.is_array ||
				    (field.array_count > 0 &&
					 expected_array_stride <= std::numeric_limits<std::uint32_t>::max() / field.array_count);
				const std::uint32_t expected_storage_size =
				    field.is_array ? expected_array_stride * field.array_count : expected_element_size;
				if (field.name.empty() || field.rows == 0 || field.rows > 4 || field.columns == 0 ||
				    field.columns > 4 || !valid_scalar_size || field.byte_offset % field.scalar_size != 0 ||
				    field.byte_offset > block_size || field.element_size != expected_element_size ||
				    field.array_stride != expected_array_stride || !array_size_valid ||
				    field.storage_size != expected_storage_size ||
				    field.storage_size > block_size - field.byte_offset)
				{
					throw std::invalid_argument(
					    "Reflected shader uniform layout contains an invalid or duplicate field");
				}
				if (!m_uniform_storage.layout.emplace(field.name, field).second)
				{
					throw std::invalid_argument(
					    "Reflected shader uniform layout contains an invalid or duplicate field");
				}
				m_uniform_storage.declaration_order.push_back(field.name);
			}
			m_uniform_storage.block_size = block_size;
		}

		void set_resource_layout(const std::vector<shader_resource_field_layout>& fields)
		{
			for (const auto& field : fields)
			{
				if (field.name.empty() ||
				    !m_uniform_storage.resource_layout_by_name.emplace(field.name, field).second)
				{
					throw std::invalid_argument(
					    "Reflected shader resource layout contains an invalid or duplicate field");
				}
				m_uniform_storage.resource_layout.push_back(field);
			}
		}

		template <typename T> void set_uniform(std::string_view name, const T& value)
		{
			using value_type = std::remove_cvref_t<T>;
			const auto field = m_uniform_storage.layout.find(std::string(name));
			if (field == m_uniform_storage.layout.end())
			{
				throw std::invalid_argument("Shader has no reflected uniform named '" + std::string(name) +
				                            "'");
			}

			std::vector<std::uint8_t> bytes(field->second.storage_size, 0);
			if (field->second.is_array)
			{
				if constexpr (std::ranges::input_range<const value_type>)
				{
					set_uniform_array(field->second, value, bytes);
				}
				else
				{
					throw std::invalid_argument("HLSL array uniforms require a range/container value");
				}
			}
			else if constexpr (std::is_arithmetic_v<value_type> ||
			                   detail::glm_vector_traits<value_type>::valid ||
			                   detail::glm_matrix_traits<value_type>::valid ||
			                   detail::std_array_traits<value_type>::valid ||
			                   std::is_convertible_v<const T&, float> ||
			                   std::is_convertible_v<const T&, glm::vec4>)
			{
				encode_uniform_value(field->second, value, bytes.data());
			}
			else if constexpr (std::ranges::input_range<const value_type>)
			{
				throw std::invalid_argument(
				    "Shader array values can only be assigned to reflected HLSL arrays");
			}
			else
			{
				static_assert(!std::is_same_v<T, T>, "Unsupported C++ shader uniform value type");
			}
			m_uniform_storage.values.insert_or_assign(field->first, std::move(bytes));
		}

		template <typename Getter>
		static void encode_uniform_components(const shader_uniform_field_layout& field, std::uint8_t rows,
		                                      std::uint8_t columns, Getter&& get_component,
		                                      std::uint8_t* destination)
		{
			if (rows != field.rows || columns != field.columns)
			{
				throw std::invalid_argument(
				    "Shader uniform value dimensions do not match its reflected declaration");
			}

			const bool matrix = field.is_matrix;
			const std::uint32_t matrix_vector_components = field.row_major ? field.columns : field.rows;
			const std::uint32_t matrix_stride =
			    (matrix_vector_components * field.scalar_size + 15) & ~std::uint32_t(15);
			for (std::uint8_t row = 0; row < rows; ++row)
			{
				for (std::uint8_t column = 0; column < columns; ++column)
				{
					std::uint32_t byte_offset = 0;
					if (matrix)
					{
						byte_offset = field.row_major
						                  ? static_cast<std::uint32_t>(row) * matrix_stride +
						                        static_cast<std::uint32_t>(column) * field.scalar_size
						                  : static_cast<std::uint32_t>(column) * matrix_stride +
						                        static_cast<std::uint32_t>(row) * field.scalar_size;
					}
					else
					{
						byte_offset = static_cast<std::uint32_t>(row) * field.scalar_size;
					}
					write_uniform_component(destination + byte_offset, field, get_component(row, column));
				}
			}
		}

		template <typename T>
		static void encode_uniform_value(const shader_uniform_field_layout& field, const T& value,
		                                 std::uint8_t* destination)
		{
			using value_type = std::remove_cvref_t<T>;
			if constexpr (std::is_arithmetic_v<value_type> || std::is_convertible_v<const T&, float>)
			{
				encode_uniform_components(
				    field, 1, 1, [&](std::uint8_t, std::uint8_t) { return value; }, destination);
			}
			else if constexpr (detail::glm_vector_traits<value_type>::valid)
			{
				encode_uniform_components(
				    field, detail::glm_vector_traits<value_type>::rows, 1,
				    [&](std::uint8_t row, std::uint8_t) { return value[row]; }, destination);
			}
			else if constexpr (detail::glm_matrix_traits<value_type>::valid)
			{
				encode_uniform_components(
				    field, detail::glm_matrix_traits<value_type>::rows,
				    detail::glm_matrix_traits<value_type>::columns,
				    [&](std::uint8_t row, std::uint8_t column) { return value[column][row]; }, destination);
			}
			else if constexpr (detail::std_array_traits<value_type>::valid)
			{
				encode_uniform_components(
				    field, detail::std_array_traits<value_type>::rows, 1,
				    [&](std::uint8_t row, std::uint8_t) { return value[row]; }, destination);
			}
			else if constexpr (std::is_convertible_v<const T&, glm::vec4>)
			{
				const glm::vec4 converted = static_cast<glm::vec4>(value);
				encode_uniform_components(
				    field, 4, 1, [&](std::uint8_t row, std::uint8_t) { return converted[row]; }, destination);
			}
			else
			{
				static_assert(!std::is_same_v<T, T>, "Unsupported C++ shader uniform value type");
			}
		}

		template <typename Range>
		    requires std::ranges::input_range<const Range>
		static void set_uniform_array(const shader_uniform_field_layout& field, const Range& values,
		                              std::vector<std::uint8_t>& destination)
		{
			shader_uniform_field_layout element = field;
			element.is_array = false;
			element.array_count = 1;
			element.array_stride = 0;
			element.storage_size = field.element_size;

			std::uint32_t index = 0;
			for (const auto& value : values)
			{
				if (index >= field.array_count)
				{
					throw std::invalid_argument("Shader array uniform received too many elements");
				}
				encode_uniform_value(element, value,
				                     destination.data() +
				                         static_cast<std::size_t>(index) * field.array_stride);
				++index;
			}
			if (index != field.array_count)
			{
				throw std::invalid_argument(
				    "Shader array uniform element count does not match its HLSL declaration");
			}
		}

		template <typename Target, typename Source>
		static void write_integral_component(std::uint8_t* destination, Source value)
		{
			if constexpr (std::is_integral_v<Source> && !std::is_same_v<Source, bool>)
			{
				if (!std::in_range<Target>(value))
				{
					throw std::out_of_range(
					    "Shader integer uniform value is outside the reflected type range");
				}
				const Target converted = static_cast<Target>(value);
				std::memcpy(destination, &converted, sizeof(converted));
			}
			else
			{
				throw std::invalid_argument("Shader integer uniform requires an integer C++ value");
			}
		}

		template <typename Source>
		static void write_uniform_component(std::uint8_t* destination,
		                                    const shader_uniform_field_layout& field, Source value)
		{
			using source_type = std::remove_cvref_t<Source>;
			if (field.is_boolean)
			{
				if constexpr (std::is_same_v<source_type, bool>)
				{
					// HLSL constant-buffer bool values occupy a 32-bit word.
					const std::uint32_t converted = value ? 1u : 0u;
					std::memcpy(destination, &converted, sizeof(converted));
					return;
				}
				throw std::invalid_argument("HLSL bool uniforms require bool C++ values");
			}

			if (field.is_signed)
			{
				switch (field.scalar_size)
				{
				case 1:
					write_integral_component<std::int8_t>(destination, value);
					return;
				case 2:
					write_integral_component<std::int16_t>(destination, value);
					return;
				case 4:
					write_integral_component<std::int32_t>(destination, value);
					return;
				case 8:
					write_integral_component<std::int64_t>(destination, value);
					return;
				default:
					break;
				}
			}
			else if (!field.is_floating_point)
			{
				switch (field.scalar_size)
				{
				case 1:
					write_integral_component<std::uint8_t>(destination, value);
					return;
				case 2:
					write_integral_component<std::uint16_t>(destination, value);
					return;
				case 4:
					write_integral_component<std::uint32_t>(destination, value);
					return;
				case 8:
					write_integral_component<std::uint64_t>(destination, value);
					return;
				default:
					break;
				}
			}
			else if constexpr ((std::is_arithmetic_v<source_type> && !std::is_same_v<source_type, bool>) ||
			                   std::is_convertible_v<const Source&, float>)
			{
				const double converted = static_cast<double>(value);
				switch (field.scalar_size)
				{
				case 2:
				{
					const std::uint16_t half = glm::packHalf1x16(static_cast<float>(converted));
					std::memcpy(destination, &half, sizeof(half));
					return;
				}
				case 4:
				{
					const float single = static_cast<float>(converted);
					std::memcpy(destination, &single, sizeof(single));
					return;
				}
				case 8:
					std::memcpy(destination, &converted, sizeof(converted));
					return;
				default:
					break;
				}
			}

			throw std::invalid_argument("C++ shader uniform scalar type does not match its HLSL declaration");
		}

		void set_texture(std::string_view name, const ::mngl::texture* source, const sampler* sampler_state)
		{
			const auto field = m_uniform_storage.resource_layout_by_name.find(std::string(name));
			if (!source || field == m_uniform_storage.resource_layout_by_name.end() ||
			    field->second.kind == shader_resource_kind::storage_buffer ||
			    field->second.kind == shader_resource_kind::read_write_storage_buffer)
			{
				throw std::invalid_argument("Shader has no texture resource named '" + std::string(name) +
				                            "'");
			}
			if (field->second.kind != shader_resource_kind::sampled_texture && sampler_state)
			{
				throw std::invalid_argument("store textures cannot have a sampler binding");
			}
			m_uniform_storage.resource_values.insert_or_assign(
			    field->first, shader_resource_assignment{.texture = source, .sampler_state = sampler_state});
		}

		void set_storage_buffer(std::string_view name, const buffer& source)
		{
			const auto field = m_uniform_storage.resource_layout_by_name.find(std::string(name));
			if (field == m_uniform_storage.resource_layout_by_name.end() ||
			    (field->second.kind != shader_resource_kind::storage_buffer &&
			     field->second.kind != shader_resource_kind::read_write_storage_buffer))
			{
				throw std::invalid_argument("Shader has no storage buffer resource named '" +
				                            std::string(name) + "'");
			}
			m_uniform_storage.resource_values.insert_or_assign(
			    field->first, shader_resource_assignment{.storage_buffer = &source});
		}
	};

	struct graphics_shader_asset : shader_asset
	{
		bool valid() const noexcept
		{
			return valid_graphics_shader_stage(stage) && shader_asset::valid() &&
			       resources.read_write_storage_textures == 0 && resources.read_write_storage_buffers == 0;
		}
		explicit operator bool() const noexcept { return valid(); }

		graphics_shader_create_view view() const noexcept
		{
			return graphics_shader_create_view{.stage = stage,
			                                   .format = code.format,
			                                   .code = code.bytes,
			                                   .entrypoint = entrypoint.c_str(),
			                                   .resources = resources};
		}

		SDL_GPUShaderCreateInfo create_info() const noexcept { return view().to_sdl(); }
	};

	struct compute_shader_asset : shader_asset
	{
		compute_shader_asset() noexcept { stage = shader_stage::compute; }

		glm::uvec3 threadgroup_size{0, 0, 0};

		bool valid() const noexcept
		{
			return valid_compute_shader_stage(stage) && shader_asset::valid() && threadgroup_size.x != 0 &&
			       threadgroup_size.y != 0 && threadgroup_size.z != 0;
		}
		explicit operator bool() const noexcept { return valid(); }
	};

	// RAII-обёртка над GPU-объектом SDL_GPUShader*
	class graphics_shader
	{
	public:
		graphics_shader() noexcept = default;

		graphics_shader(SDL_GPUDevice* device, const graphics_shader_asset& asset) : m_device(device)
		{
			if (!m_device)
			{
				throw std::invalid_argument("Cannot create graphics_shader with a null device");
			}
			if (!asset.valid())
			{
				throw std::invalid_argument("Cannot create graphics_shader from an invalid asset");
			}

			const auto format = static_cast<SDL_GPUShaderFormat>(asset.code.format);
			if ((SDL_GetGPUShaderFormats(m_device) & format) == 0)
			{
				throw std::invalid_argument("GPU device does not support the shader asset's format");
			}

			auto info = asset.create_info();
			m_handle = SDL_CreateGPUShader(m_device, &info);
			if (!m_handle)
			{
				throw sdl_error("Failed to create GPU shader: ");
			}
		}

		graphics_shader(const device& device, const graphics_shader_asset& asset)
		    : graphics_shader(device.raw(), asset)
		{
			m_device_owner = device;
		}

		~graphics_shader() { destroy(); }

		// Move-only (защита от двойного освобождения)
		graphics_shader(graphics_shader&& other) noexcept
		    : m_device_owner(std::move(other.m_device_owner)),
		      m_device(std::exchange(other.m_device, nullptr)),
		      m_handle(std::exchange(other.m_handle, nullptr))
		{
		}

		graphics_shader& operator=(graphics_shader&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device_owner = std::move(other.m_device_owner);
				m_device = std::exchange(other.m_device, nullptr);
				m_handle = std::exchange(other.m_handle, nullptr);
			}
			return *this;
		}

		graphics_shader(const graphics_shader&) = delete;
		graphics_shader& operator=(const graphics_shader&) = delete;

		[[nodiscard]] SDL_GPUShader* raw() const noexcept { return m_handle; }
		[[nodiscard]] SDL_GPUShader* handle() const noexcept { return raw(); }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle)
			{
				SDL_ReleaseGPUShader(m_device, m_handle);
			}
			m_handle = nullptr;
			m_device = nullptr;
			m_device_owner.reset();
		}

	private:
		std::optional<device> m_device_owner;
		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUShader* m_handle = nullptr;
	};
} // namespace mngl::gpu
