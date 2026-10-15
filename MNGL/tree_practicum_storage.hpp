#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#define TREE_PRACTICUM_UNDEFINE_NOMINMAX
#endif
#include <Windows.h>
#include <io.h>
#ifdef TREE_PRACTICUM_UNDEFINE_NOMINMAX
#undef NOMINMAX
#undef TREE_PRACTICUM_UNDEFINE_NOMINMAX
#endif
#else
#include <unistd.h>
#endif

namespace tree_practicum
{
	struct archive_entry
	{
		std::string path;
		std::string contents;
	};

	namespace detail
	{
		inline constexpr std::size_t tar_block_size = 512;
		inline constexpr std::uint64_t max_archive_entry_size = 256ull * 1024ull * 1024ull;
		inline constexpr std::uint64_t max_archive_size = 1024ull * 1024ull * 1024ull;
		inline constexpr std::size_t max_archive_entries = 10000;
		inline constexpr std::uint64_t max_archive_file_size =
			max_archive_size +
			max_archive_entries * tar_block_size * 2ull +
			tar_block_size * 2ull;

		inline std::atomic_uint64_t temporary_file_sequence = 0;

		using tar_block = std::array<char, tar_block_size>;

		class atomic_file_writer
		{
		public:
			explicit atomic_file_writer(const std::filesystem::path& target)
				: target_(target)
			{
				if (target_.empty())
				{
					throw std::invalid_argument(
						"Output path cannot be empty");
				}

				for (std::size_t attempt = 0; attempt < 128; ++attempt)
				{
					temporary_ = target_;
					const auto suffix = make_temporary_suffix();
#if defined(_WIN32)
					std::wstring wide_suffix;
					wide_suffix.reserve(suffix.size());
					for (const char character : suffix)
					{
						wide_suffix.push_back(
							static_cast<wchar_t>(character));
					}
					temporary_ += wide_suffix;
					const auto open_result = _wfopen_s(
						&file_,
						temporary_.c_str(),
						L"wbx");
					if (file_)
					{
						return;
					}
					if (open_result != EEXIST)
					{
						throw std::system_error(
							std::error_code(
								static_cast<int>(open_result),
								std::generic_category()),
							"Cannot create temporary output file");
					}
#else
					temporary_ += suffix;
					errno = 0;
					file_ = std::fopen(temporary_.c_str(), "wbx");
					if (file_)
					{
						return;
					}
					if (errno != EEXIST)
					{
						throw std::system_error(
							std::error_code(
								errno == 0 ? EIO : errno,
								std::generic_category()),
							"Cannot create temporary output file");
					}
#endif
				}

				throw std::runtime_error(
					"Cannot allocate a unique temporary output file");
			}

			atomic_file_writer(const atomic_file_writer&) = delete;
			atomic_file_writer& operator=(const atomic_file_writer&) = delete;

			~atomic_file_writer()
			{
				if (file_)
				{
					(void)std::fclose(file_);
				}
				if (!committed_)
				{
					std::error_code ignored;
					std::filesystem::remove(temporary_, ignored);
				}
			}

			void write(const char* data, std::size_t size)
			{
				while (size != 0)
				{
					const auto written = std::fwrite(data, 1, size, file_);
					if (written == 0)
					{
						throw std::system_error(
							std::error_code(
								errno == 0 ? EIO : errno,
								std::generic_category()),
							"Failed while writing temporary output file");
					}
					data += written;
					size -= written;
				}
			}

