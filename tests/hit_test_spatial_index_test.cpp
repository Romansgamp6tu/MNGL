#include <MNGL/hit_test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
	using namespace mngl::detail;

	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void populate_frame(hit_test_frame& frame, std::size_t item_count)
	{
		frame.target_size = {1000, 1000};
		frame.view_projection = glm::mat4(1.0f);
		frame.view_projection[0][0] = 0.002f;
		frame.view_projection[1][1] = 0.002f;
		frame.view_projection[3][0] = -1.0f;
		frame.view_projection[3][1] = -1.0f;
		frame.items.reserve(item_count);
		for (std::size_t index = 0; index < item_count; ++index)
		{
			const float x = static_cast<float>(index % 100) * 10.0f;
			const float y = static_cast<float>(index / 100) * 10.0f;
			constexpr float width = 4.0f;
			hit_test_item item;
			item.draw_index = index;
			item.destination = {{0.0f, 0.0f}, {width, width}};
			item.world_to_local = glm::mat4(1.0f);
			item.world_to_local[0][0] = 1.0f / width;
			item.world_to_local[1][1] = 1.0f / width;
			item.world_to_local[3][0] = -x / width;
			item.world_to_local[3][1] = -y / width;
			item.inverse_valid = true;
			item.world_bounds = {{x, y}, {width, width}};
			item.world_bounds_valid = true;
			frame.items.push_back(std::move(item));
		}
	}

	hit_test_item make_item(std::size_t draw_index, mngl::rectangle bounds)
	{
		hit_test_item item;
		item.draw_index = draw_index;
		item.destination = {{0.0f, 0.0f}, bounds.size};
		item.world_to_local = glm::mat4(1.0f);
		item.world_to_local[0][0] = 1.0f / bounds.size.x;
		item.world_to_local[1][1] = 1.0f / bounds.size.y;
		item.world_to_local[3][0] = -bounds.pos.x / bounds.size.x;
		item.world_to_local[3][1] = -bounds.pos.y / bounds.size.y;
		item.inverse_valid = true;
		item.world_bounds = bounds;
		item.world_bounds_valid = true;
		return item;
	}

	hit_test_item make_transformed_item(std::size_t draw_index, float x, float y, float width, float height,
	                                   float radians)
	{
		const float cosine = std::cos(radians);
		const float sine = std::sin(radians);
		glm::mat4 local_to_world(1.0f);
		local_to_world[0][0] = cosine * width;
		local_to_world[0][1] = sine * width;
		local_to_world[1][0] = -sine * height;
		local_to_world[1][1] = cosine * height;
		local_to_world[3][0] = x;
		local_to_world[3][1] = y;

		hit_test_item item;
		item.draw_index = draw_index;
		item.destination = {{0.0f, 0.0f}, {width, height}};
		item.world_to_local = glm::inverse(local_to_world);
		item.inverse_valid = true;
		const auto bounds = transformed_bounds(local_to_world, {{0.0f, 0.0f}, {1.0f, 1.0f}});
		if (!bounds)
			throw std::runtime_error("Could not calculate transformed test item bounds");
		item.world_bounds = *bounds;
		item.world_bounds_valid = true;
		return item;
	}

	bool equivalent(const std::optional<mngl::hit_test_result>& left,
	                const std::optional<mngl::hit_test_result>& right)
	{
		if (left.has_value() != right.has_value())
			return false;
		if (!left)
			return true;
		if (left->path.size() != right->path.size())
			return false;
		for (std::size_t index = 0; index < left->path.size(); ++index)
		{
			const auto& a = left->path[index];
			const auto& b = right->path[index];
			if (a.draw_index != b.draw_index || std::abs(a.local_coords.x - b.local_coords.x) > 1e-4f ||
			    std::abs(a.local_coords.y - b.local_coords.y) > 1e-4f ||
			    a.source_identity != b.source_identity || a.source_size.x != b.source_size.x ||
			    a.source_size.y != b.source_size.y ||
			    std::abs(a.source_pixel_coords.x - b.source_pixel_coords.x) > 1e-4f ||
			    std::abs(a.source_pixel_coords.y - b.source_pixel_coords.y) > 1e-4f ||
			    a.svg_element.has_value() != b.svg_element.has_value())
			{
				return false;
			}
			if (a.svg_element && (a.svg_element->id != b.svg_element->id || a.svg_element->name != b.svg_element->name ||
			                      a.svg_element->index != b.svg_element->index))
				return false;
		}
		return left->click_callback == right->click_callback &&
		       left->callback_path_index == right->callback_path_index;
	}

	void verify_equivalence_order_scissor_and_nested_target()
	{
		hit_test_frame frame;
		populate_frame(frame, 1024);
		auto callback_count = std::make_shared<std::size_t>(0);
		auto top_item = make_item(1024, {{0.0f, 0.0f}, {8.0f, 8.0f}});
		top_item.click_callback = std::make_shared<const mngl::hit_test_callback>(
		    [callback_count](const mngl::hit_test_result&, const mngl::hit_test_path_entry&) { ++*callback_count; });
		frame.items.push_back(std::move(top_item));
		auto clipped_top = make_item(1025, {{0.0f, 0.0f}, {8.0f, 8.0f}});
		clipped_top.scissor = glm::ivec4{0, 0, 100, 500};
		frame.items.push_back(std::move(clipped_top));
		auto unbounded = make_item(1026, {{20.0f, 20.0f}, {8.0f, 8.0f}});
		unbounded.world_bounds_valid = false;
		frame.items.push_back(std::move(unbounded));

		const std::array queries{glm::vec2{3.0f, 3.0f}, glm::vec2{7.0f, 7.0f}, glm::vec2{502.0f, 202.0f},
		                         glm::vec2{23.0f, 23.0f}, glm::vec2{999.0f, 999.0f}, glm::vec2{1200.0f, 20.0f}};
		for (const auto query : queries)
		{
			const auto linear = hit_test_frame_at_pixel(frame, query, hit_test_query_mode::scan_without_bounds);
			const auto indexed = hit_test_frame_at_pixel(frame, query, hit_test_query_mode::indexed);
			if (!equivalent(linear, indexed))
			{
				std::cerr << "Mismatch at pixel (" << query.x << ", " << query.y << "): linear="
				          << (linear ? std::to_string(linear->path.back().draw_index) : "miss") << ", indexed="
				          << (indexed ? std::to_string(indexed->path.back().draw_index) : "miss") << '\n';
				throw std::runtime_error("BVH and linear hit-test results differ");
			}
		}
		const auto scissored = hit_test_frame_at_pixel(frame, {3.0f, 3.0f}, hit_test_query_mode::indexed);
		require(scissored && scissored->path.size() == 1 && scissored->path.front().draw_index == 1024,
		        "Indexed hit testing must preserve topmost order after a scissor rejection");
		const auto scissored_linear =
		    hit_test_frame_at_pixel(frame, {3.0f, 3.0f}, hit_test_query_mode::scan_without_bounds);
		require(scissored_linear && scissored_linear->invoke_click_callback() &&
		            scissored->invoke_click_callback() && *callback_count == 2,
		        "Linear and indexed results must preserve the selected item's click callback");

		auto child = std::make_shared<hit_test_frame>();
		child->target_size = {64, 64};
		child->view_projection = glm::mat4(1.0f);
		child->view_projection[0][0] = 2.0f / 64.0f;
		child->view_projection[1][1] = 2.0f / 64.0f;
		child->view_projection[3][0] = -1.0f;
		child->view_projection[3][1] = -1.0f;
		child->items.push_back(make_item(0, {{16.0f, 16.0f}, {32.0f, 32.0f}}));

		hit_test_frame parent;
		parent.target_size = {64, 64};
		parent.view_projection = child->view_projection;
		auto nested = make_item(0, {{0.0f, 0.0f}, {64.0f, 64.0f}});
		nested.source_size = {64, 64};
		nested.child_frame = child;
		parent.items.push_back(std::move(nested));
		const auto nested_linear =
		    hit_test_frame_at_pixel(parent, {32.0f, 32.0f}, hit_test_query_mode::scan_without_bounds);
		const auto nested_indexed = hit_test_frame_at_pixel(parent, {32.0f, 32.0f}, hit_test_query_mode::indexed);
		require(equivalent(nested_linear, nested_indexed), "Nested-target results differ with the BVH enabled");
		require(nested_indexed && nested_indexed->path.size() == 2,
		        "The broadphase must preserve nested render-target hit paths");
	}

	void verify_dense_overlap_fallback()
	{
		hit_test_frame frame;
		populate_frame(frame, 1000);
		for (std::size_t index = 0; index < frame.items.size(); ++index)
		{
			frame.items[index] = make_item(index, {{400.0f, 400.0f}, {200.0f, 200.0f}});
			frame.items[index].shape = hit_test_shape::circle;
		}
		require(hit_test_spatial_index_ready(frame, hit_test_query_mode::indexed),
		        "Dense test frame must build its spatial index exactly once");
		const auto candidates = hit_test_candidates_at(frame, {401.0f, 401.0f});
		require(candidates.overflow && candidates.inline_count == hit_test_candidate_list::inline_capacity,
		        "Dense overlap must stop candidate collection at the bounded shortlist limit");
		const auto miss_baseline =
		    hit_test_frame_at_pixel(frame, {401.0f, 401.0f}, hit_test_query_mode::scan_without_bounds);
		const auto miss_indexed = hit_test_frame_at_pixel(frame, {401.0f, 401.0f}, hit_test_query_mode::indexed);
		require(equivalent(miss_baseline, miss_indexed) && !miss_indexed,
		        "Dense overlap fallback must preserve exact misses after the shortlist overflows");
		const auto hit_baseline =
		    hit_test_frame_at_pixel(frame, {500.0f, 500.0f}, hit_test_query_mode::scan_without_bounds);
		const auto hit_indexed = hit_test_frame_at_pixel(frame, {500.0f, 500.0f}, hit_test_query_mode::indexed);
		require(equivalent(hit_baseline, hit_indexed) && hit_indexed &&
		            hit_indexed->path.back().draw_index == 999,
		        "The topmost exact overlap must return without scanning or changing draw order");
	}

	void verify_transformed_bounds()
	{
		hit_test_frame frame;
		frame.target_size = {1000, 1000};
		frame.view_projection = glm::mat4(1.0f);
		frame.view_projection[0][0] = 0.002f;
		frame.view_projection[1][1] = 0.002f;
		frame.view_projection[3][0] = -1.0f;
		frame.view_projection[3][1] = -1.0f;
		constexpr float radians_per_degree = 0.01745329251994329577f;
		for (std::size_t index = 0; index < 512; ++index)
		{
			const float x = static_cast<float>(index % 24) * 35.0f + 30.0f;
			const float y = static_cast<float>(index / 24) * 35.0f + 30.0f;
			const float width = 4.0f + static_cast<float>(index % 7);
			const float height = 3.0f + static_cast<float>(index % 5);
			const float angle = static_cast<float>((index * 19) % 360) * radians_per_degree;
			frame.items.push_back(make_transformed_item(index, x, y, width, height, angle));
		}
		frame.items.push_back(make_transformed_item(512, 500.0f, 500.0f, 18.0f, 12.0f, 0.45f));
		frame.items.push_back(make_transformed_item(513, 500.0f, 500.0f, 18.0f, 12.0f, 0.45f));

		for (std::size_t index = 0; index < 512; index += 7)
		{
			const auto& item = frame.items[index];
			const auto local_center = glm::vec4(0.5f, 0.5f, 0.0f, 1.0f);
			const auto world_center = glm::vec2(glm::inverse(item.world_to_local) * local_center);
			const auto linear =
			    hit_test_frame_at_pixel(frame, world_center, hit_test_query_mode::scan_without_bounds);
			const auto indexed = hit_test_frame_at_pixel(frame, world_center, hit_test_query_mode::indexed);
			require(equivalent(linear, indexed), "Rotated/scaled transformed bounds must remain conservative");
		}

		const auto overlapped = frame.items.back().world_to_local;
		const auto world_center = glm::vec2(glm::inverse(overlapped) * glm::vec4(0.5f, 0.5f, 0.0f, 1.0f));
		const auto linear_top =
		    hit_test_frame_at_pixel(frame, world_center, hit_test_query_mode::scan_without_bounds);
		const auto indexed_top = hit_test_frame_at_pixel(frame, world_center, hit_test_query_mode::indexed);
		if (!equivalent(linear_top, indexed_top) || !indexed_top || indexed_top->path.back().draw_index != 513)
		{
			std::cerr << "Transformed overlap: linear="
			          << (linear_top ? std::to_string(linear_top->path.back().draw_index) : "miss")
			          << ", indexed="
			          << (indexed_top ? std::to_string(indexed_top->path.back().draw_index) : "miss")
			          << ", world=" << world_center.x << ',' << world_center.y << '\n';
			throw std::runtime_error("Reverse draw order must be preserved for overlapping transformed items");
		}
	}

	void verify_adaptive_build_threshold()
	{
		hit_test_frame frame;
		populate_frame(frame, 1000);
		const auto query_threshold = hit_test_spatial_index_query_threshold_for_items(frame.items.size());
		for (std::size_t query = 0; query + 1 < query_threshold; ++query)
		{
			(void)hit_test_frame_at_pixel(frame, {5.0f, 5.0f});
			require(!frame.spatial_index_usable && frame.spatial_index_build_count == 0,
			        "A short-lived large snapshot should avoid index construction for its first queries");
		}
		(void)hit_test_frame_at_pixel(frame, {5.0f, 5.0f});
		require(frame.spatial_index_usable,
		        "Repeated queries on a large snapshot should build the broadphase index after the threshold");
		require(frame.spatial_index_build_count == 1,
		        "The adaptive threshold must enter the one-time index builder exactly once");
		require(frame.spatial_index_query_count.load(std::memory_order_relaxed) ==
		            query_threshold,
		        "Large snapshots must defer the one-time index build until the configured query threshold");

		hit_test_frame very_large;
		populate_frame(very_large, 10000);
		const auto large_threshold = hit_test_spatial_index_query_threshold_for_items(very_large.items.size());
		#if defined(_DEBUG) || !defined(NDEBUG)
		require(large_threshold == 256 && query_threshold == 224,
		        "Debug per-size thresholds must follow measured Debug build/query crossover costs");
		#else
		require(large_threshold == 128 && query_threshold == 160,
		        "Release per-size thresholds must follow measured Release build/query crossover costs");
		#endif
		for (std::size_t query = 0; query + 1 < large_threshold; ++query)
		{
			(void)hit_test_frame_at_pixel(very_large, {5.0f, 5.0f});
			require(!very_large.spatial_index_usable && very_large.spatial_index_build_count == 0,
			        "The 10k-item snapshot must defer its index until the higher query threshold");
		}
		(void)hit_test_frame_at_pixel(very_large, {5.0f, 5.0f});
		require(very_large.spatial_index_usable && very_large.spatial_index_build_count == 1 &&
		            very_large.spatial_index_query_count.load(std::memory_order_relaxed) == large_threshold,
		        "The 10k-item index must build exactly after its configured query threshold");
	}

	void verify_concurrent_lazy_build()
	{
		hit_test_frame frame;
		populate_frame(frame, 1000);
		std::atomic_bool incorrect_result = false;
		std::array<std::thread, 8> workers;
		for (std::size_t worker = 0; worker < workers.size(); ++worker)
		{
			workers[worker] = std::thread(
			    [&frame, &incorrect_result, worker]
			    {
				    for (std::size_t query = 0; query < 40; ++query)
				    {
					    const auto item = (worker * 127 + query * 17) % frame.items.size();
					    const glm::vec2 pixel{static_cast<float>(item % 100) * 10.0f + 2.0f,
					                         static_cast<float>(item / 100) * 10.0f + 2.0f};
					    const auto result = hit_test_frame_at_pixel(frame, pixel);
					    if (!result || result->path.back().draw_index != item)
						    incorrect_result.store(true, std::memory_order_relaxed);
				    }
			    });
		}
		for (auto& worker : workers)
			worker.join();
		require(!incorrect_result.load(std::memory_order_relaxed),
		        "Concurrent adaptive queries returned a different hit during lazy index publication");
		require(frame.spatial_index_usable && !frame.spatial_index_nodes.empty(),
		        "Concurrent repeated queries must safely publish one usable broadphase index");
		require(frame.spatial_index_build_count == 1,
		        "Concurrent readers must execute the lazy BVH builder once via call_once");
		require(frame.spatial_index_query_count.load(std::memory_order_relaxed) == workers.size() * 40,
		        "Concurrent query accounting lost updates around lazy index construction");
	}

	using clock = std::chrono::steady_clock;

	struct timings
	{
		double build_ms = 0.0;
		double scan_hit_ms = 0.0;
		double bounds_hit_ms = 0.0;
		double indexed_hit_ms = 0.0;
		double scan_miss_ms = 0.0;
		double bounds_miss_ms = 0.0;
		double indexed_miss_ms = 0.0;
		double overlap_build_ms = 0.0;
		double overlap_hit_scan_ms = 0.0;
		double overlap_hit_index_ms = 0.0;
		double overlap_miss_scan_ms = 0.0;
		double overlap_miss_index_ms = 0.0;
	};

	timings benchmark_size(std::size_t item_count)
	{
		hit_test_frame frame;
		populate_frame(frame, item_count);
		const auto build_start = clock::now();
		require(hit_test_spatial_index_ready(frame, hit_test_query_mode::indexed),
		        "Benchmark frame must construct the index before query timing");
		const auto build_end = clock::now();

		constexpr std::size_t sample_count = 300;
		std::vector<glm::vec2> hit_queries;
		hit_queries.reserve(sample_count);
		for (std::size_t query = 0; query < sample_count; ++query)
		{
			const auto item = (query * (item_count / sample_count)) % item_count;
			hit_queries.emplace_back(static_cast<float>(item % 100) * 10.0f + 2.0f,
			                         static_cast<float>(item / 100) * 10.0f + 2.0f);
		}
		std::vector<glm::vec2> miss_queries(sample_count, {7.0f, 7.0f});

		std::size_t sink = 0;
		const auto measure = [&](const hit_test_frame& target, const std::vector<glm::vec2>& queries,
		                         hit_test_query_mode mode)
		{
			const auto begin = clock::now();
			for (const auto query : queries)
			{
				const auto result = hit_test_frame_at_pixel(target, query, mode);
				if (result)
					sink += result->path.back().draw_index + 1;
			}
			return std::chrono::duration<double, std::milli>(clock::now() - begin).count();
		};
		(void)measure(frame, hit_queries, hit_test_query_mode::scan_without_bounds);
		(void)measure(frame, hit_queries, hit_test_query_mode::indexed);
		const auto scan_hit = measure(frame, hit_queries, hit_test_query_mode::scan_without_bounds);
		const auto bounds_hit = measure(frame, hit_queries, hit_test_query_mode::linear);
		const auto indexed_hit = measure(frame, hit_queries, hit_test_query_mode::indexed);
		const auto scan_miss = measure(frame, miss_queries, hit_test_query_mode::scan_without_bounds);
		const auto bounds_miss = measure(frame, miss_queries, hit_test_query_mode::linear);
		const auto indexed_miss = measure(frame, miss_queries, hit_test_query_mode::indexed);

		hit_test_frame overlap;
		populate_frame(overlap, item_count);
		for (std::size_t index = 0; index < overlap.items.size(); ++index)
		{
			overlap.items[index] = make_item(index, {{400.0f, 400.0f}, {200.0f, 200.0f}});
			overlap.items[index].shape = hit_test_shape::circle;
		}
		const auto overlap_build_start = clock::now();
		(void)hit_test_spatial_index_ready(overlap, hit_test_query_mode::indexed);
		const auto overlap_build_end = clock::now();
		std::vector<glm::vec2> overlap_hit_queries(sample_count, {500.0f, 500.0f});
		std::vector<glm::vec2> overlap_miss_queries(sample_count, {401.0f, 401.0f});
		const auto overlap_hit_scan = measure(overlap, overlap_hit_queries, hit_test_query_mode::scan_without_bounds);
		const auto overlap_hit_index = measure(overlap, overlap_hit_queries, hit_test_query_mode::indexed);
		const auto overlap_miss_scan = measure(overlap, overlap_miss_queries, hit_test_query_mode::scan_without_bounds);
		const auto overlap_miss_index = measure(overlap, overlap_miss_queries, hit_test_query_mode::indexed);
		require(sink != 0, "Benchmark queries were optimized away or did not hit any item");
		return {std::chrono::duration<double, std::milli>(build_end - build_start).count(), scan_hit, bounds_hit,
		        indexed_hit, scan_miss, bounds_miss, indexed_miss,
		        std::chrono::duration<double, std::milli>(overlap_build_end - overlap_build_start).count(),
		        overlap_hit_scan, overlap_hit_index, overlap_miss_scan, overlap_miss_index};
	}
} // namespace

