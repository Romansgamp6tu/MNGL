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
			if (m_options.binary_abi.empty()) [[unlikely]]
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
				try
				{
					register_dependencies_locked(manifest.id, manifest);
				}
				catch (...)
				{
					unregister_dependencies_locked(manifest.id, manifest);
					m_loading.erase(manifest.id);
					m_loading_manifests.erase(manifest.id);
					return {mod_load_state::rejected, manifest.id,
					        "failed to reserve the mod dependency graph"};
				}
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
				if (!result && !m_pending.contains(reserved_id) && !m_loaded.contains(reserved_id))
					unregister_dependencies_locked(reserved_id, m_loading_manifests.at(reserved_id));
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
					const auto components = find_dependency_components(dependency_graph, unresolved);
					std::map<std::string, std::size_t> component_by_id;
					std::vector<bool> component_is_cycle(components.size(), false);
					std::vector<std::string> component_cycle_text(components.size());
					std::map<std::string, std::string> member_cycle_text;
					for (std::size_t component_index = 0; component_index < components.size(); ++component_index)
					{
						const auto& component = components[component_index];
						for (const auto& id : component)
							component_by_id.emplace(id, component_index);

						const bool self_cycle =
						    component.size() == 1 &&
						    std::ranges::find(dependency_graph.at(component.front()), component.front()) !=
						        dependency_graph.at(component.front()).end();
						if (component.size() == 1 && !self_cycle)
							continue;

						component_is_cycle[component_index] = true;
						std::set<std::string> component_ids(component.begin(), component.end());
						const auto representative = find_dependency_cycle_from(
						    dependency_graph, component_ids, component.front());
						component_cycle_text[component_index] = format_dependency_cycle(representative);
						for (const auto& id : component)
						{
							const auto member_cycle = find_dependency_cycle_from(dependency_graph, component_ids, id);
							member_cycle_text.emplace(id, format_dependency_cycle(member_cycle));
						}
					}

					// The SCC graph is a DAG. Propagate each reachable cycle from dependency
					// components to their dependents, starting with components with no
					// unresolved dependencies.
					std::vector<std::set<std::size_t>> component_dependencies(components.size());
					std::vector<std::set<std::size_t>> component_dependents(components.size());
					for (const auto& id : unresolved)
					{
						const auto component_index = component_by_id.at(id);
						for (const auto& dependency : dependency_graph.at(id))
						{
							if (const auto found = component_by_id.find(dependency);
							    found != component_by_id.end() && found->second != component_index)
							{
								component_dependencies[component_index].insert(found->second);
								component_dependents[found->second].insert(component_index);
							}
						}
					}
					std::vector<std::size_t> remaining_dependencies(components.size());
					std::vector<std::set<std::size_t>> reachable_cycles(components.size());
					std::set<std::size_t> ready_components;
					for (std::size_t i = 0; i < components.size(); ++i)
					{
						remaining_dependencies[i] = component_dependencies[i].size();
						if (component_is_cycle[i])
							reachable_cycles[i].insert(i);
						if (remaining_dependencies[i] == 0)
							ready_components.insert(i);
					}
					while (!ready_components.empty())
					{
						const auto dependency_index = *ready_components.begin();
						ready_components.erase(ready_components.begin());
						for (const auto dependent_index : component_dependents[dependency_index])
						{
							reachable_cycles[dependent_index].insert(reachable_cycles[dependency_index].begin(),
							                                      reachable_cycles[dependency_index].end());
							if (--remaining_dependencies[dependent_index] == 0)
								ready_components.insert(dependent_index);
						}
					}

					for (const auto& id : unresolved)
					{
						const auto component_index = component_by_id.at(id);
						std::string message;
						if (component_is_cycle[component_index])
							message = "dependency cycle detected: " + member_cycle_text.at(id) +
							          "; this mod is part of the cycle and was not loaded";
						else if (reachable_cycles[component_index].empty())
							message =
							    "dependency graph contains unresolved packages; this mod was not loaded";
						else
						{
							message = "cannot load this mod because its dependency graph contains ";
							message += reachable_cycles[component_index].size() == 1 ? "cycle: " : "cycles: ";
							bool first_cycle = true;
							for (const auto cycle_index : reachable_cycles[component_index])
							{
								if (!first_cycle)
									message += "; ";
								message += component_cycle_text[cycle_index];
								first_cycle = false;
							}
							message += reachable_cycles[component_index].size() == 1
							               ? "; this mod depends on the cycle and was not loaded"
							               : "; this mod depends on these cycles and was not loaded";
						}
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
			std::map<std::string, std::size_t> dependent_counts;
			std::map<std::string, std::vector<std::string>> loaded_dependencies;
			{
				std::lock_guard lock(m_mutex);
				ids.reserve(m_loaded.size());
				for (const auto& [id, _] : m_loaded)
				{
					ids.push_back(id);
					dependent_counts.emplace(id, 0);
				}

				// Pending and loading modules remain dependents too: their providers
				// must not enter the ready queue while either state is active.
				for (auto& [provider_id, dependent_count] : dependent_counts)
				{
					const auto reverse = m_dependents.find(provider_id);
					if (reverse == m_dependents.end())
						continue;
					for (const auto& dependent_id : reverse->second)
						if (m_loaded.contains(dependent_id) || m_pending.contains(dependent_id) ||
						    m_loading.contains(dependent_id))
							++dependent_count;
				}

				for (const auto& [dependent_id, module] : m_loaded)
				{
					auto& required = loaded_dependencies[dependent_id];
					for (const auto& dependency : module.manifest.dependencies)
						if (m_loaded.contains(dependency.id))
							required.push_back(dependency.id);
				}
			}

			std::set<std::string> ready;
			for (const auto& [id, count] : dependent_counts)
				if (count == 0)
					ready.insert(id);

			std::vector<std::string> unload_order;
			unload_order.reserve(ids.size());
			while (!ready.empty())
			{
				auto first = ready.begin();
				const auto id = *first;
				ready.erase(first);
				unload_order.push_back(id);
				if (const auto found = loaded_dependencies.find(id); found != loaded_dependencies.end())
				{
					for (const auto& provider_id : found->second)
					{
						auto& count = dependent_counts.at(provider_id);
						if (count != 0 && --count == 0)
							ready.insert(provider_id);
					}
				}
			}

			// A dependency cycle has no leaf. Keep the stable ID order so that
			// each attempted unload still reports the existing dependency guard.
			for (const auto& [id, count] : dependent_counts)
				if (count != 0)
					unload_order.push_back(id);

			std::vector<mod_unload_result> results;
			results.reserve(unload_order.size());
			for (const auto& id : unload_order)
				results.push_back(unload(id));
			(void)collect_deferred();
			return results;
		}

		// Call after destroying mod-created objects to complete any deferred native unloads.
		[[nodiscard]] std::size_t collect_deferred()
		{
			std::size_t collected = 0;
			for (;;)
			{
				std::vector<std::string> ready;
				{
					std::lock_guard lock(m_mutex);
					ready.reserve(m_pending.size());
					for (auto it = m_pending.begin(); it != m_pending.end();)
					{
						if (it->second.collecting || !blocking_dependent_locked(it->first).empty())
						{
							++it;
							continue;
						}
						const auto [active_calls, live_objects] = it->second.lifetime->outstanding();
						if (active_calls == 0 && live_objects == 0 && it->second.lifetime.use_count() == 1)
						{
							it->second.collecting = true;
							ready.push_back(it->first);
						}
						++it;
					}
				}
				if (ready.empty())
					break;

				for (const auto& id : ready)
				{
					std::shared_ptr<detail::module_lifetime> lifetime;
					{
						std::lock_guard lock(m_mutex);
						const auto found = m_pending.find(id);
						if (found == m_pending.end() || !found->second.collecting)
							continue;
						lifetime = found->second.lifetime;
					}

					// Keep the ID and dependency edges visible to reentrant loader calls
					// until user shutdown code and the native library close both finish.
					lifetime->finish_shutdown();
					lifetime->close_library();

					{
						std::lock_guard lock(m_mutex);
						const auto found = m_pending.find(id);
						if (found != m_pending.end() && found->second.collecting &&
						    found->second.lifetime == lifetime)
						{
							unregister_dependencies_locked(id, found->second.manifest);
							m_pending.erase(found);
							++collected;
						}
					}
				}
			}
			return collected;
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
			bool collecting{};
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

		void register_dependencies_locked(const std::string& mod_id, const mod_manifest& manifest)
		{
			try
			{
				for (const auto& dependency : manifest.dependencies)
					m_dependents[dependency.id].insert(mod_id);
			}
			catch (...)
			{
				unregister_dependencies_locked(mod_id, manifest);
				throw;
			}
		}

		void unregister_dependencies_locked(const std::string& mod_id, const mod_manifest& manifest)
		{
			for (const auto& dependency : manifest.dependencies)
			{
				const auto found = m_dependents.find(dependency.id);
				if (found == m_dependents.end())
					continue;
				found->second.erase(mod_id);
				if (found->second.empty())
					m_dependents.erase(found);
			}
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
			if (manifest.required_api.major != m_options.host_api.major) [[unlikely]]
				throw std::runtime_error("mod API major version does not match host");
			if (manifest.required_api.minor > m_options.host_api.minor) [[unlikely]]
				throw std::runtime_error("mod requires a newer MNGL mod API minor version");
			if (manifest.binary_abi != m_options.binary_abi) [[unlikely]]
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
			const auto found = m_dependents.find(dependency_id);
			if (found != m_dependents.end())
				for (const auto& id : found->second)
					if (id != dependency_id &&
					    (m_loaded.contains(id) || m_pending.contains(id) || m_loading.contains(id)))
						return id;
			return {};
		}

		[[nodiscard]] static std::vector<std::vector<std::string>> find_dependency_components(
		    const std::map<std::string, std::vector<std::string>>& graph,
		    const std::set<std::string>& nodes)
		{
			struct frame
			{
				std::string id;
				std::size_t next_edge{};
			};

			std::unordered_set<std::string> visited;
			std::vector<std::string> finish_order;
			for (const auto& start : nodes)
			{
				if (!visited.insert(start).second)
					continue;
				std::vector<frame> stack{frame{start, 0}};
				while (!stack.empty())
				{
					auto& current = stack.back();
					const auto edges = graph.find(current.id);
					if (edges == graph.end() || current.next_edge >= edges->second.size())
					{
						finish_order.push_back(current.id);
						stack.pop_back();
						continue;
					}

					const auto dependency = edges->second[current.next_edge++];
					if (nodes.contains(dependency) && visited.insert(dependency).second)
					{
						stack.push_back(frame{dependency, 0});
					}
				}
			}

			std::map<std::string, std::vector<std::string>> reverse_graph;
			for (const auto& id : nodes)
				if (const auto edges = graph.find(id); edges != graph.end())
					for (const auto& dependency : edges->second)
						if (nodes.contains(dependency))
							reverse_graph[dependency].push_back(id);

			visited.clear();
			std::vector<std::vector<std::string>> components;
			for (auto it = finish_order.rbegin(); it != finish_order.rend(); ++it)
			{
				if (!visited.insert(*it).second)
					continue;
				std::vector<std::string> component;
				std::vector<std::string> stack{*it};
				while (!stack.empty())
				{
					auto id = std::move(stack.back());
					stack.pop_back();
					component.push_back(id);
					if (const auto parents = reverse_graph.find(id); parents != reverse_graph.end())
						for (const auto& parent : parents->second)
							if (visited.insert(parent).second)
								stack.push_back(parent);
				}
				std::sort(component.begin(), component.end());
				components.push_back(std::move(component));
			}
			return components;
		}

		[[nodiscard]] static std::vector<std::string> find_dependency_cycle_from(
		    const std::map<std::string, std::vector<std::string>>& graph,
		    const std::set<std::string>& component,
		    std::string_view start)
		{
			struct frame
			{
				std::string id;
				std::size_t next_edge{};
			};
			const std::string start_id(start);
			std::unordered_set<std::string> visited{start_id};
			std::vector<std::string> path{start_id};
			std::vector<frame> stack{frame{start_id, 0}};
			while (!stack.empty())
			{
				auto& current = stack.back();
				const auto edges = graph.find(current.id);
				if (edges == graph.end() || current.next_edge >= edges->second.size())
				{
					stack.pop_back();
					path.pop_back();
					continue;
				}

				const auto dependency = edges->second[current.next_edge++];
				if (!component.contains(dependency))
					continue;
				if (dependency == start_id)
				{
					path.push_back(start_id);
					return path;
				}
				if (visited.insert(dependency).second)
				{
					path.push_back(dependency);
					stack.push_back(frame{dependency, 0});
				}
			}
			return {};
		}

		[[nodiscard]] static std::string format_dependency_cycle(const std::vector<std::string>& cycle)
		{
			std::string text;
			for (const auto& id : cycle)
			{
				if (!text.empty())
					text += " -> ";
				text += id;
			}
			return text;
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
		std::map<std::string, std::set<std::string>, std::less<>> m_dependents;
	};

} // namespace mngl::mods

