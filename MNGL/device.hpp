#pragma once
#include "utilities.hpp"
#include "window.hpp"

#include <SDL3/SDL_gpu.h>

namespace mngl::gpu
{

	enum class texture_format
	{
		invalid = SDL_GPU_TEXTUREFORMAT_INVALID,

		/* Unsigned Normalized Float Color Formats */
		a8_unorm = SDL_GPU_TEXTUREFORMAT_A8_UNORM,
		r8_unorm = SDL_GPU_TEXTUREFORMAT_R8_UNORM,
		r8g8_unorm = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
		r8g8b8a8_unorm = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		r16_unorm = SDL_GPU_TEXTUREFORMAT_R16_UNORM,
		r16g16_unorm = SDL_GPU_TEXTUREFORMAT_R16G16_UNORM,
		r16g16b16a16_unorm = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM,
		r10g10b10a2_unorm = SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM,
		b5g6r5_unorm = SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM,
		b5g5r5a1_unorm = SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM,
		b4g4r4a4_unorm = SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM,
		b8g8r8a8_unorm = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,

		/* Compressed Unsigned Normalized Float Color Formats */
		bc1_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
		bc2_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM,
		bc3_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM,
		bc4_r_unorm = SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM,
		bc5_rg_unorm = SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM,
		bc7_rgba_unorm = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM,

		/* Compressed Signed Float Color Formats */
		bc6h_rgb_float = SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT,

		/* Compressed Unsigned Float Color Formats */
		bc6h_rgb_ufloat = SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT,

		/* Signed Normalized Float Color Formats  */
		r8_snorm = SDL_GPU_TEXTUREFORMAT_R8_SNORM,
		r8g8_snorm = SDL_GPU_TEXTUREFORMAT_R8G8_SNORM,
		r8g8b8a8_snorm = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
		r16_snorm = SDL_GPU_TEXTUREFORMAT_R16_SNORM,
		r16g16_snorm = SDL_GPU_TEXTUREFORMAT_R16G16_SNORM,
		r16g16b16a16_snorm = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM,

		/* Signed Float Color Formats */
		r16_float = SDL_GPU_TEXTUREFORMAT_R16_FLOAT,
		r16g16_float = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT,
		r16g16b16a16_float = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
		r32_float = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
		r32g32_float = SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,
		r32g32b32a32_float = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,

		/* Unsigned Float Color Formats */
		r11g11b10_ufloat = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,

		/* Unsigned Integer Color Formats */
		r8_uint = SDL_GPU_TEXTUREFORMAT_R8_UINT,
		r8g8_uint = SDL_GPU_TEXTUREFORMAT_R8G8_UINT,
		r8g8b8a8_uint = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT,
		r16_uint = SDL_GPU_TEXTUREFORMAT_R16_UINT,
		r16g16_uint = SDL_GPU_TEXTUREFORMAT_R16G16_UINT,
		r16g16b16a16_uint = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT,
		r32_uint = SDL_GPU_TEXTUREFORMAT_R32_UINT,
		r32g32_uint = SDL_GPU_TEXTUREFORMAT_R32G32_UINT,
		r32g32b32a32_uint = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT,

		/* Signed Integer Color Formats */
		r8_int = SDL_GPU_TEXTUREFORMAT_R8_INT,
		r8g8_int = SDL_GPU_TEXTUREFORMAT_R8G8_INT,
		r8g8b8a8_int = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT,
		r16_int = SDL_GPU_TEXTUREFORMAT_R16_INT,
		r16g16_int = SDL_GPU_TEXTUREFORMAT_R16G16_INT,
		r16g16b16a16_int = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT,
		r32_int = SDL_GPU_TEXTUREFORMAT_R32_INT,
		r32g32_int = SDL_GPU_TEXTUREFORMAT_R32G32_INT,
		r32g32b32a32_int = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT,

		/* SRGB Unsigned Normalized Color Formats */
		r8g8b8a8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
		b8g8r8a8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,

		/* Compressed SRGB Unsigned Normalized Color Formats */
		bc1_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB,
		bc2_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB,
		bc3_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB,
		bc7_rgba_unorm_srgb = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB,

		/* Depth Formats */
		d16_unorm = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
		d24_unorm = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
		d32_float = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
		d24_unorm_s8_uint = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
		d32_float_s8_uint = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,

		/* Compressed ASTC Normalized Float Color Formats */
		astc_4x4_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM,
		astc_5x4_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM,
		astc_5x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM,
		astc_6x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM,
		astc_6x6_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM,
		astc_8x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM,
		astc_8x6_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM,
		astc_8x8_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM,
		astc_10x5_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM,
		astc_10x6_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM,
		astc_10x8_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM,
		astc_10x10_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM,
		astc_12x10_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM,
		astc_12x12_unorm = SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM,

		/* Compressed SRGB ASTC Normalized Float Color Formats */
		astc_4x4_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB,
		astc_5x4_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM_SRGB,
		astc_5x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM_SRGB,
		astc_6x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM_SRGB,
		astc_6x6_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM_SRGB,
		astc_8x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM_SRGB,
		astc_8x6_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM_SRGB,
		astc_8x8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM_SRGB,
		astc_10x5_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM_SRGB,
		astc_10x6_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM_SRGB,
		astc_10x8_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM_SRGB,
		astc_10x10_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM_SRGB,
		astc_12x10_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM_SRGB,
		astc_12x12_unorm_srgb = SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM_SRGB,