			void commit()
			{
				if (std::fflush(file_) != 0)
				{
					throw std::system_error(
						std::error_code(
							errno == 0 ? EIO : errno,
							std::generic_category()),
						"Failed to flush temporary output file");
				}
#if defined(_WIN32)
				if (_commit(_fileno(file_)) != 0)
#else
				if (::fsync(::fileno(file_)) != 0)
#endif
				{
					throw std::system_error(
						std::error_code(
							errno == 0 ? EIO : errno,
							std::generic_category()),
						"Failed to synchronize temporary output file");
				}

				const int close_result = std::fclose(file_);
				file_ = nullptr;
				if (close_result != 0)
				{
					throw std::system_error(
						std::error_code(
							errno == 0 ? EIO : errno,
							std::generic_category()),
						"Failed to close temporary output file");
				}

#if defined(_WIN32)
				if (!MoveFileExW(
					temporary_.c_str(),
					target_.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					throw std::system_error(
						std::error_code(
							static_cast<int>(GetLastError()),
							std::system_category()),
						"Cannot replace output file");
				}
#else
				std::error_code error;
				std::filesystem::rename(temporary_, target_, error);
				if (error)
				{
					throw std::filesystem::filesystem_error(
						"Cannot replace output file",
						temporary_,
						target_,
						error);
				}
#endif
				committed_ = true;
			}

		private:
			[[nodiscard]] static std::string make_temporary_suffix()
			{
#if defined(_WIN32)
				const auto process_id =
					static_cast<std::uint64_t>(GetCurrentProcessId());
#else
				const auto process_id =
					static_cast<std::uint64_t>(::getpid());
#endif
				const auto timestamp = static_cast<std::uint64_t>(
					std::chrono::steady_clock::now()
						.time_since_epoch()
						.count());
				const auto sequence = temporary_file_sequence.fetch_add(
					1,
					std::memory_order_relaxed);
				return ".tmp." + std::to_string(process_id) + "." +
					std::to_string(timestamp) + "." +
					std::to_string(sequence);
			}

			std::filesystem::path target_;
			std::filesystem::path temporary_;
			std::FILE* file_ = nullptr;
			bool committed_ = false;
		};

		[[nodiscard]] inline bool is_zero_block(const tar_block& block) noexcept
		{
			return std::all_of(
				block.begin(),
				block.end(),
				[](char value) { return value == '\0'; });
		}

		[[nodiscard]] inline std::string read_string_field(
			const tar_block& block,
			std::size_t offset,
			std::size_t length)
		{
			const auto first = block.begin() + static_cast<std::ptrdiff_t>(offset);
			const auto last = first + static_cast<std::ptrdiff_t>(length);
			const auto terminator = std::find(first, last, '\0');
			return { first, terminator };
		}

		[[nodiscard]] inline std::uint64_t read_octal_field(
			const char* field,
			std::size_t length,
			std::string_view field_name)
		{
			std::size_t offset = 0;
			while (offset < length && (field[offset] == ' ' || field[offset] == '\0'))
			{
				++offset;
			}

			std::uint64_t value = 0;
			bool has_digit = false;
			for (; offset < length && field[offset] != '\0' && field[offset] != ' '; ++offset)
			{
				if (field[offset] < '0' || field[offset] > '7')
				{
					throw std::invalid_argument(
						std::string("Invalid TAR ") + std::string(field_name) + " field");
				}

				const auto digit = static_cast<std::uint64_t>(field[offset] - '0');
				if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 8)
				{
					throw std::invalid_argument(
						std::string("TAR ") + std::string(field_name) + " field overflows");
				}
				value = value * 8 + digit;
				has_digit = true;
			}

			return has_digit ? value : 0;
		}

		[[nodiscard]] inline std::string normalize_archive_path(
			std::string_view path,
			bool allow_directory_root = false)
		{
			while (path.starts_with("./"))
			{
				path.remove_prefix(2);
			}
			if (path.empty() && allow_directory_root)
			{
				return {};
			}
			while (!path.empty() && path.back() == '/')
			{
				path.remove_suffix(1);
			}

			if (path.empty() ||
				path.size() > 255 ||
				path.front() == '/' ||
				path.find('\\') != std::string_view::npos ||
				path.find('\0') != std::string_view::npos)
			{
				throw std::invalid_argument("Unsafe or unsupported TAR member path");
			}

			std::size_t component_start = 0;
			while (component_start <= path.size())
			{
				const std::size_t separator = path.find('/', component_start);
				const std::size_t component_end =
					separator == std::string_view::npos ? path.size() : separator;
				const auto component =
					path.substr(component_start, component_end - component_start);
				if (component.empty() || component == "." || component == ".." ||
					component.find(':') != std::string_view::npos ||
					std::any_of(component.begin(), component.end(), [](unsigned char value)
					{
						return value < 0x20u || value == 0x7Fu;
					}))
				{
					throw std::invalid_argument("Unsafe or unsupported TAR member path");
				}

				if (separator == std::string_view::npos)
				{
					break;
				}
				component_start = separator + 1;
			}

			return std::string(path);
		}

