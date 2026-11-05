#pragma once

#include "manifest.hpp"

namespace mngl::mods
{
	class mod_loader
	{
	public:
		explicit mod_loader(registry& owner, mod_loader_options options = {})
		    : m_registry(owner), m_options(std::move(options))
		{
			if (m_options.binary_abi.empty())
				throw std::invalid_argument("mod loader ABI tag cannot be empty");
		}

		mod_loader(const mod_loader&) = delete;
		mod_loader& operator=(const mod_loader&) = delete;

		~mod_loader() { (void)unload_all(); }

		[[nodiscard]] mod_load_result load_package(const std::filesystem::path& package_directory)
		{
			(void)collect_deferred();
			mod_manifest manifest;
			try
			{
				manifest = detail::parse_manifest_file(package_directory);
				validate_manifest(manifest);
			}
			catch (const detail::manifest_parse_error& error)
			{
				return {mod_load_state::rejected, error.mod_id(), error.what()};
			}
			catch (const std::exception& error)
			{
				return {mod_load_state::rejected, manifest.id, error.what()};
			}

			{
				std::lock_guard lock(m_mutex);
				if (m_loaded.contains(manifest.id) || m_pending.contains(manifest.id) ||
				    m_loading.contains(manifest.id))
					return {mod_load_state::rejected, manifest.id,
					        "a mod with this ID is already loaded or loading"};
				if (const auto dependency_error = dependency_error_locked(manifest);
				    !dependency_error.empty())
					return {mod_load_state::rejected, manifest.id, dependency_error};
				m_loading.insert(manifest.id);
				m_loading_manifests.emplace(manifest.id, manifest);
			}

			const auto reserved_id = manifest.id;
			mod_load_result result;
			try
			{
				result = load_reserved(std::move(manifest));
			}
			catch (const std::exception& error)
			{
				result = {mod_load_state::rejected, reserved_id, error.what()};
			}
			catch (...)
			{
				result = {mod_load_state::rejected, reserved_id, "unknown error while loading mod"};
			}
			{
				std::lock_guard lock(m_mutex);
				m_loading.erase(reserved_id);
				m_loading_manifests.erase(reserved_id);
			}
			(void)collect_deferred();
			return result;
		}

