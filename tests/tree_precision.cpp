#define APP_FLAGS 0
#include "tree_renderer.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>

using namespace mngl::two_d;

static void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

static tree_draw_options practicum_options()
{
	tree_draw_options options;
	options.node_size = { 156.0f, 54.0f };
	options.horizontal_gap = 36.0f;
	options.vertical_gap = 42.0f;
	options.edge_width = 2.0f;
	options.border_width = 1.5f;
	options.corner_radius = 7.0f;
	options.value_text_height = 16.0f;
	options.metadata_text_height = 10.5f;
	return options;
}

static void check_clipping()
{
	const tree_draw_area viewport{ 100'000'000.25, -100'000'000.125, 1024.0, 512.0 };
	const glm::dvec2 origin = viewport.pos + viewport.size * 0.5;
	glm::dvec2 start = origin - glm::dvec2(1e8, 1e8);
	glm::dvec2 end = origin + glm::dvec2(1e8, 1e8);
	require(detail::clip_tree_segment(start, end, viewport), "Crossing edge was lost");
	const auto local_start = detail::tree_local_position(start, origin);
	const auto local_end = detail::tree_local_position(end, origin);
	require(local_start == glm::vec2(-256.0f) && local_end == glm::vec2(256.0f),
		"Long edge was not clipped before FP32 conversion");
	start = viewport.pos - glm::dvec2(10.0, 1.0);
	end = start + glm::dvec2(2000.0, 0.0);
	require(!detail::clip_tree_segment(start, end, viewport), "Outside parallel edge survived");
	require(detail::clip_tree_segment(start, end, viewport, 2.0), "Stroke padding was lost");
	start = end = origin;
	require(detail::clip_tree_segment(start, end, viewport), "Inside point was lost");
	start = origin + glm::dvec2(1e8, 0.0);
	end = origin - glm::dvec2(1e8, 0.0);
	require(detail::clip_tree_segment(start, end, viewport), "Reversed edge was lost");
	require(detail::tree_local_position(start, origin).x == 512.0f &&
		detail::tree_local_position(end, origin).x == -512.0f, "Reversed edge clipped incorrectly");
}

static tree_draw_model make_model(std::size_t count, bool chain)
{
	tree_draw_model model;
	model.root = 0;
	model.nodes.resize(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		auto& node = model.nodes[i];
		node.keys.push_back(std::to_string(i));
		if (chain && i + 1 < count)
			node.children = { tree_draw_model::no_node, i + 1 };
		else if (!chain && i * 2 + 1 < count)
			node.children = { i * 2 + 1,
				i * 2 + 2 < count ? i * 2 + 2 : tree_draw_model::no_node };
	}
	return model;
}

static void compare_visibility(const tree_draw_visibility& first, const tree_draw_visibility& second)
{
	require(first.nodes == second.nodes, "Culling nodes depend on absolute world position");
	require(first.leaf_link_sources == second.leaf_link_sources, "Leaf-link culling changed");
	require(first.edges.size() == second.edges.size(), "Culling edges depend on absolute position");
	for (std::size_t i = 0; i < first.edges.size(); ++i)
		require(first.edges[i].parent == second.edges[i].parent &&
			first.edges[i].child == second.edges[i].child &&
			first.edges[i].child_slot == second.edges[i].child_slot, "Culled edge changed");
}

static std::vector<unsigned char> read_pixels(mngl::texture& target)
{
	auto& device = mngl::gpu::device::global();
	const auto dimensions = target.raw_gpu_texture().get_size();
	SDL_GPUTransferBufferCreateInfo info{};
	info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	info.size = dimensions.x * dimensions.y * 4;
	const auto release = [&device](SDL_GPUTransferBuffer* value)
		{ SDL_ReleaseGPUTransferBuffer(device.raw(), value); };
	std::unique_ptr<SDL_GPUTransferBuffer, decltype(release)> transfer(
		SDL_CreateGPUTransferBuffer(device.raw(), &info), release);
	require(transfer != nullptr, SDL_GetError());
	mngl::gpu::command_buffer command(device);
	auto* pass = SDL_BeginGPUCopyPass(command.raw());
	require(pass != nullptr, SDL_GetError());
	SDL_GPUTextureRegion source{};
	source.texture = target.raw_gpu_texture().raw();
	source.w = dimensions.x;
	source.h = dimensions.y;
	source.d = 1;
	SDL_GPUTextureTransferInfo destination{};
	destination.transfer_buffer = transfer.get();
	SDL_DownloadFromGPUTexture(pass, &source, &destination);
	SDL_EndGPUCopyPass(pass);
	command.submit();
	require(device.wait(), SDL_GetError());
	const auto* mapped = static_cast<const unsigned char*>(
		SDL_MapGPUTransferBuffer(device.raw(), transfer.get(), false));
	require(mapped != nullptr, SDL_GetError());
	std::vector<unsigned char> pixels(mapped, mapped + info.size);
	SDL_UnmapGPUTransferBuffer(device.raw(), transfer.get());
	return pixels;
}

