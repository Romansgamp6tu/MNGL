#pragma once

#include "gpu_buffer.hpp"
#include "texture.hpp"

#include <algorithm>
#include <concepts>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mngl::gpu
{
	class frame_graph
	{
	private:
		using drawable_texture = ::mngl::texture;
		using gpu_texture = ::mngl::gpu::texture;
		using gpu_buffer = ::mngl::gpu::buffer;
		constexpr static uint32_t invalid_index = std::numeric_limits<uint32_t>::max();

	public:
		enum class access
		{
			read,
			write,
			read_write
		};

		struct texture_handle
		{
			[[nodiscard]] bool valid() const noexcept
			{
				return m_owner != nullptr && m_index != invalid_index;
			}

		private:
			friend class frame_graph;
			texture_handle(const frame_graph* owner, uint32_t index) noexcept
				: m_owner(owner), m_index(index)
			{ }

			const frame_graph* m_owner = nullptr;
			uint32_t m_index = invalid_index;
		};

		struct buffer_handle
		{
			[[nodiscard]] bool valid() const noexcept
			{
				return m_owner != nullptr && m_index != invalid_index;
			}

		private:
			friend class frame_graph;
			buffer_handle(const frame_graph* owner, uint32_t index) noexcept
				: m_owner(owner), m_index(index)
			{ }

			const frame_graph* m_owner = nullptr;
			uint32_t m_index = invalid_index;
		};

		struct pass_handle
		{
			[[nodiscard]] bool valid() const noexcept
			{
				return m_owner != nullptr && m_index != invalid_index;
			}

		private:
			friend class frame_graph;
			pass_handle(const frame_graph* owner, uint32_t index) noexcept
				: m_owner(owner), m_index(index)
			{ }

			const frame_graph* m_owner = nullptr;
			uint32_t m_index = invalid_index;
		};

		class pass_context
		{
		public:
			[[nodiscard]] command_buffer& commands() const noexcept { return *m_commands; }

			[[nodiscard]] ::mngl::texture& texture(texture_handle handle) const
			{
				m_graph->validate_context_resource(m_pass_index, handle);
				return m_graph->texture(handle);
			}

			[[nodiscard]] ::mngl::gpu::texture& gpu_texture_resource(
				texture_handle handle
			) const
			{
				m_graph->validate_context_resource(m_pass_index, handle);
				return m_graph->gpu_texture_resource(handle);
			}

			[[nodiscard]] ::mngl::gpu::buffer& buffer(buffer_handle handle) const
			{
				m_graph->validate_context_resource(m_pass_index, handle);
				return m_graph->buffer(handle);
			}

		private:
			friend class frame_graph;
			pass_context(
				frame_graph& graph,
				std::size_t pass_index,
				command_buffer& commands
			) noexcept
				: m_graph(&graph), m_pass_index(pass_index), m_commands(&commands)
			{ }

			frame_graph* m_graph;
			std::size_t m_pass_index;
			command_buffer* m_commands;
		};

		class pass_builder
		{
		public:
			[[nodiscard]] pass_handle handle() const noexcept { return m_handle; }

			pass_builder& read(texture_handle resource)
			{
				m_graph->add_resource_use(m_handle, resource, access::read);
				return *this;
			}

			pass_builder& write(texture_handle resource)
			{
				m_graph->add_resource_use(m_handle, resource, access::write);
				return *this;
			}

			pass_builder& read_write(texture_handle resource)
			{
				m_graph->add_resource_use(m_handle, resource, access::read_write);
				return *this;
			}

			pass_builder& read(buffer_handle resource)
			{
				m_graph->add_resource_use(m_handle, resource, access::read);
				return *this;
			}

			pass_builder& write(buffer_handle resource)
			{
				m_graph->add_resource_use(m_handle, resource, access::write);
				return *this;
			}

			pass_builder& read_write(buffer_handle resource)
			{
				m_graph->add_resource_use(m_handle, resource, access::read_write);
				return *this;
			}

			pass_builder& depends_on(pass_handle prerequisite)
			{
				m_graph->add_dependency(m_handle, prerequisite);
				return *this;
			}

		private:
			friend class frame_graph;
			pass_builder(frame_graph& graph, pass_handle handle) noexcept
				: m_graph(&graph), m_handle(handle)
			{ }

			frame_graph* m_graph;
			pass_handle m_handle;
		};

		explicit frame_graph(const device& target_device = device::global())
			: m_device(target_device)
		{
			if (!m_device.raw())
			{
				throw std::invalid_argument("A frame graph requires a valid GPU device");
			}
		}

		frame_graph(const frame_graph&) = delete;
		frame_graph& operator=(const frame_graph&) = delete;
		frame_graph(frame_graph&&) = delete;
		frame_graph& operator=(frame_graph&&) = delete;

		[[nodiscard]] texture_handle create_texture(
			glm::uvec2 size,
			texture_format format,
			uint32_t usage
		)
		{
			ensure_mutable();
			auto value = std::make_unique<drawable_texture>(
				m_device,
				size.x,
				size.y,
				format,
				usage);

			resource_entry resource;
			resource.kind = resource_kind::texture;
			resource.high_level_texture = value.get();
			resource.raw_texture = &value->raw_gpu_texture();
			resource.owned_texture = std::move(value);
			return texture_handle(this, add_resource(std::move(resource)));
		}

		[[nodiscard]] buffer_handle create_buffer(uint32_t size, buffer_usage usage)
		{
			ensure_mutable();
			if (size == 0)
			{
				throw std::invalid_argument("A frame-graph buffer must have a non-zero size");
			}

			auto value = std::make_unique<gpu_buffer>(m_device.raw(), size, usage);
			resource_entry resource;
			resource.kind = resource_kind::buffer;
			resource.raw_buffer = value.get();
			resource.owned_buffer = std::move(value);
			return buffer_handle(this, add_resource(std::move(resource)));
		}

		[[nodiscard]] texture_handle import_texture(drawable_texture& value)
		{
			if (!value.valid())
			{
				throw std::invalid_argument("Cannot import an invalid texture into a frame graph");
			}
			return import_texture(value.raw_gpu_texture(), &value);
		}

		[[nodiscard]] texture_handle import_texture(gpu_texture& value)
		{
			return import_texture(value, nullptr);
		}

		[[nodiscard]] buffer_handle import_buffer(gpu_buffer& value)
		{
			ensure_mutable();
			if (!value.valid())
			{
				throw std::invalid_argument("Cannot import an invalid buffer into a frame graph");
			}
			if (value.device_raw() != m_device.raw())
			{
				throw std::invalid_argument(
					"Imported frame-graph buffer belongs to a different GPU device");
			}

			for (uint32_t index = 0; index < m_resources.size(); ++index)
			{
				auto& resource = m_resources[index];
				if (resource.kind == resource_kind::buffer &&
					resource.raw_buffer == &value)
				{
					return buffer_handle(this, index);
				}
			}

			resource_entry resource;
			resource.kind = resource_kind::buffer;
			resource.initially_initialized = true;
			resource.raw_buffer = &value;
			return buffer_handle(this, add_resource(std::move(resource)));
		}

		[[nodiscard]] ::mngl::texture& texture(texture_handle handle)
		{
			auto& resource = checked_resource(handle, resource_kind::texture);
			if (!resource.high_level_texture)
			{
				throw std::logic_error(
					"Imported low-level GPU textures are available through gpu_texture_resource()");
			}
			return *resource.high_level_texture;
		}

		[[nodiscard]] const ::mngl::texture& texture(texture_handle handle) const
		{
			const auto& resource = checked_resource(handle, resource_kind::texture);
			if (!resource.high_level_texture)
			{
				throw std::logic_error(
					"Imported low-level GPU textures are available through gpu_texture_resource()");
			}
			return *resource.high_level_texture;
		}

		[[nodiscard]] ::mngl::gpu::texture& gpu_texture_resource(texture_handle handle)
		{
			auto& resource = checked_resource(handle, resource_kind::texture);
			return *resource.raw_texture;
		}

		[[nodiscard]] const ::mngl::gpu::texture& gpu_texture_resource(
			texture_handle handle
		) const
		{
			const auto& resource = checked_resource(handle, resource_kind::texture);
			return *resource.raw_texture;
		}

		[[nodiscard]] ::mngl::gpu::buffer& buffer(buffer_handle handle)
		{
			auto& resource = checked_resource(handle, resource_kind::buffer);
			return *resource.raw_buffer;
		}

		[[nodiscard]] const ::mngl::gpu::buffer& buffer(buffer_handle handle) const
		{
			const auto& resource = checked_resource(handle, resource_kind::buffer);
			return *resource.raw_buffer;
		}

		template<typename Callback>
			requires std::invocable<Callback&, pass_context&> &&
				std::copy_constructible<std::decay_t<Callback>>
		[[nodiscard]] pass_builder add_pass(
			std::string_view name,
			Callback&& callback
		)
		{
			ensure_mutable();
			pass_node pass;
			pass.name = checked_name(name);
			pass.callback = std::forward<Callback>(callback);
			return add_pass_node(std::move(pass));
		}

		void execute()
		{
			if (m_executing)
			{
				throw std::logic_error("A frame graph cannot execute recursively");
			}
			if (m_execution_failed)
			{
				throw std::logic_error(
					"A frame graph cannot be reused after a pass failed during execution");
			}

			const auto order = build_execution_order();
			m_executing = true;
			try
			{
				execute_passes(order);
				m_executing = false;
			}
			catch (...)
			{
				m_executing = false;
				m_execution_failed = true;
				throw;
			}
		}

	private:
		enum class resource_kind
		{
			texture,
			buffer
		};

		struct resource_entry
		{
			resource_kind kind = resource_kind::texture;
			bool initially_initialized = false;
			std::unique_ptr<drawable_texture> owned_texture;
			drawable_texture* high_level_texture = nullptr;
			gpu_texture* raw_texture = nullptr;
			std::unique_ptr<gpu_buffer> owned_buffer;
			gpu_buffer* raw_buffer = nullptr;
		};

		struct resource_use
		{
			uint32_t resource_index;
			access mode;
		};

		struct pass_node
		{
			std::string name;
			std::function<void(pass_context&)> callback;
			std::vector<resource_use> resources;
			std::vector<uint32_t> prerequisites;
		};

		[[nodiscard]] static std::string checked_name(std::string_view name)
		{
			if (name.empty())
			{
				throw std::invalid_argument("A frame-graph pass requires a name");
			}
			return std::string(name);
		}

		void ensure_mutable() const
		{
			if (m_executing)
			{
				throw std::logic_error("A frame graph cannot be modified while it is executing");
			}
			if (m_execution_failed)
			{
				throw std::logic_error(
					"A frame graph cannot be modified after a pass failed during execution");
			}
		}

		uint32_t add_resource(resource_entry resource)
		{
			if (m_resources.size() >= invalid_index)
			{
				throw std::length_error("Frame graph exhausted its resource handle space");
			}
			const auto index = static_cast<uint32_t>(m_resources.size());
			m_resources.push_back(std::move(resource));
			return index;
		}

		[[nodiscard]] texture_handle import_texture(
			gpu_texture& value,
			drawable_texture* high_level
		)
		{
			ensure_mutable();
			if (!value.valid())
			{
				throw std::invalid_argument("Cannot import an invalid texture into a frame graph");
			}
			if (value.device_raw() != m_device.raw())
			{
				throw std::invalid_argument(
					"Imported frame-graph texture belongs to a different GPU device");
			}

			for (uint32_t index = 0; index < m_resources.size(); ++index)
			{
				auto& resource = m_resources[index];
				if (resource.kind == resource_kind::texture &&
					resource.raw_texture == &value)
				{
					if (!resource.high_level_texture && high_level)
					{
						resource.high_level_texture = high_level;
					}
					return texture_handle(this, index);
				}
			}

			resource_entry resource;
			resource.kind = resource_kind::texture;
			resource.initially_initialized = true;
			resource.high_level_texture = high_level;
			resource.raw_texture = &value;
			return texture_handle(this, add_resource(std::move(resource)));
		}

		[[nodiscard]] pass_builder add_pass_node(pass_node pass)
		{
			if (m_passes.size() >= invalid_index)
			{
				throw std::length_error("Frame graph exhausted its pass handle space");
			}
			const auto index = static_cast<uint32_t>(m_passes.size());
			m_passes.push_back(std::move(pass));
			return pass_builder(*this, pass_handle(this, index));
		}

		uint32_t resource_index(texture_handle handle) const
		{
			if (handle.m_owner != this ||
				handle.m_index >= m_resources.size() ||
				m_resources[handle.m_index].kind != resource_kind::texture)
			{
				throw std::invalid_argument(
					"Frame-graph texture handle is invalid or belongs to another graph");
			}
			return handle.m_index;
		}

		uint32_t resource_index(buffer_handle handle) const
		{
			if (handle.m_owner != this ||
				handle.m_index >= m_resources.size() ||
				m_resources[handle.m_index].kind != resource_kind::buffer)
			{
				throw std::invalid_argument(
					"Frame-graph buffer handle is invalid or belongs to another graph");
			}
			return handle.m_index;
		}

		uint32_t pass_index(pass_handle handle) const
		{
			if (handle.m_owner != this || handle.m_index >= m_passes.size())
			{
				throw std::invalid_argument(
					"Frame-graph pass handle is invalid or belongs to another graph");
			}
			return handle.m_index;
		}

		resource_entry& checked_resource(texture_handle handle)
		{
			return m_resources[resource_index(handle)];
		}

		const resource_entry& checked_resource(texture_handle handle) const
		{
			return m_resources[resource_index(handle)];
		}

		resource_entry& checked_resource(buffer_handle handle)
		{
			return m_resources[resource_index(handle)];
		}

		const resource_entry& checked_resource(buffer_handle handle) const
		{
			return m_resources[resource_index(handle)];
		}

		resource_entry& checked_resource(texture_handle handle, resource_kind kind)
		{
			auto& resource = checked_resource(handle);
			if (resource.kind != kind)
			{
				throw std::invalid_argument("Frame-graph resource handle has the wrong type");
			}
			return resource;
		}

		const resource_entry& checked_resource(
			texture_handle handle,
			resource_kind kind
		) const
		{
			const auto& resource = checked_resource(handle);
			if (resource.kind != kind)
			{
				throw std::invalid_argument("Frame-graph resource handle has the wrong type");
			}
			return resource;
		}

		resource_entry& checked_resource(buffer_handle handle, resource_kind kind)
		{
			auto& resource = checked_resource(handle);
			if (resource.kind != kind)
			{
				throw std::invalid_argument("Frame-graph resource handle has the wrong type");
			}
			return resource;
		}

		const resource_entry& checked_resource(
			buffer_handle handle,
			resource_kind kind
		) const
		{
			const auto& resource = checked_resource(handle);
			if (resource.kind != kind)
			{
				throw std::invalid_argument("Frame-graph resource handle has the wrong type");
			}
			return resource;
		}

		void add_resource_use(
			pass_handle pass,
			texture_handle resource,
			access mode
		)
		{
			add_resource_use(pass_index(pass), resource_index(resource), mode);
		}

		void add_resource_use(
			pass_handle pass,
			buffer_handle resource,
			access mode
		)
		{
			add_resource_use(pass_index(pass), resource_index(resource), mode);
		}

		void add_resource_use(
			uint32_t pass,
			uint32_t resource,
			access mode
		)
		{
			ensure_mutable();
			auto& uses = m_passes[pass].resources;
			const auto existing = std::find_if(
				uses.begin(),
				uses.end(),
				[resource](const resource_use& use)
				{
					return use.resource_index == resource;
				});
			if (existing != uses.end())
			{
				if (existing->mode != mode)
				{
					throw std::invalid_argument(
						"Declare a frame-graph resource as read_write instead of adding separate read and write uses");
				}
				return;
			}
			uses.push_back({ resource, mode });
		}

		void add_dependency(pass_handle pass, pass_handle prerequisite)
		{
			ensure_mutable();
			const auto pass_id = pass_index(pass);
			const auto prerequisite_id = pass_index(prerequisite);
			auto& prerequisites = m_passes[pass_id].prerequisites;
			if (std::find(
					prerequisites.begin(),
					prerequisites.end(),
					prerequisite_id) == prerequisites.end())
			{
				prerequisites.push_back(prerequisite_id);
			}
		}

		void validate_context_resource(
			std::size_t pass,
			texture_handle handle
		) const
		{
			const auto resource = resource_index(handle);
			validate_context_resource(pass, resource);
		}

		void validate_context_resource(
			std::size_t pass,
			buffer_handle handle
		) const
		{
			const auto resource = resource_index(handle);
			validate_context_resource(pass, resource);
		}

		void validate_context_resource(
			std::size_t pass,
			uint32_t resource
		) const
		{
			if (pass >= m_passes.size())
			{
				throw std::logic_error("Frame-graph pass context is no longer valid");
			}
			const auto& uses = m_passes[pass].resources;
			if (std::none_of(
					uses.begin(),
					uses.end(),
					[resource](const resource_use& use)
					{
						return use.resource_index == resource;
					}))
			{
				throw std::logic_error(
					"Frame-graph pass accessed a resource it did not declare");
			}
		}

		[[nodiscard]] std::vector<std::size_t> build_execution_order() const
		{
			const std::size_t pass_count = m_passes.size();
			std::vector<std::vector<std::size_t>> outgoing(pass_count);
			std::vector<std::size_t> indegree(pass_count, 0);
			auto add_edge = [&](std::size_t prerequisite, std::size_t dependent)
			{
				auto& successors = outgoing[prerequisite];
				if (std::find(successors.begin(), successors.end(), dependent) ==
					successors.end())
				{
					successors.push_back(dependent);
					++indegree[dependent];
				}
			};

			for (std::size_t pass = 0; pass < pass_count; ++pass)
			{
				for (const auto prerequisite : m_passes[pass].prerequisites)
				{
					add_edge(prerequisite, pass);
				}
			}

			for (uint32_t resource_index = 0;
				 resource_index < m_resources.size();
				 ++resource_index)
			{
				const auto& resource = m_resources[resource_index];
				std::optional<std::size_t> first_writer;
				for (std::size_t pass = 0; pass < pass_count && !first_writer; ++pass)
				{
					for (const auto& use : m_passes[pass].resources)
					{
						if (use.resource_index == resource_index &&
							use.mode != access::read)
						{
							first_writer = pass;
							break;
						}
					}
				}

				if (!resource.initially_initialized && !first_writer)
				{
					for (std::size_t pass = 0; pass < pass_count; ++pass)
					{
						for (const auto& use : m_passes[pass].resources)
						{
							if (use.resource_index == resource_index &&
								use.mode != access::write)
							{
								throw std::logic_error(
									"Transient frame-graph resource " +
									std::to_string(resource_index) +
									" is read but has no producing pass");
							}
						}
					}
				}

				std::optional<std::size_t> last_writer;
				std::vector<std::size_t> readers;
				std::vector<std::size_t> readers_before_first_write;
				for (std::size_t pass = 0; pass < pass_count; ++pass)
				{
					const auto use = std::find_if(
						m_passes[pass].resources.begin(),
						m_passes[pass].resources.end(),
						[resource_index](const resource_use& item)
						{
							return item.resource_index == resource_index;
						});
					if (use == m_passes[pass].resources.end())
					{
						continue;
					}

					const bool reads = use->mode != access::write;
					const bool writes = use->mode != access::read;
					if (reads)
					{
						if (last_writer)
						{
							add_edge(*last_writer, pass);
						}
						else if (resource.initially_initialized)
						{
							if (use->mode == access::read)
							{
								readers.push_back(pass);
							}
						}
						else if (first_writer && pass < *first_writer &&
							use->mode == access::read)
						{
							add_edge(*first_writer, pass);
							readers_before_first_write.push_back(pass);
						}
						else
						{
							throw std::logic_error(
								"Transient frame-graph resource " +
								std::to_string(resource_index) +
								" must be written before it is read or read_write");
						}
					}

					if (writes)
					{
						if (last_writer)
						{
							add_edge(*last_writer, pass);
						}
						for (const auto reader : readers)
						{
							add_edge(reader, pass);
						}
						last_writer = pass;
						readers.clear();
						if (!resource.initially_initialized &&
							first_writer == pass)
						{
							readers = readers_before_first_write;
						}
					}
					else if (reads && last_writer)
					{
						readers.push_back(pass);
					}
				}
			}

			std::priority_queue<
				std::size_t,
				std::vector<std::size_t>,
				std::greater<>> ready;
			for (std::size_t pass = 0; pass < pass_count; ++pass)
			{
				if (indegree[pass] == 0)
				{
					ready.push(pass);
				}
			}

			std::vector<std::size_t> order;
			order.reserve(pass_count);
			while (!ready.empty())
			{
				const auto pass = ready.top();
				ready.pop();
				order.push_back(pass);
				for (const auto successor : outgoing[pass])
				{
					if (--indegree[successor] == 0)
					{
						ready.push(successor);
					}
				}
			}

			if (order.size() != pass_count)
			{
				throw std::logic_error("Frame graph contains a dependency cycle");
			}
			return order;
		}

		void execute_passes(const std::vector<std::size_t>& order)
		{
			if (order.empty())
			{
				return;
			}

			command_buffer commands(m_device);
			try
			{
				for (const auto pass_index : order)
				{
					auto& pass = m_passes[pass_index];
					pass_context context(*this, pass_index, commands);
					pass.callback(context);
					if (!commands.valid())
					{
						throw std::logic_error(
							"Frame-graph passes must not submit or cancel the graph's command buffer");
					}
				}
				commands.submit();
			}
			catch (...)
			{
				cancel_commands(commands);
				throw;
			}
		}

		static void cancel_commands(command_buffer& commands) noexcept
		{
			try
			{
				if (commands.valid())
				{
					commands.cancel();
				}
			}
			catch (const std::exception& error)
			{
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Failed to cancel a frame-graph command buffer: %s",
					error.what());
			}
			catch (...)
			{
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Failed to cancel a frame-graph command buffer");
			}
		}

		const device& m_device;
		std::vector<resource_entry> m_resources;
		std::vector<pass_node> m_passes;
		bool m_executing = false;
		bool m_execution_failed = false;
	};
}