		/* Compressed ASTC Signed Float Color Formats */
		astc_4x4_float = SDL_GPU_TEXTUREFORMAT_ASTC_4x4_FLOAT,
		astc_5x4_float = SDL_GPU_TEXTUREFORMAT_ASTC_5x4_FLOAT,
		astc_5x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_5x5_FLOAT,
		astc_6x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_6x5_FLOAT,
		astc_6x6_float = SDL_GPU_TEXTUREFORMAT_ASTC_6x6_FLOAT,
		astc_8x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_8x5_FLOAT,
		astc_8x6_float = SDL_GPU_TEXTUREFORMAT_ASTC_8x6_FLOAT,
		astc_8x8_float = SDL_GPU_TEXTUREFORMAT_ASTC_8x8_FLOAT,
		astc_10x5_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x5_FLOAT,
		astc_10x6_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x6_FLOAT,
		astc_10x8_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x8_FLOAT,
		astc_10x10_float = SDL_GPU_TEXTUREFORMAT_ASTC_10x10_FLOAT,
		astc_12x10_float = SDL_GPU_TEXTUREFORMAT_ASTC_12x10_FLOAT,
		astc_12x12_float = SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT
	};

	struct shader_formats
	{
		constexpr static inline SDL_GPUShaderFormat spirv = SDL_GPU_SHADERFORMAT_SPIRV;
		constexpr static inline SDL_GPUShaderFormat dxil = SDL_GPU_SHADERFORMAT_DXIL;
		constexpr static inline SDL_GPUShaderFormat msl = SDL_GPU_SHADERFORMAT_MSL;
		constexpr static inline SDL_GPUShaderFormat dxbc = SDL_GPU_SHADERFORMAT_DXBC;
		constexpr static inline SDL_GPUShaderFormat metallib = SDL_GPU_SHADERFORMAT_METALLIB;
		constexpr static inline SDL_GPUShaderFormat invalid = SDL_GPU_SHADERFORMAT_INVALID;
	};

#ifdef SDL_PLATFORM_WINDOWS
#define MNGL_DEFAULT_SHADER_FORMAT shader_formats::dxil
#elifdef SDL_PLATFORM_MACOS
#define MNGL_DEFAULT_SHADER_FORMAT shader_formats::msl
#else
#define MNGL_DEFAULT_SHADER_FORMAT shader_formats::spirv
#endif

	class device
	{
	public:
		constexpr static inline SDL_GPUShaderFormat base_formats =
			shader_formats::spirv |
			shader_formats::dxil |
			shader_formats::msl;

		[[nodiscard]] static const device& global()
		{
			static const device instance(base_formats);
			return instance;
		}

		SDL_GPUDevice* raw() const
		{
			return handle;
		}

		bool wait() const noexcept
		{
			if (!handle)
			{
				return false;
			}

			if (!SDL_WaitForGPUIdle(handle))
			{
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Failed to wait for GPU idle: %s",
					SDL_GetError());
				return false;
			}
			return true;
		}

		device(const device&) = delete;
		device& operator=(const device&) = delete;

		device(device&& o) noexcept : handle(o.handle) { o.handle = nullptr; }

		device& operator=(device&& o) noexcept
		{
			if (this != &o)
			{
				destroy();
				handle = o.handle;
				o.handle = nullptr;
			}
			return *this;
		}

		device(SDL_GPUShaderFormat allowed_formats = -1)
		{
			if (allowed_formats == -1) allowed_formats = base_formats;
			SDL_Log("MNGL startup checkpoint: creating SDL GPU device");
			TRY_SDL(handle = SDL_CreateGPUDevice(allowed_formats, debug_mode, nullptr));
			const char* driver = SDL_GetGPUDeviceDriver(handle);
			SDL_Log("MNGL startup checkpoint: SDL GPU device created with driver %s",
				driver ? driver : "<unknown>");
		}

		~device()
		{
			destroy();
		}

		SDL_GPUShaderFormat shader_format() const
		{
			return SDL_GetGPUShaderFormats(handle);
		}

		void claim_window(window& win) const
		{
			SDL_Log("MNGL startup checkpoint: claiming window for GPU device");
			TRY_SDL(SDL_ClaimWindowForGPUDevice(handle, win.raw()));
			SDL_Log("MNGL startup checkpoint: window claimed for GPU device");
		}
		void release_window(window& win) const
		{
			SDL_ReleaseWindowFromGPUDevice(handle, win.raw());
		}

		enum class hdr_mode
		{
			sdr = SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
			sdr_linear = SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
			hdr = SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR,
			hdr10 = SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084
		};

		enum class vsync_mode
		{
			vsync = SDL_GPU_PRESENTMODE_VSYNC,
			immediate = SDL_GPU_PRESENTMODE_IMMEDIATE,
			mailbox = SDL_GPU_PRESENTMODE_MAILBOX
		};
		
		void swapchain_parametrs(window& win, hdr_mode hdr, vsync_mode vsync)
		{
			TRY_SDL(SDL_SetGPUSwapchainParameters(
				handle,
				win.raw(),
				(SDL_GPUSwapchainComposition)hdr,
				(SDL_GPUPresentMode)vsync));
		}

		texture_format swapchain_texture_format(const window& win) const
		{
			return (texture_format)SDL_GetGPUSwapchainTextureFormat(handle, win.raw());
		}
	private:
		void destroy() noexcept
		{
			if (handle)
			{
				(void)wait();
				SDL_DestroyGPUDevice(handle);
				handle = nullptr;
			}
		}

		SDL_GPUDevice* handle;
	};
}