static void save_image(const std::filesystem::path& file, std::vector<unsigned char>& pixels)
{
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
		SDL_CreateSurfaceFrom(1024, 512, SDL_PIXELFORMAT_RGBA32, pixels.data(), 1024 * 4),
		SDL_DestroySurface);
	require(surface != nullptr, SDL_GetError());
	require(IMG_SavePNG(surface.get(), file.string().c_str()), SDL_GetError());
}

static void compare_rendering(const tree_draw_model& model, const tree_layout& layout,
	glm::dvec2 camera, tree_draw_options options, const std::filesystem::path& file)
{
	auto& device = mngl::gpu::device::global();
	font typeface(device, MNGL_TEST_FONT_FILE);
	mngl::texture target(device, 1024, 512);
	std::vector<unsigned char> baseline;
	tree_draw_culler culler;
	for (const glm::dvec2 translation : { glm::dvec2(0.0), glm::dvec2(100'000'000.25, -100'000'000.125) })
	{
		const tree_draw_area area{ translation - camera, layout.size };
		const tree_draw_area view{ translation - glm::dvec2(512.0, 256.0), { 1024.0, 512.0 } };
		const auto visible = culler.cull(model, layout, area, view, options);
		require(!visible.nodes.empty(), "Render comparison contains no visible nodes");
		options.coordinate_origin = translation;
		options.clip_bounds = view;
		options.visibility = &visible;
		auto frame = target.frame(mngl::color(23, 27, 34));
		frame.draw([&](Frame2D& drawing)
		{
			glm::mat4 projection(1.0f);
			projection[0][0] = 2.0f / 1024.0f;
			projection[1][1] = 2.0f / 512.0f;
			drawing.set_view_projection(projection);
			draw_tree_model_parallel(drawing, typeface, model, layout, area, options);
		});
		frame.display();
		auto pixels = read_pixels(target);
		if (baseline.empty()) baseline = std::move(pixels);
		else
		{
			require(pixels == baseline, "GPU pixels changed after translating the world by 1e8");
			save_image(file, pixels);
		}
		options.visibility = nullptr;
	}
}

static void check_large_tree(bool chain, const std::filesystem::path& render_directory)
{
	constexpr std::size_t count = 1'000'000;
	auto model = make_model(count, chain);
	const auto options = practicum_options();
	const auto layout = measure_tree_layout(model, options);
	const auto last = layout.nodes.back().center;
	if (chain)
	{
		require(layout.size.x == 156.0 + 192.0 * (count - 1), "Chain width lost precision");
		require(layout.size.y == 54.0 + 96.0 * (count - 1), "Chain height lost precision");
		require(last.x == layout.size.x - 78.0 && last.y == 27.0, "Chain endpoint moved");
	}
	else
	{
		require(layout.nodes[count - 2].center.x - layout.nodes[count - 3].center.x == 192.0,
			"Distant sibling spacing lost precision");
	}
	const auto residual = detail::tree_local_position(last, last + glm::dvec2(0.25, 0.125));
	require(residual == glm::vec2(-0.25f, -0.125f), "Subpixel camera movement was lost");

	tree_draw_culler culler;
	const tree_draw_area area{ { 0.0, 0.0 }, layout.size };
	const tree_draw_area viewport{ last - glm::dvec2(512.0, 256.0), { 1024.0, 512.0 } };
	const auto visible = culler.cull(model, layout, area, viewport, options);
	require(std::find(visible.nodes.begin(), visible.nodes.end(), count - 1) != visible.nodes.end(),
		"Far end of million-node tree cannot be selected");
	const glm::dvec2 shift(100'000'000.25, -100'000'000.125);
	const tree_draw_area moved_area{ area.pos + shift, area.size };
	const tree_draw_area moved_view{ viewport.pos + shift, viewport.size };
	compare_visibility(visible, culler.cull(model, layout, moved_area, moved_view, options));

	const auto retained = std::make_shared<const int>(1);
	const auto id = culler.request_async(retained, model, layout, moved_area, moved_view, options);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	for (;;)
	{
		if (auto completed = culler.take_completed_result())
		{
			require(completed->request_id == id && !completed->error, "Async culling failed");
			compare_visibility(visible, completed->visibility);
			break;
		}
		require(std::chrono::steady_clock::now() < deadline, "Async culling timed out");
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!render_directory.empty())
		compare_rendering(model, layout, last, options,
			render_directory / (chain ? "chain-million.png" : "balanced-million.png"));
	std::cout << (chain ? "chain" : "balanced") << ": 1,000,000 nodes passed\n";
}

