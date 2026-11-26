#pragma once

#include "svg_object.hpp"

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mngl::two_d
{
	// A positioned GPU instance that shares the resident geometry of an uploaded svg_object.
	class svg_sprite : public ::mngl::drawable
	{
	public:
		explicit svg_sprite(const svg_object& source)
		    : m_gpu_assets(source.m_gpu_state ? source.m_gpu_state->assets : nullptr),
		      m_hit_regions(source.m_hit_regions), m_source_size(source.size()),
		      m_transform(source.m_transform), m_tint(source.m_tint), m_blend(source.m_blend),
		      m_click_callback(source.m_click_callback)
		{
			if (!source.loaded()) [[unlikely]]
			{
				throw std::logic_error("Cannot create an SVG sprite from an unloaded SVG object");
			}
			if (!source.gpu_resident()) [[unlikely]]
			{
				if (source.m_gpu_state && source.m_gpu_state->pending_uploads != 0)
					throw std::logic_error(
					    "Submit the SVG upload command buffer before creating an SVG sprite");
				throw std::logic_error("Upload an SVG object before creating an SVG sprite");
			}
			m_destination = rectangle{glm::vec2(0.0f), m_source_size};
			m_geometry.reserve(source.m_gpu_state->geometry.size());
			for (const auto& part : source.m_gpu_state->geometry)
			{
				mesh instance;
				instance.m_storage = part.gpu_geometry.m_storage;
				if (!instance.m_storage && !part.image && !part.effect) [[unlikely]]
				{
					throw std::logic_error("Cannot create an SVG sprite from a moved-from GPU mesh");
				}
				m_geometry.push_back({std::move(instance), part.svg_transform, part.svg_tint,
				                      part.svg_gradient, part.image, part.effect});
			}
			update_layout(m_destination, m_pivot, m_rotation_radians, m_transform);
			update_paint();
		}

		[[nodiscard]] static svg_sprite from_file(const gpu::device& device,
		                                          const std::filesystem::path& path,
		                                          const svg_load_options& options = {})
		{
			auto source = svg_object::from_file(path, options);
			source.upload(device);
			return svg_sprite{source};
		}

		svg_sprite(const svg_sprite&) = delete;
		svg_sprite& operator=(const svg_sprite&) = delete;
		svg_sprite(svg_sprite&&) noexcept = default;
		svg_sprite& operator=(svg_sprite&&) noexcept = default;
		~svg_sprite() override = default;

		[[nodiscard]] bool empty() const noexcept { return m_geometry.empty(); }
		[[nodiscard]] std::size_t geometry_count() const noexcept { return m_geometry.size(); }
		[[nodiscard]] glm::vec2 intrinsic_size() const noexcept { return m_source_size; }

		[[nodiscard]] glm::vec2 position() const noexcept { return m_destination.pos; }
		svg_sprite& position(glm::vec2 value)
		{
			auto destination = m_destination;
			destination.pos = value;
			update_layout(destination, m_pivot, m_rotation_radians, m_transform);
			return *this;
		}

		[[nodiscard]] glm::vec2 size() const noexcept { return m_destination.size; }
		svg_sprite& size(glm::vec2 value)
		{
			auto destination = m_destination;
			destination.size = value;
			update_layout(destination, m_pivot, m_rotation_radians, m_transform);
			return *this;
		}

		[[nodiscard]] float rotation() const noexcept { return m_rotation_radians; }
		svg_sprite& rotation(float value)
		{
			update_layout(m_destination, m_pivot, value, m_transform);
			return *this;
		}

		[[nodiscard]] glm::vec2 pivot() const noexcept { return m_pivot; }
		svg_sprite& pivot(glm::vec2 value)
		{
			update_layout(m_destination, value, m_rotation_radians, m_transform);
			return *this;
		}

		[[nodiscard]] const ::mngl::two_d::transform& transform() const noexcept { return m_transform; }
		svg_sprite& transform(const ::mngl::two_d::transform& value)
		{
			update_layout(m_destination, m_pivot, m_rotation_radians, value);
			return *this;
		}

		[[nodiscard]] const ::mngl::color& tint() const noexcept { return m_tint; }
		svg_sprite& tint(::mngl::color value)
		{
			m_tint = value;
			update_paint();
			return *this;
		}

		[[nodiscard]] gpu::sprite_blend_mode blend() const noexcept { return m_blend; }
		svg_sprite& blend(gpu::sprite_blend_mode value)
		{
			m_blend = value;
			update_paint();
			return *this;
		}

		void on_click(::mngl::hit_test_callback callback)
		{
			::mngl::detail::set_hit_test_callback(m_click_callback, std::move(callback));
		}

		void clear_on_click() noexcept { m_click_callback.reset(); }

	private:
		struct geometry_part
		{
			mesh gpu_geometry;
			glm::mat4 svg_transform{1.0f};
			::mngl::color svg_tint{1.0f};
			std::optional<detail::svg::gradient_paint> svg_gradient;
			std::optional<detail::svg::image_paint> image;
			std::shared_ptr<const detail::svg::effect_layer> effect;
		};

		void compose_transforms(
		    const rectangle& destination, glm::vec2 pivot, float rotation_radians,
		    const ::mngl::two_d::transform& object_transform)
		{
			if (!std::isfinite(destination.pos.x) || !std::isfinite(destination.pos.y) ||
			    !std::isfinite(destination.size.x) || !std::isfinite(destination.size.y) ||
			    destination.size.x < 0.0f || destination.size.y < 0.0f || !std::isfinite(pivot.x) ||
			    !std::isfinite(pivot.y) || !std::isfinite(rotation_radians)) [[unlikely]]
			{
				throw std::invalid_argument(
				    "SVG sprite transform values must be finite with non-negative dimensions");
			}

			const glm::mat4 placement = placement_transform(destination, pivot, rotation_radians);
			const glm::mat4 object_matrix = object_transform.matrix();
			for (auto& part : m_geometry)
			{
				part.gpu_geometry.transform.matrix(object_matrix * placement * part.svg_transform);
			}
		}

		[[nodiscard]] glm::mat4 placement_transform(const rectangle& destination, glm::vec2 pivot,
		                                            float rotation_radians) const
		{
			const glm::vec2 scale_factors{
			    m_source_size.x > 0.0f ? destination.size.x / m_source_size.x : 0.0f,
			    m_source_size.y > 0.0f ? destination.size.y / m_source_size.y : 0.0f};
			const glm::vec2 pivot_offset = pivot * destination.size;
			glm::mat4 placement(1.0f);
			placement = glm::translate(placement, glm::vec3(destination.pos + pivot_offset, 0.0f));
			placement = glm::rotate(placement, rotation_radians, glm::vec3(0.0f, 0.0f, 1.0f));
			placement = glm::translate(placement, glm::vec3(-pivot_offset, 0.0f));
			return glm::scale(placement, glm::vec3(scale_factors, 1.0f));
		}

		void update_layout(const rectangle& destination, glm::vec2 pivot, float rotation_radians,
		                   const ::mngl::two_d::transform& object_transform)
		{
			compose_transforms(destination, pivot, rotation_radians, object_transform);
			m_destination = destination;
			m_pivot = pivot;
			m_rotation_radians = rotation_radians;
			m_transform = object_transform;
		}

		void update_paint() noexcept
		{
			for (auto& part : m_geometry)
			{
				part.gpu_geometry.tint = part.svg_tint * m_tint;
				part.gpu_geometry.blend = m_blend;
			}
		}

		void draw(frame_2d& frame) const override
		{
			if (!m_hit_regions.empty())
			{
				const glm::mat4 placement = placement_transform(m_destination, m_pivot, m_rotation_radians);
				for (const auto& region : m_hit_regions)
				{
					frame.record_svg_hit_geometry(
					    region,
					    ::mngl::two_d::transform(m_transform.matrix() * placement * region->svg_transform),
					    m_click_callback);
				}
			}
			for (const auto& part : m_geometry)
			{
				if (part.effect)
				{
					frame.submit_svg_effect(part.effect, part.gpu_geometry.transform, part.gpu_geometry.tint,
					                        m_blend, m_gpu_assets);
				}
				else if (part.image)
				{
					frame.submit_svg_image(*part.image, part.gpu_geometry.transform, part.gpu_geometry.tint,
					                       m_blend, m_gpu_assets);
				}
				else if (part.svg_gradient)
				{
					frame.submit_svg_gradient_mesh(part.gpu_geometry, *part.svg_gradient);
				}
				else
				{
					frame.draw(part.gpu_geometry);
				}
			}
		}

		void prepare(frame_2d& frame) const override
		{
			for (const auto& part : m_geometry)
			{
				if (part.svg_gradient)
				{
					frame.prepare_svg_gradient(m_blend, *part.svg_gradient);
				}
			}
		}

		std::vector<geometry_part> m_geometry;
		std::shared_ptr<const detail::svg::uploaded_assets> m_gpu_assets;
		std::vector<std::shared_ptr<const ::mngl::detail::svg_hit_test_geometry>> m_hit_regions;
		glm::vec2 m_source_size{};
		rectangle m_destination{};
		glm::vec2 m_pivot{0.5f, 0.5f};
		float m_rotation_radians = 0.0f;
		::mngl::two_d::transform m_transform{};
		::mngl::color m_tint{1.0f};
		gpu::sprite_blend_mode m_blend = gpu::sprite_blend_mode::alpha;
		std::shared_ptr<const ::mngl::hit_test_callback> m_click_callback;
	};
} // namespace mngl::two_d
