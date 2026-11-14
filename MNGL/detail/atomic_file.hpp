#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#define MNGL_DETAIL_ATOMIC_FILE_UNDEFINE_NOMINMAX
#endif
#include <Windows.h>
#ifdef MNGL_DETAIL_ATOMIC_FILE_UNDEFINE_NOMINMAX
#undef MNGL_DETAIL_ATOMIC_FILE_UNDEFINE_NOMINMAX
#undef NOMINMAX
#endif
#else
#include <unistd.h>
#endif

namespace mngl::detail
{
	class atomic_temporary_file
	{
	public:
		explicit atomic_temporary_file(const std::filesystem::path& destination)
		{
			auto parent = destination.parent_path();
			if (parent.empty())
				parent = std::filesystem::current_path();
			const auto base = parent / ".mngl-tmp-";

			for (std::size_t attempt = 0; attempt < 1024; ++attempt)
			{
				auto candidate = base;
				candidate += std::to_string(process_id());
				candidate += "-";
				candidate += std::to_string(next_id().fetch_add(1, std::memory_order_relaxed));
				m_path = candidate;
				m_stream.open(m_path, std::ios::binary | std::ios::out | std::ios::noreplace);
				if (m_stream.is_open())
					return;
				m_stream.clear();
				m_path.clear();

				std::error_code error;
				if (std::filesystem::exists(candidate, error))
					continue;
				if (error)
					throw std::filesystem::filesystem_error("cannot inspect temporary file path", candidate, error);
			}

			throw std::runtime_error("cannot reserve a unique temporary file path");
		}

		atomic_temporary_file(const atomic_temporary_file&) = delete;
		atomic_temporary_file& operator=(const atomic_temporary_file&) = delete;

		~atomic_temporary_file()
		{
			if (m_stream.is_open())
				m_stream.close();
			cleanup();
		}

		[[nodiscard]] std::ofstream& stream() noexcept { return m_stream; }
		[[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

		void close()
		{
			m_stream.flush();
			if (!m_stream) [[unlikely]]
				throw std::runtime_error("failed while writing temporary file: " + m_path.string());
			m_stream.close();
			if (m_stream.fail()) [[unlikely]]
				throw std::runtime_error("failed while closing temporary file: " + m_path.string());
		}

		void replace(const std::filesystem::path& destination)
		{
			if (m_stream.is_open())
				close();
#ifdef _WIN32
			if (!MoveFileExW(m_path.c_str(), destination.c_str(),
			                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				const std::error_code error(static_cast<int>(GetLastError()), std::system_category());
				throw std::filesystem::filesystem_error("cannot atomically replace file", m_path, destination,
				                                        error);
			}
#else
			std::filesystem::rename(m_path, destination);
#endif
		}

	private:
		[[nodiscard]] static std::uint64_t process_id() noexcept
		{
#ifdef _WIN32
			return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
			return static_cast<std::uint64_t>(::getpid());
#endif
		}

		[[nodiscard]] static std::atomic<std::uint64_t>& next_id() noexcept
		{
			static std::atomic<std::uint64_t> value{1};
			return value;
		}

		void cleanup() noexcept
		{
			if (m_path.empty())
				return;

			std::error_code ignored;
			std::filesystem::remove(m_path, ignored);
		}

		std::ofstream m_stream;
		std::filesystem::path m_path;
	};
} // namespace mngl::detail
