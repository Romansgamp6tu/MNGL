#pragma once

#include "device.hpp"
#include "utilities.hpp"

#include <vector>
#include <string>
#include <span>
#include <cstdint>
#include <utility>

namespace mngl::gpu
{
	enum class graphics_shader_stage : uint8_t
	{
		vertex = SDL_GPU_SHADERSTAGE_VERTEX,
		fragment = SDL_GPU_SHADERSTAGE_FRAGMENT,
	};

	enum class shader_format : uint32_t
	{
		invalid = 0,
		spirv = SDL_GPU_SHADERFORMAT_SPIRV,
		dxil = SDL_GPU_SHADERFORMAT_DXIL,
		msl = SDL_GPU_SHADERFORMAT_MSL,
	};

	constexpr bool valid_shader_stage(graphics_shader_stage stage) noexcept
	{
		return stage == graphics_shader_stage::vertex || stage == graphics_shader_stage::fragment;
	}

	struct shader_resource_layout
	{
		uint32_t samplers = 0;
		uint32_t storage_textures = 0;
		uint32_t storage_buffers = 0;
		uint32_t uniform_buffers = 0;

		constexpr bool empty() const noexcept
		{
			return samplers == 0 && storage_textures == 0 &&
				storage_buffers == 0 && uniform_buffers == 0;
		}

		constexpr bool operator==(const shader_resource_layout&) const noexcept = default;
	};

	struct shader_bytecode
	{
		shader_format format = shader_formats::invalid;
		std::vector<uint8_t> bytes;

		void assign(shader_format fmt, std::vector<uint8_t> data)
		{
			format = fmt;
			bytes = std::move(data);
		}

		void clear() noexcept
		{
			format = shader_formats::invalid;
			bytes.clear();
		}

		bool valid() const noexcept
		{
			return format != shader_formats::invalid && !bytes.empty();
		}

		explicit operator bool() const noexcept { return valid(); }
	};

	// Невладеющий view для быстрой передачи в SDL_GPUShaderCreateInfo
	struct graphics_shader_create_view
	{
		graphics_shader_stage stage = graphics_shader_stage::vertex;
		shader_format format = shader_formats::invalid;
		std::span<const uint8_t> code;
		const char* entrypoint = "main";
		shader_resource_layout resources{};

		bool valid() const noexcept
		{
			return valid_shader_stage(stage) &&
				format != shader_formats::invalid &&
				!code.empty() &&
				entrypoint != nullptr && entrypoint[0] != '\0';
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

	// CPU-ассет шейдера (байты на диске/в памяти)
	struct graphics_shader_asset
	{
		graphics_shader_stage stage = graphics_shader_stage::vertex;
		shader_bytecode code;
		std::string entrypoint = "main";
		shader_resource_layout resources{};
		std::string debug_name;

		bool valid() const noexcept { return code.valid() && !entrypoint.empty(); }
		explicit operator bool() const noexcept { return valid(); }

		graphics_shader_create_view view() const noexcept
		{
			return graphics_shader_create_view{
				.stage = stage,
				.format = code.format,
				.code = code.bytes,
				.entrypoint = entrypoint.c_str(),
				.resources = resources
			};
		}

		SDL_GPUShaderCreateInfo create_info() const noexcept
		{
			return view().to_sdl();
		}
	};

	// RAII-обёртка над GPU-объектом SDL_GPUShader*
	class graphics_shader
	{
	public:
		graphics_shader() noexcept = default;

		graphics_shader(SDL_GPUDevice* device, const graphics_shader_asset& asset)
			: m_device(device)
		{
			if (m_device && asset.valid())
			{
				auto info = asset.create_info();
				m_handle = SDL_CreateGPUShader(m_device, &info);
			}
		}

		~graphics_shader() { destroy(); }

		// Move-only (защита от двойного освобождения)
		graphics_shader(graphics_shader&& other) noexcept
			: m_device(std::exchange(other.m_device, nullptr))
			, m_handle(std::exchange(other.m_handle, nullptr))
		{ }

		graphics_shader& operator=(graphics_shader&& other) noexcept
		{
			if (this != &other)
			{
				destroy();
				m_device = std::exchange(other.m_device, nullptr);
				m_handle = std::exchange(other.m_handle, nullptr);
			}
			return *this;
		}

		graphics_shader(const graphics_shader&) = delete;
		graphics_shader& operator=(const graphics_shader&) = delete;

		[[nodiscard]] SDL_GPUShader* handle() const noexcept { return m_handle; }
		[[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
		explicit operator bool() const noexcept { return valid(); }

		void destroy() noexcept
		{
			if (m_device && m_handle)
			{
				SDL_ReleaseGPUShader(m_device, m_handle);
				m_handle = nullptr;
			}
		}

	private:
		SDL_GPUDevice* m_device = nullptr;
		SDL_GPUShader* m_handle = nullptr;
	};
}