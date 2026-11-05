#pragma once

#include "mods.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_loadso.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#ifndef MNGL_MOD_EXPORT
#if defined(_WIN32)
#define MNGL_MOD_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define MNGL_MOD_EXPORT __attribute__((visibility("default")))
#else
#define MNGL_MOD_EXPORT
#endif
#endif

#ifndef MNGL_MOD_CALL
#define MNGL_MOD_CALL SDLCALL
#endif

namespace mngl::mods
{
	struct api_version
	{
		std::uint32_t major{1};
		std::uint32_t minor{};
		friend bool operator==(const api_version&, const api_version&) = default;
	};

	struct mod_dependency
	{
		std::string id;
		std::string version_range;
	};

	struct mod_manifest
	{
		std::uint32_t schema_version{};
		std::string id;
		std::string name;
		std::string version;
		std::vector<mod_dependency> dependencies;
		api_version required_api;
		std::string binary_abi;
		std::filesystem::path binary_path;
	};

	[[nodiscard]] inline std::string native_binary_abi_tag();

	struct mod_loader_options
	{
		api_version host_api{1, 0};
		std::string binary_abi{native_binary_abi_tag()};
	};

	enum class mod_load_state
	{
		loaded,
		rejected
	};

	struct mod_load_result
	{
		mod_load_state state{mod_load_state::rejected};
		std::string mod_id;
		std::string message;

		[[nodiscard]] explicit operator bool() const noexcept { return state == mod_load_state::loaded; }
	};

	enum class mod_unload_state
	{
		unloaded,
		deferred,
		dependency_in_use,
		not_loaded
	};

	struct mod_unload_result
	{
		mod_unload_state state{mod_unload_state::not_loaded};
		std::string mod_id;
		std::size_t active_calls{};
		std::size_t live_objects{};
		std::string message;
	};

	[[nodiscard]] inline std::string native_binary_abi_tag()
	{
#if defined(_WIN32)
		constexpr std::string_view operating_system = "windows";
#elif defined(__APPLE__)
		constexpr std::string_view operating_system = "macos";
#elif defined(__linux__)
		constexpr std::string_view operating_system = "linux";
#elif defined(__FreeBSD__)
		constexpr std::string_view operating_system = "freebsd";
#else
		constexpr std::string_view operating_system = "unknown-os";
#endif
#if defined(_MSC_VER)
#if defined(_M_X64)
		constexpr std::string_view architecture = "x64";
#elif defined(_M_IX86)
		constexpr std::string_view architecture = "x86";
#elif defined(_M_ARM64)
		constexpr std::string_view architecture = "arm64";
#elif defined(_M_ARM)
		constexpr std::string_view architecture = "arm";
#else
		constexpr std::string_view architecture = "unknown-arch";
#endif
#if defined(_DLL)
		constexpr std::string_view crt = "md";
#else
		constexpr std::string_view crt = "mt";
#endif
#if defined(_DEBUG)
		constexpr std::string_view config = "debug";
#else
		constexpr std::string_view config = "release";
#endif
#if defined(_ITERATOR_DEBUG_LEVEL)
		constexpr int iterator_debug_level = _ITERATOR_DEBUG_LEVEL;
#else
		constexpr int iterator_debug_level = 0;
#endif
		return "msvc-" + std::to_string(_MSC_VER) + "-" + std::string(architecture) + "-" + std::string(crt) +
		       "-it" + std::to_string(iterator_debug_level) + "-" + std::string(config) + "-" +
		       std::string(operating_system);
#elif defined(__clang__)
#if defined(__x86_64__) || defined(_M_X64)
		constexpr std::string_view architecture = "x64";
#elif defined(__i386__) || defined(_M_IX86)
		constexpr std::string_view architecture = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
		constexpr std::string_view architecture = "arm64";
#else
		constexpr std::string_view architecture = "unknown-arch";
#endif
		std::string standard_library = "stdlib-unknown";
#if defined(_LIBCPP_VERSION)
		standard_library = "libcxx" + std::to_string(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
		standard_library = "libstdcxx" + std::to_string(__GLIBCXX__);
#endif
		return "clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__) + "-" +
		       std::string(architecture) + "-cxx" + std::to_string(__cplusplus) + "-" +
		       std::string(operating_system) + "-" + standard_library;
#elif defined(__GNUC__)
#if defined(__x86_64__)
		constexpr std::string_view architecture = "x64";
#elif defined(__i386__)
		constexpr std::string_view architecture = "x86";
#elif defined(__aarch64__)
		constexpr std::string_view architecture = "arm64";
#elif defined(__arm__)
		constexpr std::string_view architecture = "arm";
#else
		constexpr std::string_view architecture = "unknown-arch";
#endif
#if defined(_GLIBCXX_USE_CXX11_ABI)
		constexpr int libstdcxx_abi = _GLIBCXX_USE_CXX11_ABI;
#else
		constexpr int libstdcxx_abi = 1;
#endif
		std::string standard_library = "libstdcxx-unknown";
#if defined(__GLIBCXX__)
		standard_library = "libstdcxx" + std::to_string(__GLIBCXX__);
#endif
		return "gcc-" + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "-" +
		       std::string(architecture) + "-cxx" + std::to_string(__cplusplus) + "-glibcxx" +
		       std::to_string(libstdcxx_abi) + "-" + std::string(operating_system) + "-" + standard_library;
#else
		return "unknown-native-abi";
#endif
	}

} // namespace mngl::mods
