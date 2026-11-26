#include <MNGL/async_io.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	class temporary_directory
	{
	public:
		temporary_directory()
		{
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			m_path = std::filesystem::temp_directory_path() /
			         ("mngl-async-io-close-retry-" + std::to_string(stamp));
			std::filesystem::create_directories(m_path);
		}

		~temporary_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(m_path, ignored);
		}

		[[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

	private:
		std::filesystem::path m_path;
	};

	void close_failure_is_retried_without_losing_request()
	{
		temporary_directory temporary;
		const auto path = temporary.path() / "retry.bin";
		std::vector<std::byte> expected(32 * 1024);
		for (std::size_t index = 0; index < expected.size(); ++index)
			expected[index] = static_cast<std::byte>((index * 37) & 0xff);
		{
			std::ofstream file(path, std::ios::binary);
			file.write(reinterpret_cast<const char*>(expected.data()),
			           static_cast<std::streamsize>(expected.size()));
			require(file.good(), "Could not prepare async I/O retry fixture");
		}

		mngl::async_io::queue queue;
		mngl::async_io::detail::async_io_close_test_hooks::fail_next_close_attempts(3);
		auto request = queue.read_file(path);
		const auto outcome = queue.wait(request);

		require(outcome.has_value(), "A transient close failure must not fail a completed read");
		require(outcome->info.status == mngl::async_io::request_status::completed,
		        "The retried read must end in the completed state");
		require(outcome->data == expected, "Retrying close must preserve the asynchronous read buffer");
		require(mngl::async_io::detail::async_io_close_test_hooks::injected_failures.load(
		            std::memory_order_relaxed) == 0,
		        "The test must consume all injected close failures");
		require(mngl::async_io::detail::async_io_close_test_hooks::close_attempts.load(
		            std::memory_order_relaxed) >= 4,
		        "The failed close must be retried after the I/O completion and retry-queue insertion");
		require(queue.pending_count() == 0, "A retried close must eventually retire its request");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		close_failure_is_retried_without_losing_request();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Async I/O close retry regression failed: " << error.what() << '\n';
		return 1;
	}
}