		[[nodiscard]] std::vector<mod_load_result> load_all(const std::filesystem::path& mods_directory)
		{
			std::vector<mod_load_result> results;
			try
			{
				if (!std::filesystem::is_directory(mods_directory))
					return {{mod_load_state::rejected, {}, "mods path is not a directory"}};
				std::vector<std::filesystem::path> packages;
				for (const auto& entry : std::filesystem::directory_iterator(mods_directory))
					if (entry.is_directory() && std::filesystem::is_regular_file(entry.path() / "mod.json"))
						packages.push_back(entry.path());
				std::sort(packages.begin(), packages.end());

				struct package_candidate
				{
					std::filesystem::path package;
					mod_manifest manifest;
				};
				std::vector<package_candidate> candidates;
				candidates.reserve(packages.size());
				results.reserve(packages.size());
				for (const auto& package : packages)
				{
					mod_manifest manifest;
					try
					{
						manifest = detail::parse_manifest_file(package);
						validate_manifest(manifest);
						candidates.push_back({package, std::move(manifest)});
					}
					catch (const detail::manifest_parse_error& error)
					{
						results.push_back({mod_load_state::rejected, error.mod_id(), error.what()});
					}
					catch (const std::exception& error)
					{
						results.push_back({mod_load_state::rejected, manifest.id, error.what()});
					}
				}

				std::map<std::string, std::vector<std::size_t>> candidates_by_id;
				for (std::size_t i = 0; i < candidates.size(); ++i)
					candidates_by_id[candidates[i].manifest.id].push_back(i);

				std::unordered_set<std::string> unavailable_ids;
				{
					std::lock_guard lock(m_mutex);
					for (const auto& [id, _] : m_loaded)
						unavailable_ids.insert(id);
					for (const auto& [id, _] : m_pending)
						unavailable_ids.insert(id);
					unavailable_ids.insert(m_loading.begin(), m_loading.end());
				}

				std::map<std::string, std::size_t> graph_indices;
				for (const auto& [id, indices] : candidates_by_id)
				{
					if (indices.size() > 1)
					{
						for (const auto index : indices)
							results.push_back(
							    {mod_load_state::rejected, id, "multiple packages declare the same mod ID"});
					}
					else if (unavailable_ids.contains(id))
					{
						results.push_back({mod_load_state::rejected, id,
						                   "a mod with this ID is already loaded, loading, or unloading"});
					}
					else
					{
						graph_indices.emplace(id, indices.front());
					}
				}

				std::map<std::string, std::vector<std::string>> dependency_graph;
				std::map<std::string, std::size_t> indegree;
				std::map<std::string, std::vector<std::string>> dependents;
				for (const auto& [id, index] : graph_indices)
				{
					indegree.emplace(id, 0);
					auto& dependencies = dependency_graph[id];
					for (const auto& dependency : candidates[index].manifest.dependencies)
					{
						dependencies.push_back(dependency.id);
						if (graph_indices.contains(dependency.id))
						{
							++indegree[id];
							dependents[dependency.id].push_back(id);
						}
					}
				}

				std::set<std::string> ready;
				for (const auto& [id, degree] : indegree)
					if (degree == 0)
						ready.insert(id);

				std::vector<std::string> order;
				order.reserve(graph_indices.size());
				while (!ready.empty())
				{
					const auto id = *ready.begin();
					ready.erase(ready.begin());
					order.push_back(id);
					if (const auto found = dependents.find(id); found != dependents.end())
					{
						for (const auto& dependent : found->second)
							if (--indegree[dependent] == 0)
								ready.insert(dependent);
					}
				}

				if (order.size() != graph_indices.size())
				{
					std::set<std::string> unresolved;
					for (const auto& [id, _] : graph_indices)
						if (indegree[id] != 0)
							unresolved.insert(id);
					const auto cycle = find_dependency_cycle(dependency_graph, unresolved);
					std::string cycle_text;
					std::set<std::string> cycle_members;
					for (std::size_t i = 0; i < cycle.size(); ++i)
					{
						if (!cycle_text.empty())
							cycle_text += " -> ";
						cycle_text += cycle[i];
						if (i + 1 < cycle.size())
							cycle_members.insert(cycle[i]);
					}
					for (const auto& id : unresolved)
					{
						const auto message =
						    cycle_text.empty()
						        ? "dependency graph contains unresolved packages; this mod was not loaded"
						        : (cycle_members.contains(id)
						               ? "dependency cycle detected: " + cycle_text +
						                     "; this mod is part of the cycle and was not loaded"
						               : "cannot load this mod because its dependency graph contains a "
						                 "cycle: " +
						                     cycle_text +
						                     "; this mod depends on the cycle and was not loaded");
						results.push_back({mod_load_state::rejected, id, message});
					}
				}

				for (const auto& id : order)
				{
					const auto index = graph_indices.at(id);
					results.push_back(load_package(candidates[index].package));
				}
			}
			catch (const std::exception& error)
			{
				results.push_back({mod_load_state::rejected, {}, error.what()});
			}
			return results;
		}