static void check_b_plus(const std::filesystem::path& render_directory)
{
	tree_draw_model model;
	model.root = 0;
	model.nodes.resize(3);
	model.nodes[0].keys = { "100", "200" };
	model.nodes[0].children = { 1, 2 };
	model.nodes[0].style = tree_node_style::multiway_internal;
	model.nodes[0].width = 156.0f;
	model.nodes[0].cell_widths = { 68.25f, 87.75f };
	model.nodes[1].keys = { "0", "50", "99" };
	model.nodes[1].next_leaf = 2;
	model.nodes[2].keys = { "100", "150", "200" };
	for (const auto i : { 1, 2 })
	{
		model.nodes[i].style = tree_node_style::multiway_leaf;
		model.nodes[i].width = 156.0f;
	}
	const auto options = practicum_options();
	const auto layout = measure_tree_layout(model, options);
	tree_draw_culler culler;
	const tree_draw_area area{ { 0.0, 0.0 }, layout.size };
	const tree_draw_area view{ -100.0, -100.0, 1024.0, 512.0 };
	const auto visible = culler.cull(model, layout, area, view, options);
	require(visible.nodes.size() == 3 && visible.leaf_link_sources == std::vector<std::size_t>{ 1 },
		"B+ cells or leaf link disappeared");
	const glm::dvec2 shift(100'000'000.25, -100'000'000.125);
	compare_visibility(visible, culler.cull(model, layout,
		{ area.pos + shift, area.size }, { view.pos + shift, view.size }, options));
	if (!render_directory.empty())
		compare_rendering(model, layout, layout.size * 0.5, options, render_directory / "b-plus.png");
	std::cout << "B+ cells and leaf links passed\n";
}

static void check_parallel_rendering(const std::filesystem::path& directory)
{
	// More than 512 visible labels exercises prepared glyphs and worker recording.
	auto model = make_model(600, true);
	auto options = practicum_options();
	options.node_size = { 22.0f, 12.0f };
	options.border_width = 0.5f;
	options.edge_width = 0.5f;
	options.corner_radius = 2.0f;
	options.value_text_height = 7.0f;
	tree_layout layout;
	layout.size = { 1000.0, 480.0 };
	layout.nodes.resize(model.nodes.size());
	for (std::size_t i = 0; i < model.nodes.size(); ++i)
	{
		model.nodes[i].keys = { std::to_string(i % 10) };
		layout.nodes[i].center = { 20.0 + (i % 40) * 24.0, 20.0 + (i / 40) * 30.0 };
		layout.nodes[i].size = options.node_size;
		layout.nodes[i].cell_widths = { 22.0f };
	}
	compare_rendering(model, layout, layout.size * 0.5, options, directory / "parallel-labels.png");
	std::cout << "Parallel label recording passed\n";
}

int main(int argc, char** argv)
{
	try
	{
		std::filesystem::path render_directory;
		if (argc == 3 && std::string_view(argv[1]) == "--render")
		{
			render_directory = argv[2];
			std::filesystem::create_directories(render_directory);
			require(SDL_InitSubSystem(SDL_INIT_VIDEO), SDL_GetError());
		}
		check_clipping();
		check_b_plus(render_directory);
		check_large_tree(false, render_directory);
		check_large_tree(true, render_directory);
		if (!render_directory.empty()) check_parallel_rendering(render_directory);
		std::cout << "Tree precision checks passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
