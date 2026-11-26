#pragma once

#include "svg_effect_types.hpp"
#include "../../graphics_pipeline.hpp"
#include "../../paths.hpp"
#include "../../render_pass.hpp"
#include "../../sampler.hpp"
#include "../../shader_loader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mngl::two_d::detail::svg
{
	inline constexpr auto effect_format = gpu::texture_format::r16g16b16a16_float;
	inline constexpr std::array svg_shader_format_preference{
	    static_cast<gpu::shader_format>(gpu::default_shader_format), gpu::shader_format::dxil,
	    gpu::shader_format::spirv, gpu::shader_format::msl, gpu::shader_format::dxbc,
	    gpu::shader_format::metallib};

	[[nodiscard]] inline gpu::shader_format select_svg_shader_format(SDL_GPUShaderFormat supported) noexcept
	{
		for (const auto format : svg_shader_format_preference)
		{
			if ((supported & static_cast<SDL_GPUShaderFormat>(format)) != 0)
				return format;
		}
		return gpu::shader_format::invalid;
	}

	[[nodiscard]] inline gpu::shader_format select_svg_shader_format(const gpu::device& device)
	{
		if (!device.raw()) [[unlikely]]
			throw std::invalid_argument("Cannot select SVG shaders for an empty GPU device");

		const auto supported = device.shader_format();
		const auto selected = select_svg_shader_format(supported);
		if (selected != gpu::shader_format::invalid)
			return selected;

		const char* driver = SDL_GetGPUDeviceDriver(device.raw());
		throw std::runtime_error(std::format(
		    "SVG rendering has no shader format supported by GPU driver '{}' (supported format mask: 0x{:X})",
		    driver ? driver : "<unknown>", static_cast<unsigned int>(supported)));
	}

	[[nodiscard]] inline std::optional<std::filesystem::path> svg_shader_binary_path(
	    const std::filesystem::path& source, gpu::shader_format format)
	{
		auto binary = source;
		switch (format)
		{
		case gpu::shader_format::dxbc:
			binary.replace_extension(".dxbc");
			break;
		case gpu::shader_format::dxil:
			binary.replace_extension(".dxil");
			break;
		case gpu::shader_format::metallib:
			binary.replace_extension(".metallib");
			break;
		case gpu::shader_format::msl:
			binary.replace_extension(".msl");
			break;
		case gpu::shader_format::spirv:
			binary.replace_extension(".spv");
			break;
		default:
			return std::nullopt;
		}
		return binary;
	}

	[[nodiscard]] inline gpu::graphics_shader_asset load_svg_shader(
	    const std::filesystem::path& source, const gpu::shader_compile_options& options)
	{
		if (!gpu::valid_shader_format(options.format)) [[unlikely]]
			throw std::invalid_argument("SVG shader loading requires a valid, selected shader format");

		const auto binary = svg_shader_binary_path(source, options.format);
		if (!binary)
			return gpu::shader_loader::compile(source, options);
		if (std::filesystem::is_regular_file(*binary))
			return gpu::shader_loader::load_binary(*binary, source, options);
		if (options.format == gpu::shader_format::metallib) [[unlikely]]
			throw std::runtime_error(std::format(
			    "GPU device selected Metal library format, but the precompiled SVG shader is missing: '{}'",
			    binary->string()));
		return gpu::shader_loader::compile(source, options);
	}

	class temporary_target_pool_base
	{
	public:
		virtual ~temporary_target_pool_base() = default;
		[[nodiscard]] virtual bool evict_one_idle() noexcept = 0;
	};

	struct temporary_target_budget
	{
		std::size_t byte_limit = 64u * 1024u * 1024u;
		std::size_t target_limit = 64;
		std::size_t cached_bytes = 0;
		std::size_t cached_targets = 0;
		std::vector<temporary_target_pool_base*> pools;

		void register_pool(temporary_target_pool_base* pool) { pools.push_back(pool); }
		void unregister_pool(temporary_target_pool_base* pool) noexcept { std::erase(pools, pool); }

		[[nodiscard]] bool make_room(std::size_t bytes) noexcept
		{
			if (bytes > byte_limit || target_limit == 0)
				return false;
			while (cached_bytes > byte_limit - bytes || cached_targets >= target_limit)
			{
				bool evicted = false;
				for (auto* pool : pools)
				{
					if (pool && pool->evict_one_idle())
					{
						evicted = true;
						break;
					}
				}
				if (!evicted)
					return false;
			}
			return true;
		}
	};

	struct temporary_target_command_state
	{
		std::unordered_set<const void*> retained_targets;
		bool retained_effect_backend = false;
	};

	template <typename Target>
	class temporary_target_pool : public temporary_target_pool_base
	{
	public:
		temporary_target_pool() : temporary_target_pool(std::make_shared<temporary_target_budget>()) {}

		explicit temporary_target_pool(std::shared_ptr<temporary_target_budget> budget)
		    : m_budget(std::move(budget))
		{
			if (!m_budget)
				throw std::invalid_argument("Temporary target pool requires a shared budget");
			m_budget->register_pool(this);
		}

		temporary_target_pool(const temporary_target_pool&) = delete;
		temporary_target_pool& operator=(const temporary_target_pool&) = delete;
		~temporary_target_pool()
		{
			m_budget->unregister_pool(this);
			for (const auto& entry : m_targets)
			{
				m_budget->cached_bytes -= entry.bytes;
				--m_budget->cached_targets;
			}
		}

		template <typename Factory>
		[[nodiscard]] std::shared_ptr<Target> acquire(
		    glm::uvec2 size, Factory&& factory,
		    const std::shared_ptr<temporary_target_command_state>& command_state = {})
		{
			for (const auto& entry : m_targets)
			{
				if (target_size(*entry.target) != size)
					continue;
				const auto in_flight_state = entry.command_state.lock();
				if ((entry.target.use_count() == 1 && !in_flight_state) ||
				    (command_state && in_flight_state == command_state && entry.target.use_count() == 2))
					return entry.target;
			}

			const auto bytes = target_bytes(size);
			if (!m_budget->make_room(bytes))
			{
				throw std::length_error("SVG effect temporary-target budget exceeded");
			}

			auto target = std::forward<Factory>(factory)(size);
			if (!target || target_size(*target) != size)
				throw std::invalid_argument("Temporary target factory returned the wrong size");

			m_targets.push_back({target, bytes, {}});
			m_budget->cached_bytes += bytes;
			++m_budget->cached_targets;
			return target;
		}

		void mark_command_owned(const std::shared_ptr<Target>& target,
		                        const std::shared_ptr<temporary_target_command_state>& command_state)
		{
			if (!target || !command_state)
				throw std::invalid_argument("Temporary target command ownership requires valid resources");
			const auto found = std::ranges::find_if(m_targets, [&](const auto& entry)
			                                       { return entry.target.get() == target.get(); });
			if (found == m_targets.end())
				throw std::logic_error("Command-owned SVG target is not present in its temporary-target pool");
			found->command_state = command_state;
		}

		[[nodiscard]] std::size_t size() const noexcept { return m_targets.size(); }
		[[nodiscard]] std::size_t cached_bytes() const noexcept { return m_budget->cached_bytes; }
		[[nodiscard]] bool evict_one_idle() noexcept override
		{
			const auto idle = std::ranges::find_if(m_targets, [](const auto& entry)
			                                      { return entry.target.use_count() == 1 &&
			                                               entry.command_state.expired(); });
			if (idle == m_targets.end())
				return false;
			m_budget->cached_bytes -= idle->bytes;
			--m_budget->cached_targets;
			m_targets.erase(idle);
			return true;
		}

	private:
		struct entry
		{
			std::shared_ptr<Target> target;
			std::size_t bytes = 0;
			std::weak_ptr<temporary_target_command_state> command_state;
		};

		[[nodiscard]] static glm::uvec2 target_size(const Target& target) noexcept
		{
			if constexpr (requires { target.get_size(); })
				return target.get_size();
			else
				return target.size();
		}

		[[nodiscard]] static std::size_t target_bytes(glm::uvec2 size) noexcept
		{
			constexpr std::size_t bytes_per_pixel = 8;
			const auto width = static_cast<std::size_t>(size.x);
			const auto height = static_cast<std::size_t>(size.y);
			if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height)
				return std::numeric_limits<std::size_t>::max();
			const auto pixels = width * height;
			if (pixels > std::numeric_limits<std::size_t>::max() / bytes_per_pixel)
				return std::numeric_limits<std::size_t>::max();
			return pixels * bytes_per_pixel;
		}

		std::shared_ptr<temporary_target_budget> m_budget;
		std::vector<entry> m_targets;
	};

	class effect_renderer
	{
	public:
		explicit effect_renderer(
		    const gpu::device& device,
		    std::shared_ptr<temporary_target_budget> budget = std::make_shared<temporary_target_budget>())
		    : m_device(device), m_sampler(gpu::sampler_builder{}
			                                  .set_filter(gpu::filter_mode::linear)
			                                  .set_address_mode(gpu::sampler_address_mode::clamp_to_edge)
			                                  .build(device)),
		      m_temporary_targets(std::move(budget))
		{
			gpu::shader_compile_options options;
			options.stage = gpu::graphics_shader_stage::vertex;
			options.format = select_svg_shader_format(device);
			const auto required_vertex = options.format == gpu::shader_format::dxil ? "svg_filter.vert.dxil"
			                                                                          : "svg_filter.vert.hlsl";
			const auto directory = paths::shader_directory(required_vertex);
			const gpu::graphics_shader vertex(device,
			                                  load_svg_shader(directory / "svg_filter.vert.hlsl", options));
			options.stage = gpu::graphics_shader_stage::fragment;
			options.resources.samplers = 2;
			options.resources.uniform_buffers = 1;
			const gpu::graphics_shader fragment(device,
			                                    load_svg_shader(directory / "svg_filter.frag.hlsl", options));
			m_pipeline = gpu::graphics_pipeline_builder{}
			                 .set_debug_name("MNGL SVG filter pipeline")
			                 .set_vertex_shader(vertex)
			                 .set_fragment_shader(fragment)
			                 .add_color_target(effect_format)
			                 .build(device);
		}

		template <typename RetainResource>
		void render(gpu::command_buffer& command, gpu::texture& output, gpu::texture& source,
		            const gpu::texture* mask, rectangle region, const std::optional<mask_data>& mask_style,
		            const std::optional<filter_data>& filter, bool premultiplied,
		            const std::shared_ptr<temporary_target_command_state>& command_state,
		            RetainResource&& retain_resource)
		{
			using texture_ptr = std::shared_ptr<gpu::texture>;
			const auto size = output.get_size();
			const auto acquire = [&]
			{
				return m_temporary_targets.acquire(size, [&](glm::uvec2 target_size)
				                                    {
					                                    return std::make_shared<gpu::texture>(
					                                        m_device, target_size, effect_format,
					                                        gpu::texture::usage_flags::sampler |
					                                            gpu::texture::usage_flags::color_target);
					                                    }, command_state);
			};
			const auto pass = [&](const gpu::texture& a, const gpu::texture& b, const parameters& values)
			{
				auto target = acquire();
				// Render-pass bindings only carry raw SDL handles. The command retains each
				// target once, while the pool tracks that in-flight owner separately from CPU leases.
				retain_resource(target);
				m_temporary_targets.mark_command_owned(target, command_state);
				draw(command, *target, a, b, values);
				return target;
			};
			const auto pass_single = [&](const gpu::texture& input, filter_op operation)
			{
				return pass(input, input,
				            parameters{.control = {static_cast<std::uint32_t>(operation), 0, 0, 0}});
			};
			const auto offset_uv = [&](glm::vec2 offset) { return offset / region.size * glm::vec2(1, -1); };
			const auto blur = [&](texture_ptr input, glm::vec2 sigma, std::uint32_t edge_mode,
			                      rectangle input_region)
			{
				const auto input_uv = intersection_uv(region, input_region);
				for (int axis = 0; axis < 2; ++axis)
				{
					const double pixels = static_cast<double>(sigma[axis]) * size[axis] / region.size[axis];
					if (pixels < 0.25)
						continue;
					const double stride = std::max(1.0, pixels * (3.0 / 32.0));
					glm::vec2 uv_step(0);
					uv_step[axis] = static_cast<float>(std::min(2.0, stride / size[axis]));
					auto uniforms = parameters{
					    .control = {static_cast<std::uint32_t>(filter_op::blur), edge_mode,
					                static_cast<unsigned>(std::min(32.0, std::ceil(3.0 * pixels / stride))),
					                static_cast<std::uint32_t>(axis)},
					    .values = {uv_step, static_cast<float>(pixels / stride), 0}};
					// The horizontal pass can produce values across the filter region. The
					// vertical pass must keep those values while applying edgeMode only at
					// the original input's vertical boundary.
					uniforms.rows[0] = axis == 0 ? input_uv : glm::vec4(0, input_uv.y, 1, input_uv.w);
					input = pass(*input, *input, uniforms);
				}
				return input;
			};
			const auto clip = [&](parameters& uniforms, rectangle bounds)
			{
				const auto clipped = intersection_uv(region, bounds);
				uniforms.clip_uv = clipped;
			};

			texture_ptr image(texture_ptr{}, &source); // Borrow the source target for this submission.
			if (filter && !filter->steps.empty())
			{
				const auto& steps = filter->steps;
				auto dependencies = analyze_filter_dependencies(steps);
				const auto input_region = [&](std::size_t index)
				{
					if (index < 2)
						return filter->region;
					const auto producer = index - 2;
					return producer < steps.size() ? steps[producer].region.value_or(filter->region)
					                              : filter->region;
				};
				auto& consumers = dependencies.consumers;
				std::vector<texture_ptr> outputs(steps.size() + 2);
				std::vector<bool> output_linear(steps.size() + 2, false);
				outputs[0] = image;
				if (consumers[1] != 0)
					outputs[1] = pass_single(source, filter_op::source_alpha);
				if (consumers[0] == 0)
					outputs[0].reset();

				for (std::size_t i = 0; i < steps.size(); ++i)
				{
					if (!dependencies.live_outputs[i + 2])
						continue;
					const auto& step = steps[i];
					const bool uses_primary = filter_uses_primary_input(step.operation);
					const bool uses_secondary = filter_uses_secondary_input(step.operation);
					texture_ptr converted_input;
					const texture_ptr* input = uses_primary ? &outputs[step.input] : &image;
					if (uses_primary && output_linear[step.input] != step.linear_rgb)
					{
						converted_input = pass_single(**input, step.linear_rgb ? filter_op::to_linear : filter_op::to_srgb);
						input = &converted_input;
					}
					texture_ptr converted_other;
					const texture_ptr* other = input;
					if (uses_secondary)
					{
						other = &outputs[step.input2];
						if (output_linear[step.input2] != step.linear_rgb)
						{
							converted_other = pass_single(**other, step.linear_rgb ? filter_op::to_linear
							                                                        : filter_op::to_srgb);
							other = &converted_other;
						}
					}
					parameters uniforms{
					    .control = {static_cast<std::uint32_t>(step.operation), step.mode, 0, 0},
					    .values = step.parameters,
					    .rows = step.matrix};
					if (step.region)
						clip(uniforms, *step.region);
					texture_ptr current;
					switch (step.operation)
					{
					case filter_op::flood:
						current = pass(*image, *image, uniforms);
						break;
					case filter_op::blur:
						current = blur(*input, glm::vec2(step.parameters), step.mode,
						               input_region(step.input));
						if (step.region)
						{
							uniforms.control.x = static_cast<std::uint32_t>(filter_op::copy);
							current = pass(*current, *current, uniforms);
						}
						break;
					case filter_op::shadow:
					{
						auto blurred = blur(*input, glm::vec2(step.parameters), 0,
						                    input_region(step.input));
						const auto offset = offset_uv({step.parameters.z, step.parameters.w});
						uniforms.values = {offset, 0, 0};
						current = pass(**input, *blurred, uniforms);
						break;
					}
					case filter_op::offset:
						uniforms.values = {offset_uv(glm::vec2(step.parameters)), 0, 0};
						[[fallthrough]];
					default:
						current = pass(**input, **other, uniforms);
						break;
					}
					if (uses_primary && --consumers[step.input] == 0)
						outputs[step.input].reset();
					if (uses_secondary && --consumers[step.input2] == 0)
						outputs[step.input2].reset();
					if (consumers[i + 2] != 0)
						outputs[i + 2] = std::move(current);
					output_linear[i + 2] = step.linear_rgb;
				}
				image = outputs.back();
				if (output_linear.back())
					image = pass_single(*image, filter_op::to_srgb);
			}
			if (mask_style)
			{
				if (!mask)
				{
					gpu::render_pass clear(command, output, color(0, 0, 0, 0));
					return;
				}
				parameters uniforms{.control = {static_cast<std::uint32_t>(filter_op::mask),
				                                static_cast<std::uint32_t>(mask_style->channel), 0, 0}};
				clip(uniforms, mask_style->region);
				image = pass(*image, *mask, uniforms);
			}
			draw(command, output, *image, *image,
			     parameters{.control = {static_cast<std::uint32_t>(filter_op::output),
			                            premultiplied ? 1u : 0u, 0, 0}});
		}

	private:
		struct parameters
		{
			glm::uvec4 control{};
			glm::vec4 values{};
			glm::vec4 clip_uv{0, 0, 1, 1};
			std::array<glm::vec4, 5> rows{};
		};

		[[nodiscard]] static glm::vec4 intersection_uv(rectangle region, rectangle bounds)
		{
			const auto start = glm::max(region.pos, bounds.pos);
			const auto end = glm::min(region.pos + region.size, bounds.pos + bounds.size);
			if (end.x <= start.x || end.y <= start.y)
				return {1, 1, 0, 0};
			const auto a = (start - region.pos) / region.size;
			const auto b = (end - region.pos) / region.size;
			return {a.x, 1 - b.y, b.x, 1 - a.y};
		}

		void draw(gpu::command_buffer& command, gpu::texture& output, const gpu::texture& a,
		          const gpu::texture& b, const parameters& values)
		{
			static_assert(sizeof(parameters) == 128);
			gpu::render_pass pass(command, output, color(0, 0, 0, 0));
			pass.bind_pipeline(m_pipeline);
			const std::array<SDL_GPUTextureSamplerBinding, 2> bindings{
			    {{a.raw(), m_sampler.raw()}, {b.raw(), m_sampler.raw()}}};
			pass.bind_fragment_samplers(0, bindings);
			SDL_PushGPUFragmentUniformData(command.raw(), 0, &values, sizeof(values));
			pass.draw_primitives(3);
		}

		gpu::device m_device;
		gpu::sampler m_sampler;
		gpu::graphics_pipeline m_pipeline;
		temporary_target_pool<gpu::texture> m_temporary_targets;
	};
} // namespace mngl::two_d::detail::svg