		[[nodiscard]] mod_unload_result unload(std::string_view mod_id)
		{
			loaded_module module;
			bool was_pending = false;
			{
				std::lock_guard lock(m_mutex);
				const auto found = m_loaded.find(std::string(mod_id));
				if (found == m_loaded.end())
				{
					was_pending = m_pending.contains(std::string(mod_id));
					if (!was_pending)
						return {mod_unload_state::not_loaded, std::string(mod_id), 0, 0, "mod is not loaded"};
				}
				else
				{
					if (const auto dependent = blocking_dependent_locked(mod_id); !dependent.empty())
						return {mod_unload_state::dependency_in_use, std::string(mod_id), 0, 0,
						        "cannot unload while dependent mod '" + dependent +
						            "' is loaded, loading, or pending unload"};
					const auto id = found->first;
					m_pending.emplace(id, found->second);
					module = std::move(found->second);
					m_loaded.erase(found);
				}
			}

			if (!was_pending)
			{
				module.lifetime->stop_accepting();
				m_registry.remove_mod(module.manifest.id);
				const auto id = module.manifest.id;
				std::shared_ptr<detail::module_lifetime> lifetime = module.lifetime;
				const auto unload_entry = module.unload;
				lifetime->request_shutdown(unload_entry
				                               ? std::function<void()>([unload_entry] { unload_entry(); })
											   : std::function<void()>{});
				module.lifetime.reset();
				lifetime.reset();
				(void)collect_deferred();
				return pending_result(id);
			}

			(void)collect_deferred();
			{
				std::lock_guard lock(m_mutex);
				if (!m_pending.contains(std::string(mod_id)))
					return {mod_unload_state::unloaded, std::string(mod_id), 0, 0,
					        "deferred unload completed"};
			}
			return pending_result(std::string(mod_id));
		}

		[[nodiscard]] std::vector<mod_unload_result> unload_all()
		{
			std::vector<std::string> ids;
			std::map<std::string, std::vector<std::string>> dependencies;
			{
				std::lock_guard lock(m_mutex);
				ids.reserve(m_loaded.size());
				for (const auto& [id, module] : m_loaded)
				{
					ids.push_back(id);
					auto& required = dependencies[id];
					for (const auto& dependency : module.manifest.dependencies)
						required.push_back(dependency.id);
				}
			}

			std::set<std::string> remaining(ids.begin(), ids.end());
			ids.clear();
			while (!remaining.empty())
			{
				std::vector<std::string> leaves;
				for (const auto& candidate : remaining)
				{
					const bool has_loaded_dependent =
					    std::any_of(remaining.begin(), remaining.end(),
						            [&](const std::string& other)
						            {
						                if (other == candidate)
							                return false;
						                const auto found = dependencies.find(other);
						                return found != dependencies.end() &&
										       std::find(found->second.begin(), found->second.end(),
										                 candidate) != found->second.end();
					                });
					if (!has_loaded_dependent)
						leaves.push_back(candidate);
				}
				if (leaves.empty())
				{
					ids.insert(ids.end(), remaining.begin(), remaining.end());
					break;
				}
				for (const auto& leaf : leaves)
				{
					ids.push_back(leaf);
					remaining.erase(leaf);
				}
			}

			std::vector<mod_unload_result> results;
			results.reserve(ids.size());
			for (const auto& id : ids)
				results.push_back(unload(id));
			(void)collect_deferred();
			return results;
		}

		// Call after destroying mod-created objects to complete any deferred native unloads.
		[[nodiscard]] std::size_t collect_deferred()
		{
			std::vector<loaded_module> ready;
			{
				std::lock_guard lock(m_mutex);
				for (auto it = m_pending.begin(); it != m_pending.end();)
				{
					const auto [active_calls, live_objects] = it->second.lifetime->outstanding();
					if (active_calls == 0 && live_objects == 0 && it->second.lifetime.use_count() == 1)
					{
						ready.push_back(std::move(it->second));
						it = m_pending.erase(it);
					}
					else
						++it;
				}
			}

			for (auto& module : ready)
			{
				module.lifetime->finish_shutdown();
				module.lifetime->close_library();
			}
			return ready.size();
		}

		[[nodiscard]] bool is_loaded(std::string_view mod_id) const
		{
			std::lock_guard lock(m_mutex);
			return m_loaded.contains(std::string(mod_id));
		}