int main()
{
	try
	{
		verify_equivalence_order_scissor_and_nested_target();
		verify_transformed_bounds();
		verify_dense_overlap_fallback();
		verify_adaptive_build_threshold();
		verify_concurrent_lazy_build();
		std::cout << "Hit-test broadphase timings (ms; 300 sparse hit/miss/overlap queries; scan shares cached inverse)\n"
		          << "items build scan-hit aabb-hit index-hit scan-miss aabb-miss index-miss overlap-build "
		             "overlap-hit-scan overlap-hit-index overlap-miss-scan overlap-miss-index\n";
		for (const auto item_count : {std::size_t{1000}, std::size_t{10000}})
		{
			const auto result = benchmark_size(item_count);
			std::cout << item_count << ' ' << result.build_ms << ' ' << result.scan_hit_ms << ' '
			          << result.bounds_hit_ms << ' ' << result.indexed_hit_ms << ' ' << result.scan_miss_ms << ' '
			          << result.bounds_miss_ms << ' ' << result.indexed_miss_ms << ' ' << result.overlap_build_ms << ' '
			          << result.overlap_hit_scan_ms << ' ' << result.overlap_hit_index_ms << ' '
			          << result.overlap_miss_scan_ms << ' ' << result.overlap_miss_index_ms << '\n';
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Hit-test spatial index regression failed: " << error.what() << '\n';
		return 1;
	}
}
