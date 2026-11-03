#pragma once

#include "utilities.hpp"

#include <SDL3/SDL_asyncio.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mngl::async_io
{
	enum class operation
	{
		read,
		write
	};

	enum class request_status
	{
		pending,
		completed,
		failed,
		cancelled
	};

	struct request_result
	{
		request_status status = request_status::pending;
		operation type = operation::read;
		std::uint64_t offset = 0;
		std::uint64_t bytes_requested = 0;
		std::uint64_t bytes_transferred = 0;
		std::string error;

		[[nodiscard]] bool ready() const noexcept
		{
			return status != request_status::pending;
		}
	};

	namespace detail
	{
		struct request_state
		{
			mutable std::mutex mutex;
			SDL_AsyncIO* file = nullptr;
			operation type = operation::read;
			request_status status = request_status::pending;
			std::vector<std::byte> buffer;
			std::uint64_t offset = 0;
			std::uint64_t bytes_requested = 0;
			std::uint64_t bytes_transferred = 0;
			bool io_done = false;
			bool io_succeeded = false;
			bool io_cancelled = false;
			bool close_queued = false;
			bool close_done = false;
			bool close_succeeded = false;
			bool close_cancelled = false;
			bool flush_on_close = false;
			bool data_taken = false;
			std::string error;
		};

		inline std::string path_to_utf8(const std::filesystem::path& path)
		{
			if (path.empty())
			{
				throw std::invalid_argument("async I/O path cannot be empty");
			}

			const auto encoded = path.u8string();
			std::string result(
				reinterpret_cast<const char*>(encoded.data()),
				encoded.size());
			if (result.find('\0') != std::string::npos)
			{
				throw std::invalid_argument("async I/O path cannot contain a null character");
			}
			return result;
		}

		inline std::string sdl_error_message(std::string_view prefix)
		{
			std::string message(prefix);
			if (const char* error = SDL_GetError(); error && *error)
			{
				message += error;
			}
			return message;
		}

		struct async_file_guard
		{
			SDL_AsyncIO* file = nullptr;
			SDL_AsyncIOQueue* queue = nullptr;

			~async_file_guard() noexcept
			{
				if (file)
				{
					SDL_CloseAsyncIO(file, false, queue, nullptr);
				}
			}

			void release() noexcept
			{
				file = nullptr;
			}
		};
	}

	class Request
	{
	public:
		Request(const Request&) noexcept = default;
		Request& operator=(const Request&) noexcept = default;
		Request(Request&&) noexcept = default;
		Request& operator=(Request&&) noexcept = default;

		[[nodiscard]] request_result result() const
		{
			std::lock_guard lock(m_state->mutex);
			return {
				m_state->status,
				m_state->type,
				m_state->offset,
				m_state->bytes_requested,
				m_state->bytes_transferred,
				m_state->error
			};
		}

		[[nodiscard]] bool ready() const
		{
			std::lock_guard lock(m_state->mutex);
			return m_state->status != request_status::pending;
		}

		// Transfers the completed read buffer to the caller. While the operation
		// is pending, the queue retains the request state and its buffer.
		[[nodiscard]] std::vector<std::byte> take_data()
		{
			std::lock_guard lock(m_state->mutex);
			if (m_state->type != operation::read)
			{
				throw std::logic_error("write requests do not have a result buffer");
			}
			if (m_state->status == request_status::pending)
			{
				throw std::logic_error("async read has not completed");
			}
			if (m_state->status != request_status::completed)
			{
				throw std::runtime_error(
					m_state->error.empty() ? "async read did not complete successfully" : m_state->error);
			}
			if (m_state->data_taken)
			{
				throw std::logic_error("async read result has already been taken");
			}

			m_state->data_taken = true;
			return std::move(m_state->buffer);
		}

		[[nodiscard]] std::string take_text()
		{
			const auto bytes = take_data();
			std::string text(bytes.size(), '\0');
			if (!bytes.empty())
			{
				std::memcpy(text.data(), bytes.data(), bytes.size());
			}
			return text;
		}

	private:
		explicit Request(std::shared_ptr<detail::request_state> state)
			: m_state(std::move(state))
		{
		}

		std::shared_ptr<detail::request_state> m_state;

		friend class Queue;
	};

	// Owns an SDL async I/O queue. File open and size queries are synchronous;
	// reads, writes and file closes are processed asynchronously.
	class Queue
	{
	public:
		Queue()
			: m_queue(SDL_CreateAsyncIOQueue())
		{
			if (!m_queue)
			{
				throw sdl_error("failed to create async I/O queue: ");
			}
		}

		Queue(const Queue&) = delete;
		Queue& operator=(const Queue&) = delete;
		Queue(Queue&&) = delete;
		Queue& operator=(Queue&&) = delete;

		~Queue() noexcept
		{
			shutdown();
		}

		[[nodiscard]] Request read_file(const std::filesystem::path& path)
		{
			std::lock_guard queue_lock(m_mutex);
			ensure_open();

			const auto utf8_path = detail::path_to_utf8(path);
			auto* file = SDL_AsyncIOFromFile(utf8_path.c_str(), "r");
			if (!file)
			{
				throw sdl_error("failed to open file for async reading: ");
			}
			detail::async_file_guard file_guard{ file, m_queue };

			const auto file_size = SDL_GetAsyncIOSize(file);
			if (file_size < 0)
			{
				throw sdl_error("failed to get file size for async reading: ");
			}
			return start_read(
				file,
				0,
				static_cast<std::uint64_t>(file_size),
				file_guard);
		}

		// Reads up to size bytes beginning at offset. Reading past end-of-file is
		// allowed; inspect bytes_transferred for the actual amount returned.
		[[nodiscard]] Request read_range(
			const std::filesystem::path& path,
			std::uint64_t offset,
			std::uint64_t size)
		{
			std::lock_guard queue_lock(m_mutex);
			ensure_open();
			validate_range(offset, size);

			const auto utf8_path = detail::path_to_utf8(path);
			auto* file = SDL_AsyncIOFromFile(utf8_path.c_str(), "r");
			if (!file)
			{
				throw sdl_error("failed to open file for async reading: ");
			}
			detail::async_file_guard file_guard{ file, m_queue };
			return start_read(file, offset, size, file_guard);
		}

		[[nodiscard]] Request write_file(
			const std::filesystem::path& path,
			std::vector<std::byte> contents,
			bool flush_on_close = false)
		{
			std::lock_guard queue_lock(m_mutex);
			ensure_open();

			const auto utf8_path = detail::path_to_utf8(path);
			auto* file = SDL_AsyncIOFromFile(utf8_path.c_str(), "w");
			if (!file)
			{
				throw sdl_error("failed to open file for async writing: ");
			}
			detail::async_file_guard file_guard{ file, m_queue };
			return start_write(file, 0, std::move(contents), flush_on_close, file_guard);
		}

		// Writes at offset without truncating the existing file. The file must
		// already exist; creating a new file is provided by write_file().
		[[nodiscard]] Request write_range(
			const std::filesystem::path& path,
			std::uint64_t offset,
			std::vector<std::byte> contents,
			bool flush_on_close = false)
		{
			std::lock_guard queue_lock(m_mutex);
			ensure_open();
			validate_range(offset, static_cast<std::uint64_t>(contents.size()));

			const auto utf8_path = detail::path_to_utf8(path);
			auto* file = SDL_AsyncIOFromFile(utf8_path.c_str(), "r+");
			if (!file)
			{
				throw sdl_error("failed to open existing file for async range writing: ");
			}
			detail::async_file_guard file_guard{ file, m_queue };
			return start_write(file, offset, std::move(contents), flush_on_close, file_guard);
		}

		[[nodiscard]] Request write_file(
			const std::filesystem::path& path,
			std::span<const std::byte> contents,
			bool flush_on_close = false)
		{
			return write_file(
				path,
				std::vector<std::byte>(contents.begin(), contents.end()),
				flush_on_close);
		}

		[[nodiscard]] Request write_range(
			const std::filesystem::path& path,
			std::uint64_t offset,
			std::span<const std::byte> contents,
			bool flush_on_close = false)
		{
			return write_range(
				path,
				offset,
				std::vector<std::byte>(contents.begin(), contents.end()),
				flush_on_close);
		}

		[[nodiscard]] Request write_text_file(
			const std::filesystem::path& path,
			std::string_view contents,
			bool flush_on_close = false)
		{
			const auto bytes = std::as_bytes(std::span(contents.data(), contents.size()));
			return write_file(path, bytes, flush_on_close);
		}

		[[nodiscard]] Request write_text_range(
			const std::filesystem::path& path,
			std::uint64_t offset,
			std::string_view contents,
			bool flush_on_close = false)
		{
			const auto bytes = std::as_bytes(std::span(contents.data(), contents.size()));
			return write_range(path, offset, bytes, flush_on_close);
		}

		// Collects ready SDL outcomes without blocking. Returns the number of
		// requests that became final during this call.
		[[nodiscard]] std::size_t poll()
		{
			std::lock_guard queue_lock(m_mutex);
			ensure_open();

			std::size_t completed_requests = 0;
			SDL_AsyncIOOutcome outcome{};
			while (SDL_GetAsyncIOResult(m_queue, &outcome))
			{
				process_outcome(outcome, completed_requests);
			}

			retry_closes();
			return completed_requests;
		}

		// Blocks for at most timeout and processes outcomes like poll(). A zero
		// timeout is a non-blocking check; larger values are clamped to SDL's limit.
		[[nodiscard]] std::size_t wait(std::chrono::milliseconds timeout)
		{
			if (timeout.count() < 0)
			{
				throw std::invalid_argument("async I/O wait timeout cannot be negative");
			}

			std::lock_guard queue_lock(m_mutex);
			ensure_open();
			if (m_requests.empty())
			{
				return 0;
			}
			retry_closes();

			const auto max_timeout = static_cast<std::int64_t>(std::numeric_limits<Sint32>::max());
			const auto timeout_ms = static_cast<Sint32>(
				std::min<std::int64_t>(timeout.count(), max_timeout));
			SDL_AsyncIOOutcome outcome{};
			std::size_t completed_requests = 0;
			if (SDL_WaitAsyncIOResult(m_queue, &outcome, timeout_ms))
			{
				process_outcome(outcome, completed_requests);
				while (SDL_GetAsyncIOResult(m_queue, &outcome))
				{
					process_outcome(outcome, completed_requests);
				}
			}

			retry_closes();
			return completed_requests;
		}

		[[nodiscard]] std::size_t pending_count() const
		{
			std::lock_guard lock(m_mutex);
			return m_requests.size();
		}

	private:
		static void validate_range(std::uint64_t offset, std::uint64_t size)
		{
			if (size > std::numeric_limits<std::uint64_t>::max() - offset)
			{
				throw std::length_error("async I/O range overflows the file offset type");
			}
			if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
			{
				throw std::length_error("async I/O range is too large to fit in memory");
			}
		}

		void ensure_open() const
		{
			if (!m_queue)
			{
				throw std::logic_error("async I/O queue is closed");
			}
		}

		Request start_read(
			SDL_AsyncIO* file,
			std::uint64_t offset,
			std::uint64_t size,
			detail::async_file_guard& file_guard)
		{
			validate_range(offset, size);
			auto state = std::make_shared<detail::request_state>();
			state->file = file;
			state->type = operation::read;
			state->offset = offset;
			state->bytes_requested = size;
			state->buffer.resize(static_cast<std::size_t>(size));
			m_requests.emplace(state.get(), state);
			file_guard.release();

			if (size == 0)
			{
				state->io_done = true;
				state->io_succeeded = true;
			}
			else if (!SDL_ReadAsyncIO(file, state->buffer.data(), offset, size, m_queue, state.get()))
			{
				state->io_done = true;
				state->io_succeeded = false;
				state->error = detail::sdl_error_message("failed to queue async file read: ");
			}

			try_queue_close(state);
			return Request(std::move(state));
		}

		Request start_write(
			SDL_AsyncIO* file,
			std::uint64_t offset,
			std::vector<std::byte> contents,
			bool flush_on_close,
			detail::async_file_guard& file_guard)
		{
			const auto size = static_cast<std::uint64_t>(contents.size());
			validate_range(offset, size);
			auto state = std::make_shared<detail::request_state>();
			state->file = file;
			state->type = operation::write;
			state->offset = offset;
			state->bytes_requested = size;
			state->buffer = std::move(contents);
			state->flush_on_close = flush_on_close;
			m_requests.emplace(state.get(), state);
			file_guard.release();

			if (state->buffer.empty())
			{
				state->io_done = true;
				state->io_succeeded = true;
			}
			else if (!SDL_WriteAsyncIO(
				file,
				state->buffer.data(),
				offset,
				size,
				m_queue,
				state.get()))
			{
				state->io_done = true;
				state->io_succeeded = false;
				state->error = detail::sdl_error_message("failed to queue async file write: ");
			}

			try_queue_close(state);
			return Request(std::move(state));
		}

		void retry_closes() noexcept
		{
			for (auto& [_, state] : m_requests)
			{
				bool should_retry_close = false;
				{
					std::lock_guard state_lock(state->mutex);
					should_retry_close = state->io_done && state->file && !state->close_queued;
				}
				if (should_retry_close)
				{
					try_queue_close(state);
				}
			}
		}

		void try_queue_close(const std::shared_ptr<detail::request_state>& state) noexcept
		{
			std::lock_guard state_lock(state->mutex);
			if (!state->file || state->close_queued || !m_queue)
			{
				return;
			}

			if (SDL_CloseAsyncIO(state->file, state->flush_on_close, m_queue, state.get()))
			{
				// A successful SDL_CloseAsyncIO invalidates the handle immediately.
				state->file = nullptr;
				state->close_queued = true;
			}
		}

		void process_outcome(const SDL_AsyncIOOutcome& outcome, std::size_t& completed_requests)
		{
			const auto found = m_requests.find(outcome.userdata);
			if (found == m_requests.end())
			{
				return;
			}

			const auto state = found->second;
			{
				std::lock_guard state_lock(state->mutex);
				if (outcome.type == SDL_ASYNCIO_TASK_READ || outcome.type == SDL_ASYNCIO_TASK_WRITE)
				{
					state->io_done = true;
					state->bytes_transferred = outcome.bytes_transferred;
					state->io_succeeded = outcome.result == SDL_ASYNCIO_COMPLETE;
					state->io_cancelled = outcome.result == SDL_ASYNCIO_CANCELED;
					if (state->type == operation::read &&
						outcome.bytes_transferred <= state->buffer.size())
					{
						state->buffer.resize(static_cast<std::size_t>(outcome.bytes_transferred));
					}
					if (!state->io_succeeded && state->error.empty())
					{
						state->error = state->io_cancelled
							? "async file operation was cancelled"
							: "async file operation failed";
					}
				}
				else if (outcome.type == SDL_ASYNCIO_TASK_CLOSE)
				{
					state->close_done = true;
					state->close_succeeded = outcome.result == SDL_ASYNCIO_COMPLETE;
					state->close_cancelled = outcome.result == SDL_ASYNCIO_CANCELED;
					if (!state->close_succeeded && state->error.empty())
					{
						state->error = state->close_cancelled
							? "async file close was cancelled"
							: "async file close failed";
					}
				}
			}

			bool retry_close = false;
			bool final = false;
			{
				std::lock_guard state_lock(state->mutex);
				retry_close = state->io_done && state->file && !state->close_queued;
				final = state->io_done && state->close_done;
				if (final)
				{
					if (state->io_cancelled || state->close_cancelled)
					{
						state->status = request_status::cancelled;
					}
					else if (state->io_succeeded && state->close_succeeded)
					{
						state->status = request_status::completed;
					}
					else
					{
						state->status = request_status::failed;
						if (state->error.empty())
						{
							state->error = "async file request failed";
						}
					}
				}
			}

			if (retry_close)
			{
				try_queue_close(state);
			}
			if (final)
			{
				m_requests.erase(found);
				++completed_requests;
			}
		}

		void shutdown() noexcept
		{
			std::lock_guard queue_lock(m_mutex);
			if (!m_queue)
			{
				return;
			}

			for (auto& [_, state] : m_requests)
			{
				try_queue_close(state);
			}

			SDL_DestroyAsyncIOQueue(std::exchange(m_queue, nullptr));
			for (auto& [_, state] : m_requests)
			{
				std::lock_guard state_lock(state->mutex);
				state->file = nullptr;
				if (state->status == request_status::pending)
				{
					state->status = request_status::failed;
					state->error = "async I/O queue was destroyed before the request was collected";
				}
			}
			m_requests.clear();
		}

		SDL_AsyncIOQueue* m_queue = nullptr;
		mutable std::mutex m_mutex;
		std::unordered_map<void*, std::shared_ptr<detail::request_state>> m_requests;
	};
}