		[[nodiscard]] inline std::string duplicate_key(std::string_view path)
		{
			std::string key(path);
			std::transform(
				key.begin(),
				key.end(),
				key.begin(),
				[](unsigned char value)
				{
					return value >= 'A' && value <= 'Z'
						? static_cast<char>(value + ('a' - 'A'))
						: static_cast<char>(value);
				});
			return key;
		}

		inline void read_exact(std::istream& stream, char* destination, std::size_t size)
		{
			if (size == 0)
			{
				return;
			}
			stream.read(destination, static_cast<std::streamsize>(size));
			if (stream.gcount() != static_cast<std::streamsize>(size))
			{
				throw std::runtime_error("Unexpected end of TAR archive");
			}
		}

		inline void skip_exact(std::istream& stream, std::uint64_t size)
		{
			std::array<char, tar_block_size> scratch{};
			while (size != 0)
			{
				const auto chunk = static_cast<std::size_t>(
					std::min<std::uint64_t>(size, scratch.size()));
				read_exact(stream, scratch.data(), chunk);
				size -= chunk;
			}
		}

		[[nodiscard]] inline std::uint64_t header_checksum(const tar_block& header) noexcept
		{
			std::uint64_t checksum = 0;
			for (std::size_t i = 0; i < header.size(); ++i)
			{
				checksum += static_cast<unsigned char>(
					i >= 148 && i < 156 ? ' ' : header[i]);
			}
			return checksum;
		}

		inline void write_octal_field(
			char* field,
			std::size_t length,
			std::uint64_t value)
		{
			if (length < 2)
			{
				throw std::invalid_argument("TAR numeric field is too small");
			}

			field[length - 1] = '\0';
			for (std::size_t i = length - 1; i > 0; --i)
			{
				field[i - 1] = static_cast<char>('0' + (value & 7u));
				value >>= 3;
			}
			if (value != 0)
			{
				throw std::length_error("TAR numeric field is too large");
			}
		}

		inline void write_checksum(tar_block& header)
		{
			std::fill(header.begin() + 148, header.begin() + 156, ' ');
			const auto checksum = header_checksum(header);
			if (checksum > 0777777u)
			{
				throw std::length_error("TAR header checksum is too large");
			}

			std::uint64_t remaining = checksum;
			for (std::size_t i = 6; i > 0; --i)
			{
				header[148 + i - 1] =
					static_cast<char>('0' + (remaining & 7u));
				remaining >>= 3;
			}
			header[154] = '\0';
			header[155] = ' ';
		}

		inline void write_header_path(tar_block& header, std::string_view path)
		{
			std::string_view name = path;
			std::string_view prefix;
			if (name.size() > 100)
			{
				bool split_found = false;
				std::size_t separator = name.rfind('/');
				while (separator != std::string_view::npos)
				{
					if (separator <= 155 && name.size() - separator - 1 <= 100)
					{
						prefix = name.substr(0, separator);
						name.remove_prefix(separator + 1);
						split_found = true;
						break;
					}
					if (separator == 0)
					{
						break;
					}
					separator = name.rfind('/', separator - 1);
				}
				if (!split_found)
				{
					throw std::length_error(
						"TAR member path exceeds the USTAR path limit");
				}
			}

			std::copy(name.begin(), name.end(), header.begin());
			std::copy(prefix.begin(), prefix.end(), header.begin() + 345);
		}
	}

