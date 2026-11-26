#pragma once

#ifndef STBTT_STATIC
#define STBTT_STATIC
#define MNGL_TEXT_UNDEFINE_STBTT_STATIC
#endif
#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define MNGL_TEXT_UNDEFINE_STB_TRUETYPE_IMPLEMENTATION
#endif
#include "imgui/imstb_truetype.h"
#ifdef MNGL_TEXT_UNDEFINE_STB_TRUETYPE_IMPLEMENTATION
#undef STB_TRUETYPE_IMPLEMENTATION
#undef MNGL_TEXT_UNDEFINE_STB_TRUETYPE_IMPLEMENTATION
#endif
#ifdef MNGL_TEXT_UNDEFINE_STBTT_STATIC
#undef STBTT_STATIC
#undef MNGL_TEXT_UNDEFINE_STBTT_STATIC
#endif

#include "texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mngl::two_d
{
	class renderer;

	namespace detail
	{
		inline constexpr std::size_t text_max_font_file_size = 64u * 1024u * 1024u;
		inline constexpr std::uint32_t text_no_atlas_page = std::numeric_limits<std::uint32_t>::max();

		struct text_glyph
		{
			const ::mngl::texture* atlas_page = nullptr;
			glm::vec4 uv_bounds{};
			std::uint32_t codepoint = 0;
			float advance = 0.0f;
			int x_offset = 0;
			int y_offset = 0;
			int width = 0;
			int height = 0;
		};
	} // namespace detail

	class font
	{
	public:
		font(const gpu::device& device, const std::filesystem::path& file, float raster_height_pixels = 48.0f,
		     glm::uvec2 atlas_size = {1024, 1024});
		explicit font(const std::filesystem::path& file, float raster_height_pixels = 48.0f,
		              glm::uvec2 atlas_size = {1024, 1024});

		font(const font&) = delete;
		font& operator=(const font&) = delete;
		font(font&&) noexcept;
		font& operator=(font&&) noexcept;
		~font();

		[[nodiscard]] float raster_height_pixels() const noexcept;
		[[nodiscard]] float ascent() const noexcept;
		[[nodiscard]] float descent() const noexcept;
		[[nodiscard]] float line_height() const noexcept;
		[[nodiscard]] float measure_text_line_width(std::string_view text, float height);
		[[nodiscard]] float measure_text_line_width_prepared(std::string_view text, float height) const;

	private:
		friend class renderer;

		struct implementation;
		void prepare_glyphs(std::string_view text);
		[[nodiscard]] detail::text_glyph glyph(std::uint32_t codepoint, bool rasterize);
		[[nodiscard]] detail::text_glyph glyph_prepared(std::uint32_t codepoint) const;
		[[nodiscard]] float kerning(std::uint32_t left, std::uint32_t right);
		[[nodiscard]] float kerning_prepared(std::uint32_t left, std::uint32_t right) const;
		[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept;
		void upload_pending(gpu::command_buffer& command_buffer);
		void mark_uploads_dirty() noexcept;

		std::unique_ptr<implementation> m_implementation;
	};

	struct text_draw_options
	{
		float height = 0.05f;
		color tint{1.0f, 1.0f, 1.0f, 1.0f};
		std::optional<rectangle> clip_bounds;
		std::optional<SDL_Rect> scissor;
		// Zero disables automatic wrapping for string-based text.
		float wrap_width = 0.0f;
	};

	class text_document
	{
	public:
		text_document();
		explicit text_document(std::string_view text);

		void set_text(std::string_view text);
		void set_line(std::size_t index, std::string_view line);
		void append_line(std::string_view line);
		void erase_line(std::size_t index);

		[[nodiscard]] std::size_t line_count() const noexcept;
		[[nodiscard]] std::string_view line(std::size_t index) const;

	private:
		std::vector<std::string> m_lines;
	};

	struct font::implementation
	{
		struct atlas_upload_state
		{
			std::mutex mutex;
			std::vector<SDL_Rect> dirty_regions;
			SDL_GPUCommandBuffer* last_command = nullptr;
			std::uint64_t revision = 0;
			bool dirty = true;
			bool full_dirty = true;
		};

		struct atlas_page
		{
			atlas_page(const gpu::device& device, glm::uvec2 size)
			    : texture(device, size.x, size.y, gpu::texture_format::r8g8b8a8_unorm,
				          ::mngl::texture::usage::sampler),
			      pixels(checked_pixel_bytes(size), 0), size(size)
			{
				for (std::size_t index = 0; index < pixels.size(); index += 4)
				{
					pixels[index] = 255;
					pixels[index + 1] = 255;
					pixels[index + 2] = 255;
				}
			}

			static std::size_t checked_pixel_bytes(glm::uvec2 size)
			{
				const std::uint64_t bytes = static_cast<std::uint64_t>(size.x) * size.y * 4u;
				if (bytes > std::numeric_limits<std::uint32_t>::max() ||
				    bytes > std::numeric_limits<std::size_t>::max()) [[unlikely]]
				{
					throw std::length_error("Font atlas page exceeds the supported upload size");
				}
				return static_cast<std::size_t>(bytes);
			}

			::mngl::texture texture;
			std::vector<std::uint8_t> pixels;
			std::shared_ptr<atlas_upload_state> upload_state = std::make_shared<atlas_upload_state>();
			glm::uvec2 size{};
			std::uint32_t cursor_x = 1;
			std::uint32_t cursor_y = 1;
			std::uint32_t row_height = 0;
		};

		struct cached_glyph
		{
			std::uint32_t codepoint = 0;
			std::uint32_t atlas_page = detail::text_no_atlas_page;
			glm::vec4 uv_bounds{};
			float advance = 0.0f;
			int x_offset = 0;
			int y_offset = 0;
			int width = 0;
			int height = 0;
			bool rasterized = false;
		};

		implementation(const gpu::device& device, const std::filesystem::path& file, float pixel_height,
		               glm::uvec2 page_size)
		    : device(device), raster_height_pixels(pixel_height), atlas_size(page_size)
		{
			if (!device.raw()) [[unlikely]]
			{
				throw std::invalid_argument("Cannot create a font with a null GPU device");
			}
			if (!std::isfinite(raster_height_pixels) || raster_height_pixels <= 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Font raster height must be finite and positive");
			}
			if (atlas_size.x < 8 || atlas_size.y < 8 ||
			    atlas_size.x > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
			    atlas_size.y > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Font atlas dimensions must be at least 8 and fit in signed integers");
			}

			std::ifstream stream(file, std::ios::binary | std::ios::ate);
			if (!stream) [[unlikely]]
			{
				throw std::runtime_error("Failed to open font file: " + file.string());
			}
			const std::streamoff file_size = stream.tellg();
			if (file_size <= 0 || static_cast<std::uint64_t>(file_size) > detail::text_max_font_file_size) [[unlikely]]
			{
				throw std::invalid_argument("Font file must be non-empty and no larger than 64 MiB");
			}

			font_data.resize(static_cast<std::size_t>(file_size));
			stream.seekg(0, std::ios::beg);
			if (!stream.read(reinterpret_cast<char*>(font_data.data()),
			                 static_cast<std::streamsize>(font_data.size()))) [[unlikely]]
			{
				throw std::runtime_error("Failed to read font file: " + file.string());
			}

			const int font_offset = stbtt_GetFontOffsetForIndex(font_data.data(), 0);
			if (font_offset < 0 || !stbtt_InitFont(&font_info, font_data.data(), font_offset)) [[unlikely]]
			{
				throw std::invalid_argument("Font file does not contain a supported TrueType font");
			}

			scale = stbtt_ScaleForPixelHeight(&font_info, raster_height_pixels);
			if (!std::isfinite(scale) || scale <= 0.0f) [[unlikely]]
			{
				throw std::invalid_argument("Font has invalid vertical metrics");
			}
			int ascent_units = 0;
			int descent_units = 0;
			int line_gap_units = 0;
			stbtt_GetFontVMetrics(&font_info, &ascent_units, &descent_units, &line_gap_units);
			ascent_pixels = static_cast<float>(ascent_units) * scale;
			descent_pixels = static_cast<float>(descent_units) * scale;
			if (!std::isfinite(ascent_pixels) || !std::isfinite(descent_pixels)) [[unlikely]]
			{
				throw std::invalid_argument("Font has invalid vertical metrics");
			}
			const std::int64_t line_height_units =
			    static_cast<std::int64_t>(ascent_units) - descent_units + line_gap_units;
			line_height_pixels = static_cast<float>(line_height_units) * scale;
			if (!std::isfinite(line_height_pixels) || line_height_pixels <= 0.0f)
			{
				line_height_pixels = raster_height_pixels;
			}
		}

		cached_glyph& get_glyph(std::uint32_t codepoint)
		{
			codepoint = resolve_codepoint(codepoint);
			if (codepoint < ascii_glyphs.size())
			{
				auto& entry = ascii_glyphs[codepoint];
				if (!entry)
				{
					entry.emplace(make_glyph(codepoint));
				}
				return *entry;
			}

			const auto found = other_glyphs.find(codepoint);
			if (found != other_glyphs.end())
			{
				return found->second;
			}
			return other_glyphs.emplace(codepoint, make_glyph(codepoint)).first->second;
		}

		cached_glyph make_glyph(std::uint32_t requested_codepoint) const
		{
			const std::uint32_t codepoint = resolve_codepoint(requested_codepoint);

			int advance_units = 0;
			stbtt_GetCodepointHMetrics(&font_info, static_cast<int>(codepoint), &advance_units, nullptr);

			cached_glyph result{};
			result.codepoint = codepoint;
			result.advance = static_cast<float>(advance_units) * scale;
			int x0 = 0;
			int y0 = 0;
			int x1 = 0;
			int y1 = 0;
			stbtt_GetCodepointBitmapBox(&font_info, static_cast<int>(codepoint), scale, scale, &x0, &y0, &x1,
			                            &y1);
			const std::int64_t width = static_cast<std::int64_t>(x1) - x0;
			const std::int64_t height = static_cast<std::int64_t>(y1) - y0;
			if (width < 0 || width > std::numeric_limits<int>::max() || height < 0 ||
			    height > std::numeric_limits<int>::max()) [[unlikely]]
			{
				throw std::length_error("Font glyph metrics exceed the supported range");
			}
			result.x_offset = x0;
			result.y_offset = y0;
			result.width = static_cast<int>(width);
			result.height = static_cast<int>(height);
			result.rasterized = result.width <= 0 || result.height <= 0;
			return result;
		}

		std::uint32_t resolve_codepoint(std::uint32_t codepoint) const
		{
			if (codepoint == 0 || stbtt_FindGlyphIndex(&font_info, static_cast<int>(codepoint)) != 0)
			{
				return codepoint;
			}
			if (stbtt_FindGlyphIndex(&font_info, 0xFFFD) != 0)
			{
				return 0xFFFD;
			}
			if (stbtt_FindGlyphIndex(&font_info, '?') != 0)
			{
				return '?';
			}
			return 0;
		}

		void rasterize(cached_glyph& glyph)
		{
			if (glyph.rasterized)
			{
				return;
			}
			if (glyph.width < 0 || glyph.height < 0 ||
			    static_cast<std::uint32_t>(glyph.width) + 2 > atlas_size.x ||
			    static_cast<std::uint32_t>(glyph.height) + 2 > atlas_size.y) [[unlikely]]
			{
				throw std::length_error("A font glyph is larger than its atlas page");
			}

			const std::uint64_t bitmap_size = static_cast<std::uint64_t>(glyph.width) * glyph.height;
			if (bitmap_size > std::numeric_limits<std::size_t>::max()) [[unlikely]]
			{
				throw std::length_error("A font glyph bitmap is too large");
			}
			std::vector<std::uint8_t> bitmap(static_cast<std::size_t>(bitmap_size));
			stbtt_MakeCodepointBitmap(&font_info, bitmap.data(), glyph.width, glyph.height, glyph.width,
			                          scale, scale, static_cast<int>(glyph.codepoint));

			auto allocation = allocate_glyph(glyph.width, glyph.height);
			auto& page = pages[allocation.page];
			std::lock_guard upload_lock(page.upload_state->mutex);
			for (int row = 0; row < glyph.height; ++row)
			{
				for (int column = 0; column < glyph.width; ++column)
				{
					const std::size_t source_index = static_cast<std::size_t>(row) * glyph.width + column;
					const std::size_t destination_index =
					    (static_cast<std::size_t>(allocation.y + row) * page.size.x + allocation.x + column) *
					    4;
					page.pixels[destination_index + 0] = 255;
					page.pixels[destination_index + 1] = 255;
					page.pixels[destination_index + 2] = 255;
					page.pixels[destination_index + 3] = bitmap[source_index];
				}
			}
			mark_dirty_region(page, SDL_Rect{static_cast<int>(allocation.x), static_cast<int>(allocation.y),
			                                 glyph.width, glyph.height});
			glyph.atlas_page = allocation.page;
			glyph.uv_bounds = {
			    static_cast<float>(allocation.x) / static_cast<float>(page.size.x),
			    static_cast<float>(allocation.y) / static_cast<float>(page.size.y),
			    static_cast<float>(allocation.x + glyph.width) / static_cast<float>(page.size.x),
			    static_cast<float>(allocation.y + glyph.height) / static_cast<float>(page.size.y)};
			glyph.rasterized = true;
		}

		void mark_dirty_region(atlas_page& page, SDL_Rect region)
		{
			auto& state = *page.upload_state;
			++state.revision;
			state.dirty = true;
			if (state.full_dirty)
			{
				return;
			}

			auto overlaps_or_touches = [](const SDL_Rect& a, const SDL_Rect& b)
			{
				return a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
			};
			for (std::size_t index = 0; index < state.dirty_regions.size();)
			{
				const auto current = state.dirty_regions[index];
				if (!overlaps_or_touches(current, region))
				{
					++index;
					continue;
				}
				const int left = std::min(current.x, region.x);
				const int top = std::min(current.y, region.y);
				const int right = std::max(current.x + current.w, region.x + region.w);
				const int bottom = std::max(current.y + current.h, region.y + region.h);
				region = {left, top, right - left, bottom - top};
				state.dirty_regions.erase(state.dirty_regions.begin() + static_cast<std::ptrdiff_t>(index));
				index = 0;
			}
			if (state.dirty_regions.size() >= 16)
			{
				state.dirty_regions.clear();
				state.full_dirty = true;
				return;
			}
			state.dirty_regions.push_back(region);
		}

		struct glyph_allocation
		{
			std::uint32_t page = 0;
			std::uint32_t x = 0;
			std::uint32_t y = 0;
		};

		glyph_allocation allocate_glyph(std::uint32_t width, std::uint32_t height)
		{
			for (std::size_t page_index = pages.size(); page_index > 0; --page_index)
			{
				if (auto allocation = try_allocate(pages[page_index - 1],
				                                   static_cast<std::uint32_t>(page_index - 1), width, height))
				{
					return *allocation;
				}
			}

			if (pages.size() >= detail::text_no_atlas_page) [[unlikely]]
			{
				throw std::length_error("Font exhausted its glyph atlas page index space");
			}
			pages.emplace_back(device, atlas_size);
			if (auto allocation =
			        try_allocate(pages.back(), static_cast<std::uint32_t>(pages.size() - 1), width, height))
			{
				return *allocation;
			}
			throw std::length_error("A font glyph does not fit in a new atlas page");
		}

		std::optional<glyph_allocation> try_allocate(atlas_page& page, std::uint32_t page_index,
		                                             std::uint32_t width, std::uint32_t height)
		{
			if (page.cursor_x + width + 1 > page.size.x)
			{
				page.cursor_x = 1;
				page.cursor_y += page.row_height + 1;
				page.row_height = 0;
			}
			if (page.cursor_y + height + 1 > page.size.y)
			{
				return std::nullopt;
			}

			const glyph_allocation result{page_index, page.cursor_x, page.cursor_y};
			page.cursor_x += width + 1;
			page.row_height = std::max(page.row_height, height);
			return result;
		}

		gpu::device device;
		float raster_height_pixels = 0.0f;
		float ascent_pixels = 0.0f;
		float descent_pixels = 0.0f;
		float line_height_pixels = 0.0f;
		float scale = 0.0f;
		glm::uvec2 atlas_size{};
		std::vector<std::uint8_t> font_data;
		stbtt_fontinfo font_info{};
		std::array<std::optional<cached_glyph>, 128> ascii_glyphs{};
		std::unordered_map<std::uint32_t, cached_glyph> other_glyphs;
		std::vector<atlas_page> pages;
		std::mutex upload_mutex;
	};

	inline font::font(const gpu::device& device, const std::filesystem::path& file,
	                  float raster_height_pixels, glm::uvec2 atlas_size)
	    : m_implementation(std::make_unique<implementation>(device, file, raster_height_pixels, atlas_size))
	{
	}

	inline font::font(const std::filesystem::path& file, float raster_height_pixels, glm::uvec2 atlas_size)
	    : font(gpu::device::global(), file, raster_height_pixels, atlas_size)
	{
	}

	inline font::font(font&&) noexcept = default;
	inline font& font::operator=(font&&) noexcept = default;
	inline font::~font() = default;

	inline float font::raster_height_pixels() const noexcept
	{
		return m_implementation ? m_implementation->raster_height_pixels : 0.0f;
	}

	inline float font::ascent() const noexcept
	{
		return m_implementation ? m_implementation->ascent_pixels : 0.0f;
	}

	inline float font::descent() const noexcept
	{
		return m_implementation ? m_implementation->descent_pixels : 0.0f;
	}

	inline float font::line_height() const noexcept
	{
		return m_implementation ? m_implementation->line_height_pixels : 0.0f;
	}

	inline detail::text_glyph font::glyph(std::uint32_t codepoint, bool rasterize)
	{
		if (!m_implementation) [[unlikely]]
		{
			throw std::logic_error("Cannot use a moved-from font");
		}

		auto& cached = m_implementation->get_glyph(codepoint);
		if (rasterize)
		{
			m_implementation->rasterize(cached);
		}

		const ::mngl::texture* page = nullptr;
		if (cached.atlas_page != detail::text_no_atlas_page)
		{
			page = &m_implementation->pages[cached.atlas_page].texture;
		}
		return {page,           cached.uv_bounds, cached.codepoint,
		        cached.advance, cached.x_offset,  cached.y_offset,
		        cached.width,   cached.height};
	}

	inline detail::text_glyph font::glyph_prepared(std::uint32_t codepoint) const
	{
		if (!m_implementation) [[unlikely]]
		{
			throw std::logic_error("Cannot use a moved-from font");
		}

		const implementation::cached_glyph* cached = nullptr;
		if (codepoint < m_implementation->ascii_glyphs.size())
		{
			const auto& entry = m_implementation->ascii_glyphs[codepoint];
			if (entry)
			{
				cached = &*entry;
			}
		}
		else if (const auto found = m_implementation->other_glyphs.find(codepoint);
		         found != m_implementation->other_glyphs.end())
		{
			cached = &found->second;
		}

		if (!cached || !cached->rasterized) [[unlikely]]
		{
			throw std::logic_error(
			    "Glyph was not prepared for parallel text rendering; call font::prepare_glyphs() first");
		}

		const ::mngl::texture* page = nullptr;
		if (cached->atlas_page != detail::text_no_atlas_page)
		{
			if (cached->atlas_page >= m_implementation->pages.size()) [[unlikely]]
			{
				throw std::logic_error("Prepared font glyph refers to an invalid atlas page");
			}
			page = &m_implementation->pages[cached->atlas_page].texture;
		}
		return {page,
		        cached->uv_bounds,
		        cached->codepoint,
		        cached->advance,
		        cached->x_offset,
		        cached->y_offset,
		        cached->width,
		        cached->height};
	}

	inline float font::kerning(std::uint32_t left, std::uint32_t right)
	{
		if (!m_implementation || left == 0 || right == 0)
		{
			return 0.0f;
		}
		const auto left_glyph = glyph(left, false);
		const auto right_glyph = glyph(right, false);
		return static_cast<float>(stbtt_GetCodepointKernAdvance(&m_implementation->font_info,
		                                                        static_cast<int>(left_glyph.codepoint),
		                                                        static_cast<int>(right_glyph.codepoint))) *
		       m_implementation->scale;
	}

	inline float font::kerning_prepared(std::uint32_t left, std::uint32_t right) const
	{
		if (!m_implementation || left == 0 || right == 0)
		{
			return 0.0f;
		}
		const auto left_glyph = glyph_prepared(left);
		const auto right_glyph = glyph_prepared(right);
		return static_cast<float>(stbtt_GetCodepointKernAdvance(&m_implementation->font_info,
		                                                        static_cast<int>(left_glyph.codepoint),
		                                                        static_cast<int>(right_glyph.codepoint))) *
		       m_implementation->scale;
	}

	inline SDL_GPUDevice* font::device_raw() const noexcept
	{
		return m_implementation ? m_implementation->device.raw() : nullptr;
	}

	inline void font::upload_pending(gpu::command_buffer& command_buffer)
	{
		if (!m_implementation) [[unlikely]]
		{
			throw std::logic_error("Cannot use a moved-from font");
		}
		const auto command_handle = command_buffer.raw();
		for (auto& page : m_implementation->pages)
		{
			auto state = page.upload_state;
			std::unique_lock upload_lock(state->mutex);
			const bool another_command_pending = state->last_command && state->last_command != command_handle;
			if (!state->dirty && !another_command_pending)
			{
				continue;
			}

			bool full_upload = state->full_dirty || another_command_pending;
			const std::uint64_t page_area = static_cast<std::uint64_t>(page.size.x) * page.size.y;
			std::uint64_t dirty_area = 0;
			for (const auto& region : state->dirty_regions)
			{
				const auto area = static_cast<std::uint64_t>(region.w) * static_cast<std::uint64_t>(region.h);
				if (area >= page_area || dirty_area >= page_area - area)
				{
					dirty_area = page_area;
					break;
				}
				dirty_area += area;
			}
			if (state->dirty_regions.size() > 16 || dirty_area >= (page_area + 1) / 2)
			{
				full_upload = true;
			}

			if (full_upload)
			{
				page.texture.update_pixels(command_buffer, std::span<const std::uint8_t>(page.pixels));
			}
			else
			{
				const auto source_stride = static_cast<std::size_t>(page.size.x) * 4;
				for (const auto& region : state->dirty_regions)
				{
					const auto offset = (static_cast<std::size_t>(region.y) * page.size.x +
					                     static_cast<std::size_t>(region.x)) * 4;
					::mngl::texture::upload_region(
					    page.texture.m_gpu_tex, command_buffer, page.pixels.data() + offset, source_stride,
					    static_cast<std::uint32_t>(region.x), static_cast<std::uint32_t>(region.y),
					    static_cast<std::uint32_t>(region.w), static_cast<std::uint32_t>(region.h), &page.texture);
				}
			}

			const auto revision = state->revision;
			command_buffer.on_completion(
			    [state, command_handle, revision](bool submitted)
			    {
				    std::lock_guard lock(state->mutex);
				    if (state->last_command == command_handle)
				    {
					    state->last_command = nullptr;
				    }
				    if (!submitted)
				    {
					    state->dirty = true;
					    state->full_dirty = true;
					    state->dirty_regions.clear();
				    }
				    else if (state->revision == revision)
				    {
					    state->dirty = false;
					    state->full_dirty = false;
					    state->dirty_regions.clear();
				    }
				    else
				    {
					    state->dirty = true;
				    }
			    });
			state->dirty = false;
			state->full_dirty = false;
			state->dirty_regions.clear();
			state->last_command = command_handle;
		}
	}

	inline void font::mark_uploads_dirty() noexcept
	{
		if (m_implementation)
		{
			for (auto& page : m_implementation->pages)
			{
			std::lock_guard upload_lock(page.upload_state->mutex);
			page.upload_state->dirty = true;
			page.upload_state->full_dirty = true;
			page.upload_state->dirty_regions.clear();
			++page.upload_state->revision;
			}
		}
	}

	inline text_document::text_document() = default;

	inline text_document::text_document(std::string_view text)
	{
		set_text(text);
	}

	inline void text_document::set_text(std::string_view text)
	{
		if (text.empty())
		{
			m_lines.clear();
			return;
		}

		std::vector<std::string> new_lines;
		std::size_t begin = 0;
		while (true)
		{
			const std::size_t end = text.find('\n', begin);
			std::string_view line_text =
			    text.substr(begin, end == std::string_view::npos ? end : end - begin);
			if (!line_text.empty() && line_text.back() == '\r')
			{
				line_text.remove_suffix(1);
			}
			new_lines.emplace_back(line_text);
			if (end == std::string_view::npos)
			{
				break;
			}
			begin = end + 1;
			if (begin == text.size())
			{
				new_lines.emplace_back();
				break;
			}
		}
		m_lines = std::move(new_lines);
	}

	inline void text_document::set_line(std::size_t index, std::string_view line_text)
	{
		if (line_text.find_first_of("\r\n") != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("A text document line cannot contain a line break");
		}
		m_lines.at(index).assign(line_text);
	}

	inline void text_document::append_line(std::string_view line_text)
	{
		if (line_text.find_first_of("\r\n") != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("A text document line cannot contain a line break");
		}
		m_lines.emplace_back(line_text);
	}

	inline void text_document::erase_line(std::size_t index)
	{
		if (index >= m_lines.size()) [[unlikely]]
		{
			throw std::out_of_range("Text document line index is out of range");
		}
		m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(index));
	}

	inline std::size_t text_document::line_count() const noexcept
	{
		return m_lines.size();
	}

	inline std::string_view text_document::line(std::size_t index) const
	{
		return m_lines.at(index);
	}

	namespace detail
	{
		inline bool next_utf8_codepoint(std::string_view text, std::size_t& offset,
		                                std::uint32_t& codepoint) noexcept
		{
			if (offset >= text.size())
			{
				return false;
			}

			const std::size_t start = offset;
			const auto first = static_cast<std::uint8_t>(text[start]);
			auto continuation = [&](std::size_t index)
			{ return index < text.size() && (static_cast<std::uint8_t>(text[index]) & 0xC0u) == 0x80u; };
			auto invalid = [&]
			{
				offset = start + 1;
				codepoint = 0xFFFD;
				return true;
			};

			if (first < 0x80u)
			{
				offset = start + 1;
				codepoint = first;
				return true;
			}

			if (first >= 0xC2u && first <= 0xDFu)
			{
				if (!continuation(start + 1))
				{
					return invalid();
				}
				codepoint = (static_cast<std::uint32_t>(first & 0x1Fu) << 6) |
				            (static_cast<std::uint8_t>(text[start + 1]) & 0x3Fu);
				offset = start + 2;
				return true;
			}

			if (first >= 0xE0u && first <= 0xEFu)
			{
				if (!continuation(start + 1) || !continuation(start + 2))
				{
					return invalid();
				}
				const auto second = static_cast<std::uint8_t>(text[start + 1]);
				if ((first == 0xE0u && second < 0xA0u) || (first == 0xEDu && second >= 0xA0u))
				{
					return invalid();
				}
				codepoint = (static_cast<std::uint32_t>(first & 0x0Fu) << 12) |
				            (static_cast<std::uint32_t>(second & 0x3Fu) << 6) |
				            (static_cast<std::uint8_t>(text[start + 2]) & 0x3Fu);
				offset = start + 3;
				return true;
			}

			if (first >= 0xF0u && first <= 0xF4u)
			{
				if (!continuation(start + 1) || !continuation(start + 2) || !continuation(start + 3))
				{
					return invalid();
				}
				const auto second = static_cast<std::uint8_t>(text[start + 1]);
				if ((first == 0xF0u && second < 0x90u) || (first == 0xF4u && second > 0x8Fu))
				{
					return invalid();
				}
				codepoint =
				    (static_cast<std::uint32_t>(first & 0x07u) << 18) |
				    (static_cast<std::uint32_t>(second & 0x3Fu) << 12) |
				    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(text[start + 2]) & 0x3Fu) << 6) |
				    (static_cast<std::uint8_t>(text[start + 3]) & 0x3Fu);
				offset = start + 4;
				return true;
			}

			return invalid();
		}
	} // namespace detail

	inline float font::measure_text_line_width(std::string_view text, float height)
	{
		if (!m_implementation) [[unlikely]]
		{
			throw std::logic_error("Cannot use a moved-from font");
		}
		if (!std::isfinite(height) || height <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Measured text height must be finite and positive");
		}
		if (text.find_first_of("\r\n") != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("Text line measurement does not accept line breaks");
		}

		const float scale = height / raster_height_pixels();
		if (!std::isfinite(scale) || scale <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Font metrics cannot be scaled to the requested text height");
		}

		float pen_x = 0.0f;
		std::uint32_t previous_codepoint = 0;
		const float tab_width = glyph(' ', false).advance * scale * 4.0f;

		for (std::size_t offset = 0; offset < text.size();)
		{
			std::uint32_t codepoint = 0;
			(void)detail::next_utf8_codepoint(text, offset, codepoint);
			if (codepoint == '\t')
			{
				if (tab_width > 0.0f)
				{
					pen_x = (std::floor(pen_x / tab_width) + 1.0f) * tab_width;
				}
				if (!std::isfinite(pen_x)) [[unlikely]]
				{
					throw std::length_error("Measured text exceeds the renderer's coordinate range");
				}
				previous_codepoint = 0;
				continue;
			}

			const auto metrics = glyph(codepoint, false);
			if (previous_codepoint != 0)
			{
				pen_x += kerning(previous_codepoint, metrics.codepoint) * scale;
			}
			pen_x += metrics.advance * scale;
			if (!std::isfinite(pen_x)) [[unlikely]]
			{
				throw std::length_error("Measured text exceeds the renderer's coordinate range");
			}
			previous_codepoint = metrics.codepoint;
		}

		return pen_x;
	}

	inline float font::measure_text_line_width_prepared(std::string_view text, float height) const
	{
		if (!m_implementation) [[unlikely]]
		{
			throw std::logic_error("Cannot use a moved-from font");
		}
		if (!std::isfinite(height) || height <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Measured text height must be finite and positive");
		}
		if (text.find_first_of("\r\n") != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("Text line measurement does not accept line breaks");
		}

		const float scale = height / raster_height_pixels();
		if (!std::isfinite(scale) || scale <= 0.0f) [[unlikely]]
		{
			throw std::invalid_argument("Font metrics cannot be scaled to the requested text height");
		}

		float pen_x = 0.0f;
		std::uint32_t previous_codepoint = 0;
		const float tab_width = glyph_prepared(' ').advance * scale * 4.0f;

		for (std::size_t offset = 0; offset < text.size();)
		{
			std::uint32_t codepoint = 0;
			(void)detail::next_utf8_codepoint(text, offset, codepoint);
			if (codepoint == '\t')
			{
				if (tab_width > 0.0f)
				{
					pen_x = (std::floor(pen_x / tab_width) + 1.0f) * tab_width;
				}
				if (!std::isfinite(pen_x)) [[unlikely]]
				{
					throw std::length_error("Measured text exceeds the renderer's coordinate range");
				}
				previous_codepoint = 0;
				continue;
			}

			const auto metrics = glyph_prepared(codepoint);
			if (previous_codepoint != 0)
			{
				pen_x += kerning_prepared(previous_codepoint, metrics.codepoint) * scale;
			}
			pen_x += metrics.advance * scale;
			if (!std::isfinite(pen_x)) [[unlikely]]
			{
				throw std::length_error("Measured text exceeds the renderer's coordinate range");
			}
			previous_codepoint = metrics.codepoint;
		}

		return pen_x;
	}

	inline void font::prepare_glyphs(std::string_view text)
	{
		(void)glyph(' ', true);
		for (std::size_t offset = 0; offset < text.size();)
		{
			std::uint32_t codepoint = 0;
			(void)detail::next_utf8_codepoint(text, offset, codepoint);
			if (codepoint != '\t' && codepoint != '\r' && codepoint != '\n')
			{
				const auto prepared = glyph(codepoint, true);
				if (prepared.codepoint != codepoint)
				{
					(void)glyph(prepared.codepoint, true);
				}
			}
		}
	}
} // namespace mngl::two_d