		[[nodiscard]] std::vector<mod_manifest> loaded_mods() const
		{
			std::lock_guard lock(m_mutex);
			std::vector<mod_manifest> result;
			result.reserve(m_loaded.size());
			for (const auto& [_, module] : m_loaded)
				result.push_back(module.manifest);
			std::sort(result.begin(), result.end(),
			          [](const auto& left, const auto& right) { return left.id < right.id; });
			return result;
		}

	private:
		struct loaded_module
		{
			mod_manifest manifest;
			std::shared_ptr<detail::module_lifetime> lifetime;
			detail::mod_unload_entrypoint unload{};
		};

		[[nodiscard]] mod_unload_result pending_result(const std::string& mod_id) const
		{
			std::lock_guard lock(m_mutex);
			const auto found = m_pending.find(mod_id);
			if (found == m_pending.end())
				return {mod_unload_state::unloaded, mod_id, 0, 0, "deferred unload completed"};
			const auto [active_calls, live_objects] = found->second.lifetime->outstanding();
			return {mod_unload_state::deferred, mod_id, active_calls, live_objects,
			        "registrations removed; call collect_deferred after active calls and mod objects end"};
		}

		struct rollback_guard
		{
			mod_loader* loader{};
			mod_manifest manifest;
			std::shared_ptr<detail::module_lifetime> lifetime;
			detail::mod_unload_entrypoint unload{};
			bool active{true};

			~rollback_guard()
			{
				if (active && loader && lifetime)
					loader->cleanup_failed_load(manifest, lifetime, unload);
			}
		};

		void cleanup_failed_load(const mod_manifest& manifest,
		                         const std::shared_ptr<detail::module_lifetime>& lifetime,
		                         detail::mod_unload_entrypoint unload) noexcept
		{
			lifetime->stop_accepting();
			try
			{
				m_registry.remove_mod(manifest.id);
			}
			catch (...)
			{
			}

			try
			{
				{
					std::lock_guard lock(m_mutex);
					m_pending.emplace(manifest.id, loaded_module{manifest, lifetime, unload});
				}
				lifetime->request_shutdown(unload ? std::function<void()>([unload] { unload(); })
				                                  : std::function<void()>{});
			}
			catch (...)
			{
			}
		}

		void validate_manifest(const mod_manifest& manifest) const
		{
			if (manifest.required_api.major != m_options.host_api.major)
				throw std::runtime_error("mod API major version does not match host");
			if (manifest.required_api.minor > m_options.host_api.minor)
				throw std::runtime_error("mod requires a newer MNGL mod API minor version");
			if (manifest.binary_abi != m_options.binary_abi)
				throw std::runtime_error("mod binary_abi does not match host ABI tag '" +
				                         m_options.binary_abi + "'");
		}

		[[nodiscard]] std::string dependency_error_locked(const mod_manifest& manifest) const
		{
			for (const auto& dependency : manifest.dependencies)
			{
				const auto found = m_loaded.find(dependency.id);
				if (found == m_loaded.end())
				{
					if (m_pending.contains(dependency.id))
						return "required dependency '" + dependency.id + "' is unloading";
					if (m_loading.contains(dependency.id))
						return "required dependency '" + dependency.id + "' is still loading";
					return "required dependency '" + dependency.id + "' is not loaded";
				}
				if (!detail::version_satisfies(found->second.manifest.version, dependency.version_range))
					return "required dependency '" + dependency.id + "' version " +
					       found->second.manifest.version + " does not satisfy '" + dependency.version_range +
					       "'";
			}
			return {};
		}

		[[nodiscard]] std::string blocking_dependent_locked(std::string_view dependency_id) const
		{
			const auto depends_on = [dependency_id](const mod_manifest& manifest)
			{
				return std::any_of(manifest.dependencies.begin(), manifest.dependencies.end(),
				                   [dependency_id](const mod_dependency& dependency)
				                   { return dependency.id == dependency_id; });
			};
			for (const auto& [id, module] : m_loaded)
				if (id != dependency_id && depends_on(module.manifest))
					return id;
			for (const auto& [id, module] : m_pending)
				if (id != dependency_id && depends_on(module.manifest))
					return id;
			for (const auto& [id, manifest] : m_loading_manifests)
				if (id != dependency_id && depends_on(manifest))
					return id;
			return {};
		}

