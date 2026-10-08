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
#include <fstream>
#include <filesystem>
#include <limits>
#include <memory>
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
		inline constexpr uint32_t text_no_atlas_page =
			std::numeric_limits<uint32_t>::max();

		struct text_glyph
		{
			const ::mngl::texture* atlas_page = nullptr;
			glm::vec4 uv_bounds{};
			uint32_t codepoint = 0;
			float advance = 0.0f;
			int x_offset = 0;
			int y_offset = 0;
			int width = 0;
			int height = 0;
		};
	}

	class font
	{
	public:
		font(
			const gpu::device& device,
			const std::filesystem::path& file,
			float raster_height_pixels = 48.0f,
			glm::uvec2 atlas_size = { 1024, 1024 });
		explicit font(
			const std::filesystem::path& file,
			float raster_height_pixels = 48.0f,
			glm::uvec2 atlas_size = { 1024, 1024 });

		font(const font&) = delete;
		font& operator=(const font&) = delete;
		font(font&&) noexcept;
		font& operator=(font&&) noexcept;
		~font();

		[[nodiscard]] float raster_height_pixels() const noexcept;
		[[nodiscard]] float ascent() const noexcept;
		[[nodiscard]] float descent() const noexcept;
		[[nodiscard]] float line_height() const noexcept;

	private:
		friend class renderer;

		struct implementation;
		[[nodiscard]] detail::text_glyph glyph(uint32_t codepoint, bool rasterize);
		[[nodiscard]] float kerning(uint32_t left, uint32_t right);
		[[nodiscard]] SDL_GPUDevice* device_raw() const noexcept;
		void upload_pending(gpu::command_buffer& command_buffer);
		void mark_uploads_dirty() noexcept;

		std::unique_ptr<implementation> m_implementation;
	};

	struct text_draw_options
	{
		float height = 0.05f;
		color tint{ 1.0f, 1.0f, 1.0f, 1.0f };
		std::optional<rectangle> clip_bounds;
		std::optional<SDL_Rect> scissor;
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
		struct atlas_page
		{
			atlas_page(
				const gpu::device& device,
				glm::uvec2 size
			)
				: texture(
					device,
					size.x,
					size.y,
					gpu::texture_format::r8g8b8a8_unorm,
					::mngl::texture::usage::sampler)
				, pixels(checked_pixel_bytes(size), 0)
				, size(size)
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
				const uint64_t bytes =
					static_cast<uint64_t>(size.x) * size.y * 4u;
				if (bytes > std::numeric_limits<uint32_t>::max() ||
					bytes > std::numeric_limits<std::size_t>::max())
				{
					throw std::length_error(
						"Font atlas page exceeds the supported upload size");
				}
				return static_cast<std::size_t>(bytes);
			}

			::mngl::texture texture;
			std::vector<uint8_t> pixels;
			glm::uvec2 size{};
			uint32_t cursor_x = 1;
			uint32_t cursor_y = 1;
			uint32_t row_height = 0;
			bool dirty = false;
		};

		struct cached_glyph
		{
			uint32_t codepoint = 0;
			uint32_t atlas_page = detail::text_no_atlas_page;
			glm::vec4 uv_bounds{};
			float advance = 0.0f;
			int x_offset = 0;
			int y_offset = 0;
			int width = 0;
			int height = 0;
			bool rasterized = false;
		};

		implementation(
			const gpu::device& device,
			const std::filesystem::path& file,
			float pixel_height,
			glm::uvec2 page_size)
			: device(device)
			, raster_height_pixels(pixel_height)
			, atlas_size(page_size)
		{
			if (!device.raw())
			{
				throw std::invalid_argument("Cannot create a font with a null GPU device");
			}
			if (!std::isfinite(raster_height_pixels) || raster_height_pixels <= 0.0f)
			{
				throw std::invalid_argument("Font raster height must be finite and positive");
			}
			if (atlas_size.x < 8 || atlas_size.y < 8 ||
				atlas_size.x > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
				atlas_size.y > static_cast<uint32_t>(std::numeric_limits<int>::max()))
			{
				throw std::invalid_argument(
					"Font atlas dimensions must be at least 8 and fit in signed integers");
			}

			std::ifstream stream(file, std::ios::binary | std::ios::ate);
			if (!stream)
			{
				throw std::runtime_error("Failed to open font file: " + file.string());
			}
			const std::streamoff file_size = stream.tellg();
			if (file_size <= 0 ||
				static_cast<uint64_t>(file_size) > detail::text_max_font_file_size)
			{
				throw std::invalid_argument(
					"Font file must be non-empty and no larger than 64 MiB");
			}

			font_data.resize(static_cast<std::size_t>(file_size));
			stream.seekg(0, std::ios::beg);
			if (!stream.read(
				reinterpret_cast<char*>(font_data.data()),
				static_cast<std::streamsize>(font_data.size())))
			{
				throw std::runtime_error("Failed to read font file: " + file.string());
			}

			const int font_offset = stbtt_GetFontOffsetForIndex(font_data.data(), 0);
			if (font_offset < 0 || !stbtt_InitFont(&font_info, font_data.data(), font_offset))
			{
				throw std::invalid_argument(
					"Font file does not contain a supported TrueType font");
			}

			scale = stbtt_ScaleForPixelHeight(&font_info, raster_height_pixels);
			if (!std::isfinite(scale) || scale <= 0.0f)
			{
				throw std::invalid_argument("Font has invalid vertical metrics");
			}
			int ascent_units = 0;
			int descent_units = 0;
			int line_gap_units = 0;
			stbtt_GetFontVMetrics(
				&font_info,
				&ascent_units,
				&descent_units,
				&line_gap_units);
			ascent_pixels = static_cast<float>(ascent_units) * scale;
			descent_pixels = static_cast<float>(descent_units) * scale;
			if (!std::isfinite(ascent_pixels) || !std::isfinite(descent_pixels))
			{
				throw std::invalid_argument("Font has invalid vertical metrics");
			}
			const int64_t line_height_units =
				static_cast<int64_t>(ascent_units) - descent_units + line_gap_units;
			line_height_pixels = static_cast<float>(line_height_units) * scale;
			if (!std::isfinite(line_height_pixels) || line_height_pixels <= 0.0f)
			{
				line_height_pixels = raster_height_pixels;
			}
		}

		cached_glyph& get_glyph(uint32_t codepoint)
		{
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

		cached_glyph make_glyph(uint32_t requested_codepoint) const
		{
			uint32_t codepoint = requested_codepoint;
			if (codepoint != 0 &&
				stbtt_FindGlyphIndex(&font_info, static_cast<int>(codepoint)) == 0)
			{
				if (stbtt_FindGlyphIndex(&font_info, 0xFFFD) != 0)
				{
					codepoint = 0xFFFD;
				}
				else if (stbtt_FindGlyphIndex(&font_info, '?') != 0)
				{
					codepoint = '?';
				}
				else
				{
					codepoint = 0;
				}
			}

			int advance_units = 0;
			stbtt_GetCodepointHMetrics(
				&font_info,
				static_cast<int>(codepoint),
				&advance_units,
				nullptr);

			cached_glyph result{};
			result.codepoint = codepoint;
			result.advance = static_cast<float>(advance_units) * scale;
			int x0 = 0;
			int y0 = 0;
			int x1 = 0;
			int y1 = 0;
			stbtt_GetCodepointBitmapBox(
				&font_info,
				static_cast<int>(codepoint),
				scale,
				scale,
				&x0,
				&y0,
				&x1,
				&y1);
			const int64_t width = static_cast<int64_t>(x1) - x0;
			const int64_t height = static_cast<int64_t>(y1) - y0;
			if (width < 0 || width > std::numeric_limits<int>::max() ||
				height < 0 || height > std::numeric_limits<int>::max())
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

		void rasterize(cached_glyph& glyph)
		{
			if (glyph.rasterized)
			{
				return;
			}
			if (glyph.width < 0 || glyph.height < 0 ||
				static_cast<uint32_t>(glyph.width) + 2 > atlas_size.x ||
				static_cast<uint32_t>(glyph.height) + 2 > atlas_size.y)
			{
				throw std::length_error("A font glyph is larger than its atlas page");
			}

			const uint64_t bitmap_size =
				static_cast<uint64_t>(glyph.width) * glyph.height;
			if (bitmap_size > std::numeric_limits<std::size_t>::max())
			{
				throw std::length_error("A font glyph bitmap is too large");
			}
			std::vector<uint8_t> bitmap(static_cast<std::size_t>(bitmap_size));
			stbtt_MakeCodepointBitmap(
				&font_info,
				bitmap.data(),
				glyph.width,
				glyph.height,
				glyph.width,
				scale,
				scale,
				static_cast<int>(glyph.codepoint));

			auto allocation = allocate_glyph(glyph.width, glyph.height);
			auto& page = pages[allocation.page];
			for (int row = 0; row < glyph.height; ++row)
			{
				for (int column = 0; column < glyph.width; ++column)
				{
					const std::size_t source_index =
						static_cast<std::size_t>(row) * glyph.width + column;
					const std::size_t destination_index =
						(static_cast<std::size_t>(allocation.y + row) * page.size.x +
							allocation.x + column) * 4;
					page.pixels[destination_index + 0] = 255;
					page.pixels[destination_index + 1] = 255;
					page.pixels[destination_index + 2] = 255;
					page.pixels[destination_index + 3] = bitmap[source_index];
				}
			}
			page.dirty = true;
			glyph.atlas_page = allocation.page;
			glyph.uv_bounds = {
				static_cast<float>(allocation.x) / static_cast<float>(page.size.x),
				static_cast<float>(allocation.y) / static_cast<float>(page.size.y),
				static_cast<float>(allocation.x + glyph.width) / static_cast<float>(page.size.x),
				static_cast<float>(allocation.y + glyph.height) / static_cast<float>(page.size.y)
			};
			glyph.rasterized = true;
		}

		struct glyph_allocation
		{
			uint32_t page = 0;
			uint32_t x = 0;
			uint32_t y = 0;
		};

		glyph_allocation allocate_glyph(uint32_t width, uint32_t height)
		{
			for (std::size_t page_index = pages.size(); page_index > 0; --page_index)
			{
				if (auto allocation = try_allocate(
					pages[page_index - 1],
					static_cast<uint32_t>(page_index - 1),
					width,
					height))
				{
					return *allocation;
				}
			}

			if (pages.size() >= detail::text_no_atlas_page)
			{
				throw std::length_error("Font exhausted its glyph atlas page index space");
			}
			pages.emplace_back(device, atlas_size);
			if (auto allocation = try_allocate(
				pages.back(),
				static_cast<uint32_t>(pages.size() - 1),
				width,
				height))
			{
				return *allocation;
			}
			throw std::length_error("A font glyph does not fit in a new atlas page");
		}

		std::optional<glyph_allocation> try_allocate(
			atlas_page& page,
			uint32_t page_index,
			uint32_t width,
			uint32_t height)
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

			const glyph_allocation result{ page_index, page.cursor_x, page.cursor_y };
			page.cursor_x += width + 1;
			page.row_height = std::max(page.row_height, height);
			return result;
		}

		const gpu::device& device;
		float raster_height_pixels = 0.0f;
		float ascent_pixels = 0.0f;
		float descent_pixels = 0.0f;
		float line_height_pixels = 0.0f;
		float scale = 0.0f;
		glm::uvec2 atlas_size{};
		std::vector<uint8_t> font_data;
		stbtt_fontinfo font_info{};
		std::array<std::optional<cached_glyph>, 128> ascii_glyphs{};
		std::unordered_map<uint32_t, cached_glyph> other_glyphs;
		std::vector<atlas_page> pages;
	};

	inline font::font(
		const gpu::device& device,
		const std::filesystem::path& file,
		float raster_height_pixels,
		glm::uvec2 atlas_size)
		: m_implementation(std::make_unique<implementation>(
			device,
			file,
			raster_height_pixels,
			atlas_size))
	{ }

	inline font::font(
		const std::filesystem::path& file,
		float raster_height_pixels,
		glm::uvec2 atlas_size)
		: font(gpu::device::global(), file, raster_height_pixels, atlas_size)
	{ }

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

	inline detail::text_glyph font::glyph(uint32_t codepoint, bool rasterize)
	{
		if (!m_implementation)
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
		return {
			page,
			cached.uv_bounds,
			cached.codepoint,
			cached.advance,
			cached.x_offset,
			cached.y_offset,
			cached.width,
			cached.height
		};
	}

	inline float font::kerning(uint32_t left, uint32_t right)
	{
		if (!m_implementation || left == 0 || right == 0)
		{
			return 0.0f;
		}
		const auto left_glyph = glyph(left, false);
		const auto right_glyph = glyph(right, false);
		return static_cast<float>(stbtt_GetCodepointKernAdvance(
			&m_implementation->font_info,
			static_cast<int>(left_glyph.codepoint),
			static_cast<int>(right_glyph.codepoint))) * m_implementation->scale;
	}

	inline SDL_GPUDevice* font::device_raw() const noexcept
	{
		return m_implementation ? m_implementation->device.raw() : nullptr;
	}

	inline void font::upload_pending(gpu::command_buffer& command_buffer)
	{
		if (!m_implementation)
		{
			throw std::logic_error("Cannot use a moved-from font");
		}
		for (auto& page : m_implementation->pages)
		{
			if (page.dirty)
			{
				page.texture.update_pixels(
					command_buffer,
					std::span<const uint8_t>(page.pixels));
				page.dirty = false;
			}
		}
	}

	inline void font::mark_uploads_dirty() noexcept
	{
		if (m_implementation)
		{
			for (auto& page : m_implementation->pages)
			{
				page.dirty = true;
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
			std::string_view line_text = text.substr(
				begin,
				end == std::string_view::npos ? end : end - begin);
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
		if (line_text.find_first_of("\r\n") != std::string_view::npos)
		{
			throw std::invalid_argument("A text document line cannot contain a line break");
		}
		m_lines.at(index).assign(line_text);
	}

	inline void text_document::append_line(std::string_view line_text)
	{
		if (line_text.find_first_of("\r\n") != std::string_view::npos)
		{
			throw std::invalid_argument("A text document line cannot contain a line break");
		}
		m_lines.emplace_back(line_text);
	}

	inline void text_document::erase_line(std::size_t index)
	{
		if (index >= m_lines.size())
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
		inline bool next_utf8_codepoint(
			std::string_view text,
			std::size_t& offset,
			uint32_t& codepoint) noexcept
		{
			if (offset >= text.size())
			{
				return false;
			}

			const std::size_t start = offset;
			const auto first = static_cast<uint8_t>(text[start]);
			auto continuation = [&](std::size_t index)
			{
				return index < text.size() &&
					(static_cast<uint8_t>(text[index]) & 0xC0u) == 0x80u;
			};
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
				codepoint =
					(static_cast<uint32_t>(first & 0x1Fu) << 6) |
					(static_cast<uint8_t>(text[start + 1]) & 0x3Fu);
				offset = start + 2;
				return true;
			}

			if (first >= 0xE0u && first <= 0xEFu)
			{
				if (!continuation(start + 1) || !continuation(start + 2))
				{
					return invalid();
				}
				const auto second = static_cast<uint8_t>(text[start + 1]);
				if ((first == 0xE0u && second < 0xA0u) ||
					(first == 0xEDu && second >= 0xA0u))
				{
					return invalid();
				}
				codepoint =
					(static_cast<uint32_t>(first & 0x0Fu) << 12) |
					(static_cast<uint32_t>(second & 0x3Fu) << 6) |
					(static_cast<uint8_t>(text[start + 2]) & 0x3Fu);
				offset = start + 3;
				return true;
			}

			if (first >= 0xF0u && first <= 0xF4u)
			{
				if (!continuation(start + 1) ||
					!continuation(start + 2) ||
					!continuation(start + 3))
				{
					return invalid();
				}
				const auto second = static_cast<uint8_t>(text[start + 1]);
				if ((first == 0xF0u && second < 0x90u) ||
					(first == 0xF4u && second > 0x8Fu))
				{
					return invalid();
				}
				codepoint =
					(static_cast<uint32_t>(first & 0x07u) << 18) |
					(static_cast<uint32_t>(second & 0x3Fu) << 12) |
					(static_cast<uint32_t>(static_cast<uint8_t>(text[start + 2]) & 0x3Fu) << 6) |
					(static_cast<uint8_t>(text[start + 3]) & 0x3Fu);
				offset = start + 4;
				return true;
			}

			return invalid();
		}
	}
}