	[[nodiscard]] inline std::string read_tree_file(const std::filesystem::path& path)
	{
		std::error_code size_error;
		const auto file_size = std::filesystem::file_size(path, size_error);
		if (size_error)
		{
			throw std::runtime_error(
				"Cannot determine tree file size: " + size_error.message());
		}
		if (file_size > detail::max_archive_entry_size ||
			file_size > static_cast<std::uint64_t>(
				std::numeric_limits<std::streamsize>::max()))
		{
			throw std::length_error("Tree file exceeds the supported size limit");
		}

		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			throw std::runtime_error("Cannot open tree file");
		}

		std::string contents(static_cast<std::size_t>(file_size), '\0');
		if (!contents.empty())
		{
			stream.read(
				contents.data(),
				static_cast<std::streamsize>(contents.size()));
			if (stream.gcount() != static_cast<std::streamsize>(contents.size()))
			{
				throw std::runtime_error("Tree file changed or was truncated while reading");
			}
		}
		return contents;
	}

	inline void write_tree_file(
		const std::filesystem::path& path,
		std::string_view contents)
	{
		if (contents.size() > detail::max_archive_entry_size ||
			contents.size() > static_cast<std::size_t>(
				std::numeric_limits<std::streamsize>::max()))
		{
			throw std::length_error("Tree file exceeds the supported size limit");
		}

		detail::atomic_file_writer stream(path);
		if (!contents.empty())
		{
			stream.write(contents.data(), contents.size());
		}
		stream.commit();
	}

	[[nodiscard]] inline std::vector<archive_entry> read_tar_archive(
		const std::filesystem::path& path)
	{
		std::error_code size_error;
		const auto archive_size = std::filesystem::file_size(path, size_error);
		if (size_error)
		{
			throw std::runtime_error(
				"Cannot determine TAR archive size: " + size_error.message());
		}
		if (archive_size > detail::max_archive_file_size)
		{
			throw std::length_error("TAR archive exceeds the supported size limit");
		}

		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			throw std::runtime_error("Cannot open session TAR archive");
		}

		std::vector<archive_entry> entries;
		std::unordered_set<std::string> names;
		std::uint64_t total_size = 0;
		std::size_t header_count = 0;
		bool has_end_marker = false;

		while (true)
		{
			detail::tar_block header{};
			stream.read(header.data(), static_cast<std::streamsize>(header.size()));
			const auto header_bytes = stream.gcount();
			if (header_bytes == 0)
			{
				break;
			}
			if (header_bytes != static_cast<std::streamsize>(header.size()))
			{
				throw std::runtime_error("Truncated TAR header");
			}
			if (detail::is_zero_block(header))
			{
				detail::tar_block second_end_marker{};
				detail::read_exact(
					stream,
					second_end_marker.data(),
					second_end_marker.size());
				if (!detail::is_zero_block(second_end_marker))
				{
					throw std::invalid_argument("Invalid TAR end marker");
				}
				has_end_marker = true;
				break;
			}
			if (++header_count > detail::max_archive_entries)
			{
				throw std::length_error("TAR archive has too many entries");
			}

			const auto expected_checksum = detail::read_octal_field(
				header.data() + 148,
				8,
				"checksum");
			if (detail::header_checksum(header) != expected_checksum)
			{
				throw std::invalid_argument("TAR header checksum mismatch");
			}
			if (std::string_view(header.data() + 257, 5) != "ustar")
			{
				throw std::invalid_argument("Only POSIX USTAR archives are supported");
			}

			std::string member_name =
				detail::read_string_field(header, 0, 100);
			const std::string prefix =
				detail::read_string_field(header, 345, 155);
			if (!prefix.empty())
			{
				member_name = prefix + "/" + member_name;
			}

			const auto member_size = detail::read_octal_field(
				header.data() + 124,
				12,
				"size");
			const char type_flag = header[156];
			if (type_flag != '\0' && type_flag != '0' && type_flag != '5')
			{
				throw std::invalid_argument(
					"Unsupported TAR entry type; only regular files and directories are supported");
			}

			if (member_size > detail::max_archive_entry_size ||
				member_size > static_cast<std::uint64_t>(
					std::numeric_limits<std::streamsize>::max()))
			{
				throw std::length_error("TAR member exceeds the supported size limit");
			}

			const auto padded_size =
				member_size + ((detail::tar_block_size -
					(member_size % detail::tar_block_size)) %
					detail::tar_block_size);
			if (padded_size > detail::max_archive_size ||
				total_size > detail::max_archive_size - member_size)
			{
				throw std::length_error("TAR archive exceeds the supported size limit");
			}

			if (type_flag == '5')
			{
				if (member_size != 0)
				{
					throw std::invalid_argument("TAR directory entry has a nonzero size");
				}
				(void)detail::normalize_archive_path(member_name, true);
				continue;
			}

			member_name = detail::normalize_archive_path(member_name);
			if (entries.size() >= detail::max_archive_entries)
			{
				throw std::length_error("TAR archive has too many entries");
			}
			if (!names.insert(detail::duplicate_key(member_name)).second)
			{
				throw std::invalid_argument("TAR archive contains duplicate member paths");
			}

			archive_entry entry;
			entry.path = std::move(member_name);
			entry.contents.resize(static_cast<std::size_t>(member_size));
			detail::read_exact(stream, entry.contents.data(), entry.contents.size());
			detail::skip_exact(stream, padded_size - member_size);
			total_size += member_size;
			entries.push_back(std::move(entry));
		}

		if (!has_end_marker)
		{
			throw std::runtime_error("TAR archive is missing its end marker");
		}
		return entries;
	}

	inline void write_tar_archive(
		const std::filesystem::path& path,
		std::span<const archive_entry> entries)
	{
		if (entries.size() > detail::max_archive_entries)
		{
			throw std::length_error("TAR archive has too many entries");
		}

		std::unordered_set<std::string> names;
		std::uint64_t total_size = 0;
		for (const auto& entry : entries)
		{
			const auto safe_path = detail::normalize_archive_path(entry.path);
			if (safe_path != entry.path)
			{
				throw std::invalid_argument("TAR member path is not normalized");
			}
			detail::tar_block path_check{};
			detail::write_header_path(path_check, entry.path);
			if (!names.insert(detail::duplicate_key(entry.path)).second)
			{
				throw std::invalid_argument("TAR archive contains duplicate member paths");
			}
			if (entry.contents.size() > detail::max_archive_entry_size ||
				total_size > detail::max_archive_size - entry.contents.size())
			{
				throw std::length_error("TAR archive exceeds the supported size limit");
			}
			total_size += entry.contents.size();
		}

		detail::atomic_file_writer stream(path);

		for (const auto& entry : entries)
		{
			detail::tar_block header{};
			detail::write_header_path(header, entry.path);
			detail::write_octal_field(header.data() + 100, 8, 0644);
			detail::write_octal_field(header.data() + 108, 8, 0);
			detail::write_octal_field(header.data() + 116, 8, 0);
			detail::write_octal_field(
				header.data() + 124,
				12,
				static_cast<std::uint64_t>(entry.contents.size()));
			detail::write_octal_field(header.data() + 136, 12, 0);
			header[156] = '0';
			std::copy_n("ustar", 5, header.begin() + 257);
			header[262] = '\0';
			header[263] = '0';
			header[264] = '0';
			detail::write_checksum(header);

			stream.write(header.data(), header.size());
			if (!entry.contents.empty())
			{
				stream.write(entry.contents.data(), entry.contents.size());
			}

			const auto padding =
				(detail::tar_block_size -
					(entry.contents.size() % detail::tar_block_size)) %
				detail::tar_block_size;
			if (padding != 0)
			{
				const detail::tar_block zero_padding{};
				stream.write(zero_padding.data(), padding);
			}
		}

		const detail::tar_block end_marker{};
		stream.write(end_marker.data(), end_marker.size());
		stream.write(end_marker.data(), end_marker.size());
		stream.commit();
	}
}