		[[nodiscard]] static std::vector<std::string> find_dependency_cycle(
		    const std::map<std::string, std::vector<std::string>>& graph,
		    const std::set<std::string>& unresolved)
		{
			struct frame
			{
				std::string id;
				std::size_t next_dependency{};
			};
			std::unordered_map<std::string, unsigned char> state;
			std::vector<std::string> path;
			for (const auto& start : unresolved)
			{
				if (state[start] != 0)
					continue;
				state[start] = 1;
				path.push_back(start);
				std::vector<frame> stack{frame{start, 0}};
				while (!stack.empty())
				{
					auto& current = stack.back();
					const auto edges = graph.find(current.id);
					if (edges == graph.end() || current.next_dependency >= edges->second.size())
					{
						state[current.id] = 2;
						stack.pop_back();
						path.pop_back();
						continue;
					}

					const auto dependency = edges->second[current.next_dependency++];
					if (!unresolved.contains(dependency))
						continue;
					if (state[dependency] == 0)
					{
						state[dependency] = 1;
						path.push_back(dependency);
						stack.push_back(frame{dependency, 0});
					}
					else if (state[dependency] == 1)
					{
						const auto first = std::find(path.begin(), path.end(), dependency);
						std::vector<std::string> cycle(first, path.end());
						cycle.push_back(dependency);
						return cycle;
					}
				}
			}
			return {};
		}

		mod_load_result load_reserved(mod_manifest manifest)
		{
			const auto encoded_path = detail::path_to_utf8(manifest.binary_path);
			std::unique_ptr<SDL_SharedObject, detail::shared_object_deleter> library(
			    SDL_LoadObject(encoded_path.c_str()));
			if (!library)
				return {mod_load_state::rejected, manifest.id,
				        std::string("cannot load mod binary: ") + SDL_GetError()};

			const auto load = detail::load_mod_load_entrypoint(library.get());
			const auto unload = detail::load_mod_unload_entrypoint(library.get());
			if (!load)
				return {mod_load_state::rejected, manifest.id, "mod binary does not export mngl_mod_load_v1"};

			auto* raw_library = library.get();
			auto lifetime =
			    std::make_shared<detail::module_lifetime>([raw_library] { SDL_UnloadObject(raw_library); });
			library.release();
			rollback_guard rollback{this, manifest, lifetime, unload, true};

			bool initialized = false;
			try
			{
				auto context = m_registry.for_mod(manifest.id, lifetime);
				initialized = load(context);
			}
			catch (const std::exception& error)
			{
				return {mod_load_state::rejected, manifest.id,
				        std::string("mod initialization threw: ") + error.what()};
			}
			catch (...)
			{
				return {mod_load_state::rejected, manifest.id,
				        "mod initialization threw an unknown exception"};
			}
			if (!initialized)
				return {mod_load_state::rejected, manifest.id, "mngl_mod_load_v1 returned false"};

			{
				std::lock_guard lock(m_mutex);
				m_loaded.emplace(manifest.id, loaded_module{manifest, lifetime, unload});
			}
			rollback.active = false;
			return {mod_load_state::loaded, manifest.id, "mod loaded"};
		}

		registry& m_registry;
		mod_loader_options m_options;
		mutable std::mutex m_mutex;
		std::unordered_map<std::string, loaded_module> m_loaded;
		std::unordered_map<std::string, loaded_module> m_pending;
		std::unordered_set<std::string> m_loading;
		std::unordered_map<std::string, mod_manifest> m_loading_manifests;
	};

} // namespace mngl::mods
