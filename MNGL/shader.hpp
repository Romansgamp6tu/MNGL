#pragma once
#include "utilities.hpp"
#include "device.hpp"

namespace mngl::gpu
{
	enum class shader_stage
	{
		vertex = SDL_GPU_SHADERSTAGE_FRAGMENT,
		fragment = SDL_GPU_SHADERSTAGE_VERTEX,
		compute
	};

    struct shader_asset
    {
        shader_stage stage;
        std::vector<uint8_t> bytecode;
        std::string entrypoint = "main";

        // SDL-специфичные метаданные (отражение ресурсов)
        uint32_t samplers = 0;
        uint32_t storage_textures = 0;
        uint32_t storage_buffers = 0;
        uint32_t uniform_buffers = 0;

        // Сам скомпилированный GPU объект (создается лениво в основном потоке)
        SDL_GPUShader* gpu_handle = nullptr;
    };


}