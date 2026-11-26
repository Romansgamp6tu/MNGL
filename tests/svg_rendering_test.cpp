#include <MNGL/svg_sprite.hpp>
#include <MNGL/detail/two_d/svg_effect_renderer.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <utility>

namespace
{
	namespace svg = mngl::two_d::detail::svg;

	void require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	float area(const svg::geometry_data& part)
	{
		const auto triangles = part.triangle_view();
		float total = 0;
		for (std::size_t i = 0; i < triangles.size(); i += 3)
		{
			const auto u = triangles[i + 1] - triangles[i];
			const auto v = triangles[i + 2] - triangles[i];
			total += std::abs(u.x * v.y - u.y * v.x) / 2;
		}
		return total;
	}

	bool mesh_contains(const svg::geometry_data& part, glm::vec2 point)
	{
		const auto triangles = part.triangle_view();
		constexpr float epsilon = 1.0e-5f;
		const auto cross = [](glm::vec2 left, glm::vec2 right)
		{ return left.x * right.y - left.y * right.x; };
		for (std::size_t index = 0; index + 2 < triangles.size(); index += 3)
		{
			const auto a = triangles[index];
			const auto b = triangles[index + 1];
			const auto c = triangles[index + 2];
			const float ab = cross(b - a, point - a);
			const float bc = cross(c - b, point - b);
			const float ca = cross(a - c, point - c);
			if ((ab >= -epsilon && bc >= -epsilon && ca >= -epsilon) ||
			    (ab <= epsilon && bc <= epsilon && ca <= epsilon))
				return true;
		}
		return false;
	}

	void require_same_path_geometry(std::string_view left, std::string_view right)
	{
		const auto left_contours = svg::parse_path(left);
		const auto right_contours = svg::parse_path(right);
		require(left_contours.size() == right_contours.size(), "Equivalent arc paths must have the same contours");
		for (std::size_t i = 0; i < left_contours.size(); ++i)
		{
			const auto& left_points = left_contours[i].points;
			const auto& right_points = right_contours[i].points;
			require(left_points.size() == right_points.size(), "Equivalent arc paths must have the same point count");
			for (std::size_t j = 0; j < left_points.size(); ++j)
			{
				require(left_points[j].x == right_points[j].x && left_points[j].y == right_points[j].y,
				        "Equivalent arc paths must produce identical points");
			}
		}
	}

	void compact_arc_flags()
	{
		require_same_path_geometry("M2 15 A10 10 0 0118 15 Z", "M2 15 A10 10 0 0 1 18 15 Z");
		require_same_path_geometry("m2 15 a10 10 0 0118 0", "m2 15 a10 10 0 0 1 18 0");
		require_same_path_geometry("M2 15 A10 10 0 0118 15 10 10 0 10 28 15",
		                           "M2 15 A10 10 0 0 1 18 15 10 10 0 1 0 28 15");

		const auto compact_relative = svg::parse_path("m2 15 a10 10 0 0118 0");
		require(compact_relative.size() == 1 && compact_relative.front().points.size() > 2 &&
		            compact_relative.front().points.front() == glm::vec2{2, 15} &&
		            compact_relative.front().points.back() == glm::vec2{20, 15} &&
		            std::ranges::any_of(compact_relative.front().points,
		                                [](glm::vec2 point) { return std::abs(point.y - 15.0f) > 0.01f; }),
		        "A compact relative arc flag pair must emit a curved segment ending at the requested point");
		const auto compact_two_arcs = svg::parse_path("M2 15 A10 10 0 0118 15 10 10 0 10 28 15");
		require(compact_two_arcs.size() == 1 && compact_two_arcs.front().points.size() > 4 &&
		            compact_two_arcs.front().points.back() == glm::vec2{28, 15} &&
		            std::ranges::any_of(compact_two_arcs.front().points,
		                                [](glm::vec2 point) { return std::abs(point.y - 15.0f) > 0.01f; }),
		        "Adjacent compact arc flag pairs must emit both arcs and reach the final endpoint");

		for (const auto invalid_path : {"M2 15 A10 10 0 2 1 18 15", "M2 15 A10 10 0 0 +1 18 15",
		                               "M2 15 A10 10 0 0.0 1 18 15"})
		{
			bool rejected = false;
			try
			{
				(void)svg::parse_path(invalid_path);
			}
			catch (const std::invalid_argument&)
			{
				rejected = true;
			}
			require(rejected, "Arc flags must be one 0 or 1 character");
		}
	}

	void require_same_path_contours(const std::vector<svg::contour>& left,
	                                const std::vector<svg::contour>& right)
	{
		require(left.size() == right.size(), "Equivalent post-close paths must have the same contour count");
		for (std::size_t i = 0; i < left.size(); ++i)
		{
			require(left[i].closed == right[i].closed, "Equivalent paths must retain contour closure state");
			require(left[i].points.size() == right[i].points.size(), "Equivalent paths must have equal point counts");
			for (std::size_t j = 0; j < left[i].points.size(); ++j)
			{
				require(left[i].points[j] == right[i].points[j], "Equivalent paths must have equal points");
			}
			require(left[i].marker_vertices.size() == right[i].marker_vertices.size(),
			        "Equivalent paths must retain the same marker vertices");
			for (std::size_t j = 0; j < left[i].marker_vertices.size(); ++j)
			{
				const auto& a = left[i].marker_vertices[j];
				const auto& b = right[i].marker_vertices[j];
				require(a.point == b.point && a.incoming == b.incoming && a.outgoing == b.outgoing,
				        "Equivalent paths must retain the same marker tangents");
			}
		}
	}

	void require_same_draw_geometry(const svg::document_data& left, const svg::document_data& right)
	{
		require(left.geometry.size() == right.geometry.size(), "Equivalent paths must emit the same paint and markers");
		for (std::size_t i = 0; i < left.geometry.size(); ++i)
		{
			const auto& a = left.geometry[i];
			const auto& b = right.geometry[i];
			const auto a_triangles = a.triangle_view();
			const auto b_triangles = b.triangle_view();
			require(a_triangles.size() == b_triangles.size(), "Equivalent paths must emit equal fill/stroke geometry");
			for (std::size_t j = 0; j < a_triangles.size(); ++j)
			{
				require(a_triangles[j] == b_triangles[j], "Equivalent paths must emit equal triangle vertices");
			}
			for (glm::length_t column = 0; column < 4; ++column)
			{
				for (glm::length_t row = 0; row < 4; ++row)
				{
					require(a.transform[column][row] == b.transform[column][row],
					        "Equivalent paths must place markers at the same transforms");
				}
			}
		}
	}

	void post_closepath_continuation()
	{
		const auto require_continuation_endpoints = [](const std::vector<svg::contour>& contours,
		                                              glm::vec2 expected_end, const char* message)
		{
			require(contours.size() == 2 && !contours[1].marker_vertices.empty() &&
			            contours[1].marker_vertices.front().point == glm::vec2{2, 2} &&
			            contours[1].marker_vertices.back().point == expected_end,
			        message);
		};
		const auto absolute = svg::parse_path("M2 2 H10 V10 Z L18 2 L18 10", 0.25f, 1'000'000, true);
		const auto absolute_with_move = svg::parse_path("M2 2 H10 V10 Z M2 2 L18 2 L18 10", 0.25f,
		                                               1'000'000, true);
		require_same_path_contours(absolute, absolute_with_move);
		require(absolute.size() == 2 && absolute[1].marker_vertices.front().point == glm::vec2{2, 2} &&
		            absolute[1].marker_vertices.back().point == glm::vec2{18, 10},
		        "An absolute command after closepath must start at the closed subpath's initial point");

		const auto relative = svg::parse_path("M2 2 h8 v8 z l8 0 l0 8", 0.25f, 1'000'000, true);
		const auto relative_with_move = svg::parse_path("M2 2 h8 v8 z m0 0 l8 0 l0 8", 0.25f,
		                                               1'000'000, true);
		require_same_path_contours(relative, relative_with_move);
		require_continuation_endpoints(relative, {10, 10},
		                               "Relative commands after closepath must continue from the subpath start");

		const auto smooth_cubic = svg::parse_path("M2 2 C4 2 6 2 8 2 Z S10 0 12 2", 0.25f, 1'000'000, true);
		const auto smooth_cubic_with_move =
		    svg::parse_path("M2 2 C4 2 6 2 8 2 Z M2 2 S10 0 12 2", 0.25f, 1'000'000, true);
		require_same_path_contours(smooth_cubic, smooth_cubic_with_move);
		require_continuation_endpoints(smooth_cubic, {12, 2},
		                               "A smooth cubic after closepath must start at the closed subpath origin");
		const auto smooth_quadratic = svg::parse_path("M2 2 Q4 2 6 2 Z T10 2", 0.25f, 1'000'000, true);
		const auto smooth_quadratic_with_move =
		    svg::parse_path("M2 2 Q4 2 6 2 Z M2 2 T10 2", 0.25f, 1'000'000, true);
		require_same_path_contours(smooth_quadratic, smooth_quadratic_with_move);
		require_continuation_endpoints(smooth_quadratic, {10, 2},
		                               "A smooth quadratic after closepath must start at the closed subpath origin");

		const auto parse_document = [](std::string_view path)
		{
			std::string source = R"svg(<svg viewBox="0 0 32 32"><defs>
			    <marker id="arrow" markerWidth="4" markerHeight="4" markerUnits="userSpaceOnUse" orient="auto">
			        <path d="M0 0 L4 2 L0 4 Z"/>
			    </marker>
			</defs><path id="continued" fill="red" stroke="blue" stroke-width="2"
			    marker-start="url(#arrow)" marker-mid="url(#arrow)" marker-end="url(#arrow)" d=")svg";
			source.append(path);
			source += R"svg("/></svg>)svg";
			return svg::parse(source);
		};
		const auto implicit_draw = parse_document("M2 2 H10 V10 Z L18 2 L18 10");
		const auto explicit_move = parse_document("M2 2 H10 V10 Z M2 2 L18 2 L18 10");
		require_same_draw_geometry(implicit_draw, explicit_move);
		require(implicit_draw.geometry.size() > 2, "The continued path must retain fill, stroke, and marker parts");
		const auto relative_implicit_draw = parse_document("M2 2 h8 v8 z l8 0 l0 8");
		const auto relative_explicit_move = parse_document("M2 2 h8 v8 z m0 0 l8 0 l0 8");
		require_same_draw_geometry(relative_implicit_draw, relative_explicit_move);
	}

	bool same_mesh(const svg::geometry_data& first, const svg::geometry_data& second)
	{
		const auto first_triangles = first.triangle_view();
		const auto second_triangles = second.triangle_view();
		if (first_triangles.size() != second_triangles.size())
			return false;
		for (std::size_t index = 0; index < first_triangles.size(); ++index)
		{
			if (glm::length(first_triangles[index] - second_triangles[index]) > 0.0001f)
				return false;
		}
		return true;
	}

	void intersecting_fills()
	{
		const auto bow = svg::parse(R"svg(<svg><path d="M0 0 L10 10 L0 10 L10 0 Z"/></svg>)svg");
		require(bow.geometry.size() == 1 && area(bow.geometry[0]) == 50, "Both lobes of a bow tie must fill");
		const auto even_odd_bow =
		    svg::parse(R"svg(<svg><path fill-rule="evenodd" d="M0 0 L10 10 L0 10 L10 0 Z"/></svg>)svg");
		require(even_odd_bow.geometry.size() == 1 && area(even_odd_bow.geometry[0]) == 50,
		        "Evenodd must fill both lobes of a self-intersecting bow tie");
		for (const auto* bow_lobes : {&bow.geometry.front(), &even_odd_bow.geometry.front()})
			require(mesh_contains(*bow_lobes, {2, 8}) && mesh_contains(*bow_lobes, {8, 2}),
			        "Both interior lobes of a self-intersecting bow tie must contain fill triangles");

		const auto overlap = svg::parse(R"svg(<svg>
		    <path d="M0 0 H10 V10 H0 Z M5 0 H15 V10 H5 Z"/>
		    <path fill-rule="evenodd" d="M0 0 H10 V10 H0 Z M5 0 H15 V10 H5 Z"/>
		    <path d="M0 0 H10 V10 H0 Z M5 0 V10 H15 V0 Z"/>
		    </svg>)svg");
		require(overlap.geometry.size() == 3, "All three compound paths should have visible regions");
		require(area(overlap.geometry[0]) == 150, "Nonzero must fill overlap once");
		require(area(overlap.geometry[1]) == 100, "Evenodd must cut out the overlap");
		require(area(overlap.geometry[2]) == 100, "Opposite winding must cancel the overlap");
		require(mesh_contains(overlap.geometry[0], {2, 5}) && mesh_contains(overlap.geometry[0], {7, 5}) &&
		            mesh_contains(overlap.geometry[0], {12, 5}),
		        "Nonzero fill must cover both outer strips and the overlapping interior");
		require(mesh_contains(overlap.geometry[1], {2, 5}) && !mesh_contains(overlap.geometry[1], {7, 5}) &&
		            mesh_contains(overlap.geometry[1], {12, 5}),
		        "Evenodd fill must leave the overlap empty while retaining both outer strips");
		require(mesh_contains(overlap.geometry[2], {2, 5}) && !mesh_contains(overlap.geometry[2], {7, 5}) &&
		            mesh_contains(overlap.geometry[2], {12, 5}),
		        "Opposite winding must cancel the overlap while retaining both outer strips");

		const auto touching = svg::parse(R"svg(<svg>
		    <path id="edge-nonzero-same" d="M0 0 H10 V10 H0 Z M10 0 H20 V10 H10 Z"/>
		    <path id="edge-nonzero-opposite" d="M0 0 H10 V10 H0 Z M10 0 V10 H20 V0 Z"/>
		    <path id="edge-evenodd-same" fill-rule="evenodd"
		          d="M0 0 H10 V10 H0 Z M10 0 H20 V10 H10 Z"/>
		    <path id="edge-evenodd-opposite" fill-rule="evenodd"
		          d="M0 0 H10 V10 H0 Z M10 0 V10 H20 V0 Z"/>
		    <path id="point-nonzero-same" d="M0 0 H10 V10 H0 Z M10 10 H20 V20 H10 Z"/>
		    <path id="point-nonzero-opposite" d="M0 0 H10 V10 H0 Z M10 10 V20 H20 V10 Z"/>
		    <path id="point-evenodd-same" fill-rule="evenodd"
		          d="M0 0 H10 V10 H0 Z M10 10 H20 V20 H10 Z"/>
		    <path id="point-evenodd-opposite" fill-rule="evenodd"
		          d="M0 0 H10 V10 H0 Z M10 10 V20 H20 V10 Z"/>
		    </svg>)svg");
		require(touching.geometry.size() == 8,
		        "Contours sharing a boundary edge or vertex must remain compound fill geometry");
		for (std::size_t index = 0; index < touching.geometry.size(); ++index)
		{
			const auto& part = touching.geometry[index];
			require(area(part) == 200,
			        "Touching contours must preserve the combined area under either fill rule and winding");
			const auto second_center = index < 4 ? glm::vec2{15, 5} : glm::vec2{15, 15};
			require(mesh_contains(part, {5, 5}) && mesh_contains(part, second_center),
			        "Both contours must retain filled interior points when they touch at an edge or vertex");
		}
	}

	void zero_length_path_strokes()
	{
		const auto rounded = svg::parse(R"svg(<svg>
		    <path id="move-only" fill="none" stroke="red" stroke-width="6" stroke-linecap="round"
		          d="M10 10"/>
		    <path id="zero-line" fill="none" stroke="red" stroke-width="6" stroke-linecap="round"
		          d="M20 10L20 10"/>
		    <path id="zero-close" fill="none" stroke="red" stroke-width="6" stroke-linecap="round"
		          d="M30 10Z"/>
		    </svg>)svg");
		require(rounded.geometry.size() == 2,
		        "A move-only subpath must have no hit geometry while explicit zero-length segments keep round caps");
		require(rounded.geometry[0].svg_element.id == "zero-line" &&
		            rounded.geometry[1].svg_element.id == "zero-close",
		        "Only explicitly drawn zero-length subpaths must retain their stroke geometry");
		for (const auto& part : rounded.geometry)
			require(!part.triangle_view().empty() && area(part) > 0.0f,
			        "Round caps on zero-length drawn segments must create a visible and hittable region");

		const auto butt = svg::parse(R"svg(<svg>
		    <path id="move-only" fill="none" stroke="red" stroke-width="6" d="M10 10"/>
		    <path id="zero-line" fill="none" stroke="red" stroke-width="6" d="M20 10L20 10"/>
		    <path id="zero-close" fill="none" stroke="red" stroke-width="6" d="M30 10Z"/>
		    </svg>)svg");
		require(butt.geometry.empty(), "Butt caps must not create visible geometry for zero-length subpaths");

		const auto square = svg::parse(R"svg(<svg>
		    <path id="move-only" fill="none" stroke="red" stroke-width="6" stroke-linecap="square"
		          d="M10 10"/>
		    <path id="zero-line" fill="none" stroke="red" stroke-width="6" stroke-linecap="square"
		          d="M20 10L20 10"/>
		    <path id="zero-close" fill="none" stroke="red" stroke-width="6" stroke-linecap="square"
		          d="M30 10Z"/>
		    </svg>)svg");
		require(square.geometry.size() == 2 && square.geometry[0].svg_element.id == "zero-line" &&
		            square.geometry[1].svg_element.id == "zero-close",
		        "Square caps must create zero-length drawn segments while leaving move-only paths empty");
		for (const auto& part : square.geometry)
			require(!part.triangle_view().empty() && area(part) > 0.0f,
			        "Square caps on zero-length drawn segments must produce a visible hittable region");
	}

	void rectangle_corner_radii()
	{
		const auto document = svg::parse(R"svg(<svg viewBox="0 0 60 20">
		    <rect id="explicit-zero" width="20" height="20" rx="0" ry="5"/>
		    <rect id="ry-only" x="20" width="20" height="20" ry="5"/>
		    <rect id="both-radii" x="40" width="20" height="20" rx="5" ry="5"/>
		    </svg>)svg");
		require(document.geometry.size() == 3, "All rectangle radius cases must produce geometry");
		const auto explicit_zero = area(document.geometry[0]);
		const auto ry_only = area(document.geometry[1]);
		const auto both_radii = area(document.geometry[2]);
		require(explicit_zero == 400, "An explicit zero rx must preserve sharp rectangle corners");
		require(ry_only < 400, "An omitted rx must inherit the nonzero ry");
		require(std::abs(ry_only - both_radii) < 0.001f,
		        "An omitted rx must match an rx copied from ry");

		const auto percent = svg::parse(R"svg(<svg viewBox="0 0 100 100">
		    <rect width="40" height="20" rx="25%" ry="25%"/>
		    </svg>)svg");
		const auto absolute = svg::parse(R"svg(<svg viewBox="0 0 100 100">
		    <rect width="40" height="20" rx="10" ry="5"/>
		    </svg>)svg");
		require(same_mesh(percent.geometry.front(), absolute.geometry.front()),
		        "Rectangle radius percentages must resolve against that rectangle's width and height");

		const auto css = svg::parse(R"svg(<svg viewBox="0 0 100 100">
		    <style>.rounded { rx: 25%; ry: 25%; }</style>
		    <rect class="rounded" width="40" height="20" rx="0" ry="0"/>
		    </svg>)svg");
		require(same_mesh(percent.geometry.front(), css.geometry.front()),
		        "CSS radius percentages must override presentation attributes and use rect dimensions");

		const auto css_zero_radius = svg::parse(R"svg(<svg viewBox="0 0 100 100">
		    <style>.square { rx: 0; ry: 25%; }</style>
		    <rect class="square" width="40" height="20"/>
		    </svg>)svg");
		require(area(css_zero_radius.geometry.front()) == 800,
		        "An explicit zero CSS radius must keep the rectangle corners sharp");

		const auto percent_clamp = svg::parse(R"svg(<svg viewBox="0 0 100 100">
		    <rect width="40" height="20" rx="100%" ry="100%"/>
		    </svg>)svg");
		const auto absolute_clamp = svg::parse(R"svg(<svg viewBox="0 0 100 100">
		    <rect width="40" height="20" rx="20" ry="10"/>
		    </svg>)svg");
		require(same_mesh(percent_clamp.geometry.front(), absolute_clamp.geometry.front()),
		        "Percentage radii must retain the half-dimension clamp");
	}

	void nested_fill_rules()
	{
		const auto nested = svg::parse(R"svg(<svg>
		    <path d="M0 0 H12 V12 H0 Z M4 4 H8 V8 H4 Z"/>
		    <path fill-rule="evenodd" d="M0 0 H12 V12 H0 Z M4 4 H8 V8 H4 Z"/>
		    <path d="M0 0 H12 V12 H0 Z M4 4 V8 H8 V4 Z"/>
		    </svg>)svg");
		require(nested.geometry.size() == 3, "Nested contours should produce three fill regions");
		require(area(nested.geometry[0]) == 144, "Same-winding nested contours must remain filled for nonzero");
		require(area(nested.geometry[1]) == 128, "Evenodd must cut a nested hole out of the fill");
		require(area(nested.geometry[2]) == 128, "Opposite-winding nested contours must cut a nonzero hole");
	}

	std::size_t disjoint_fill_triangle_count(std::size_t count)
	{
		std::string path;
		path.reserve(count * 48);
		for (std::size_t i = 0; i < count; ++i)
		{
			path += "M" + std::to_string(i * 2) + " " + std::to_string(i) + " h1 v" +
			        std::to_string(count * 2) + " h-1 Z ";
		}
		const auto document = svg::parse("<svg><path d=\"" + path + "\"/></svg>");
		require(document.geometry.size() == 1, "A compound disjoint path must stay one paint part");
		return document.geometry.front().triangle_view().size() / 3;
	}

	void disjoint_compound_output()
	{
		const auto triangles_512 = disjoint_fill_triangle_count(512);
		const auto triangles_1024 = disjoint_fill_triangle_count(1024);
		std::cout << "SVG disjoint fill triangles: 512=" << triangles_512 << ", 1024=" << triangles_1024
		          << '\n';
		require(triangles_512 == 2 * 512, "Each disjoint rectangle should emit only two triangles at N=512");
		require(triangles_1024 == 2 * 1024,
		        "Each disjoint rectangle should emit only two triangles at N=1024");

		constexpr std::size_t band_count = 32;
		constexpr float extent = 2.0f * band_count;
		std::string diagonal_bands;
		for (std::size_t index = 0; index < band_count; ++index)
		{
			const float offset = static_cast<float>(index);
			diagonal_bands += std::format("M0 {} L{} {} L{} {} L0 {} Z ", offset, extent, extent + offset,
			                              extent, extent + offset + 0.25f, offset + 0.25f);
		}
		const float expected_area = static_cast<float>(band_count) * extent * 0.25f;
		for (const auto fill_rule : {"nonzero", "evenodd"})
		{
			const auto diagonal = svg::parse(std::format(
			    R"svg(<svg><path fill-rule="{}" d="{}"/></svg>)svg", fill_rule, diagonal_bands));
			require(diagonal.geometry.size() == 1,
			        "Parallel disjoint diagonal contours must remain one compound fill part");
			const auto triangle_count = diagonal.geometry.front().triangle_view().size() / 3;
			require(triangle_count <= 8 * band_count,
			        "Disjoint diagonal contours with overlapping AABBs must emit a linear number of triangles");
			require(std::abs(area(diagonal.geometry.front()) - expected_area) < 0.01f,
			        "Projection-based contour partitioning must preserve disjoint diagonal fill area");
		}
	}

	void closed_contour_dash_seam()
	{
		const auto solid = svg::parse(R"svg(<svg><rect x="16" y="16" width="24" height="24"
		    fill="none" stroke="red" stroke-width="8"/></svg>)svg");
		const auto one_dash = svg::parse(R"svg(<svg><rect x="16" y="16" width="24" height="24"
		    fill="none" stroke="red" stroke-width="8" stroke-dasharray="1000 1"/></svg>)svg");
		const auto solid_triangles = solid.geometry[0].triangle_view();
		const auto dashed_triangles = one_dash.geometry[0].triangle_view();
		require(solid.geometry.size() == 1 && one_dash.geometry.size() == 1,
		        "A dash covering a closed contour must produce one stroke mesh");
		require(!solid_triangles.empty() && dashed_triangles.size() == solid_triangles.size(),
		        "A dash covering a closed contour must match the solid stroke mesh size");
		for (std::size_t point = 0; point < solid_triangles.size(); ++point)
		{
			const auto a = solid_triangles[point];
			const auto b = dashed_triangles[point];
			require(a == b, "A dash covering a closed contour must match its solid stroke");
		}

		svg::contour rectangle{{{16, 16}, {40, 16}, {40, 40}, {16, 40}}, true, {}};
		svg::paint_state dashed_style;
		dashed_style.stroke_dasharray = {30, 10};
		dashed_style.stroke_dashoffset = 5;
		std::size_t generated_segments = 0;
		const auto runs = svg::make_stroke_paths(rectangle, dashed_style, 100, generated_segments);
		require(runs.size() >= 2, "A gap away from the contour seam must leave separate dash runs");
		const auto& seam_run = runs.front();
		require(!seam_run.closed, "A dash crossing the seam must remain open at its gap");
		const auto seam = std::find(seam_run.points.begin(), seam_run.points.end(), glm::vec2{16, 16});
		require(seam != seam_run.points.end() && seam != seam_run.points.begin() &&
		            seam != seam_run.points.end() - 1,
		        "The seam must be an interior join in the merged dash run, not a capped endpoint");
		dashed_style.stroke_width = 8.0f;
		std::vector<glm::vec2> seam_mesh;
		svg::append_stroke_geometry(seam_mesh, seam_run, dashed_style, 0.25f, 100, generated_segments);
		require(std::ranges::find(seam_mesh, glm::vec2{12, 12}) != seam_mesh.end(),
		        "The rendered mesh must include the miter tip at the merged closed-contour dash seam");
	}

	void clip_path_support()
	{
		const auto clipped = svg::parse(R"svg(<svg><defs><clipPath id="cut"><circle cx="6" cy="6" r="4"/></clipPath></defs>
		    <rect id="clipped" width="12" height="12" clip-path="url(#cut)"/></svg>)svg");
		require(clipped.geometry.size() == 1 && clipped.geometry.front().effect &&
		            !clipped.geometry.front().effect->hit_clip_triangles.empty(),
		        "A local clipPath must mask geometry and supply hit-test clipping geometry");
		require(clipped.geometry.front().effect->source.size() == 1 &&
		            clipped.geometry.front().effect->source.front().svg_element.id == "clipped",
		        "Clipped source geometry must retain its originating element");

		const auto no_clip = svg::parse(R"svg(<svg><rect width="12" height="12" clip-path="none"/></svg>)svg");
		require(no_clip.geometry.size() == 1 && !no_clip.geometry.front().effect,
		        "clip-path none must be accepted as a no-op");

		const auto object_box = svg::parse(R"svg(<svg><style>.window { clip-path: url(#cut); }</style>
		    <defs><clipPath id="cut" clipPathUnits="objectBoundingBox"><circle cx=".5" cy=".5" r=".5"/></clipPath></defs>
		    <g class="window"><rect width="10" height="6"/></g></svg>)svg");
		require(object_box.geometry.size() == 1 && object_box.geometry.front().effect &&
		            !object_box.geometry.front().effect->hit_clip_triangles.empty(),
		        "CSS group clipPath must support objectBoundingBox coordinates");

		const auto clipped_line = svg::parse(R"svg(<svg><defs><clipPath id="window">
		    <rect x="2" y="4" width="28" height="16"/></clipPath></defs>
		    <line id="stroked-line" x1="4" y1="12" x2="28" y2="12" fill="none"
		          stroke="red" stroke-width="8" clip-path="url(#window)"/></svg>)svg");
		require(clipped_line.geometry.size() == 1 && clipped_line.geometry.front().effect &&
		            clipped_line.geometry.front().effect->region.size.y > 7.0f &&
		            clipped_line.geometry.front().effect->source.size() == 1 &&
		            !clipped_line.geometry.front().effect->source.front().triangle_view().empty() &&
		            clipped_line.geometry.front().effect->source.front().svg_element.id == "stroked-line",
		        "A userSpace clipPath must retain a stroked line and allocate its nonzero paint bounds");

		const auto object_box_stroke = svg::parse(R"svg(<svg><defs><clipPath id="half" clipPathUnits="objectBoundingBox">
		    <rect width=".5" height="1"/></clipPath></defs>
		    <rect id="outlined" x="2" y="3" width="10" height="8" stroke="red" stroke-width="4"
		          clip-path="url(#half)"/></svg>)svg");
		require(object_box_stroke.geometry.size() == 1 && object_box_stroke.geometry.front().effect,
		        "An objectBoundingBox clipPath must retain a stroked target");
		const auto& object_box_layer = *object_box_stroke.geometry.front().effect;
		glm::vec2 clip_min(std::numeric_limits<float>::max());
		glm::vec2 clip_max(std::numeric_limits<float>::lowest());
		for (const auto point : object_box_layer.hit_clip_triangles)
		{
			clip_min = glm::min(clip_min, point);
			clip_max = glm::max(clip_max, point);
		}
		require(object_box_layer.region.size.x > 10.5f && object_box_layer.region.size.y > 8.5f &&
		            std::abs(clip_min.x - 2.0f) < 1.0e-4f &&
		            std::abs(clip_max.x - 7.0f) < 1.0e-4f &&
		            std::abs(clip_min.y - 3.0f) < 1.0e-4f &&
		            std::abs(clip_max.y - 11.0f) < 1.0e-4f,
		        "objectBoundingBox clipping must use geometry bounds while allocating expanded stroke paint bounds");

		const auto image = svg::parse(R"svg(<svg><defs><clipPath id="cut"><rect x="2" y="2" width="8" height="8"/></clipPath></defs>
		    <image width="12" height="12" clip-path="url(#cut)" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4////fwAJ+wP9KobjigAAAABJRU5ErkJggg=="/></svg>)svg");
		require(image.geometry.size() == 1 && image.geometry.front().effect &&
		            image.geometry.front().effect->source.size() == 1 && image.geometry.front().effect->source.front().image,
		        "clipPath must wrap an image without losing its image paint");

		const auto empty_clip = svg::parse(R"svg(<svg><defs><clipPath id="empty"/></defs>
		    <rect width="4" height="4" clip-path="url(#empty)"/></svg>)svg");
		require(empty_clip.geometry.size() == 1 && empty_clip.geometry.front().effect &&
		            empty_clip.geometry.front().effect->hit_clip_empty,
		        "An empty clipPath must exclude all corresponding hit geometry");

		const auto definition_only = svg::parse(R"svg(<svg><clipPath id="unused"><circle r="4"/></clipPath>
		    <rect width="8" height="8"/></svg>)svg");
		require(definition_only.geometry.size() == 1 && definition_only.geometry.front().svg_element.name == "rect",
		        "A clipPath definition must not paint when it appears outside defs");
	}

	void svg_clip_and_symbol_regressions()
	{
		const auto paint_independent_clip = svg::parse(R"svg(<svg viewBox="0 0 64 32"><defs>
		    <clipPath id="paint-independent"><rect x="4" y="4" width="16" height="16"
		      fill="none" stroke="black" stroke-width="8" opacity="0"/></clipPath>
		    <clipPath id="hole" clip-rule="evenodd"><path fill-rule="nonzero"
		      d="M0 0H12V12H0Z M4 4H8V8H4Z" fill="none" stroke="black" stroke-width="4"/></clipPath>
		    </defs><rect id="first" width="24" height="24" clip-path="url(#paint-independent)"/>
		    <rect id="second" x="32" width="12" height="12" clip-path="url(#hole)"/></svg>)svg");
		require(paint_independent_clip.geometry.size() == 2 &&
		            paint_independent_clip.geometry[0].effect && paint_independent_clip.geometry[1].effect,
		        "Clip silhouettes must be built from geometry even when child paint is disabled");
		require(paint_independent_clip.geometry[0].effect->mask_source.size() == 1 &&
		            area(paint_independent_clip.geometry[0].effect->mask_source.front()) == 256.0f,
		        "ClipPath child fill, stroke, and opacity must not alter its geometric silhouette");
		require(paint_independent_clip.geometry[1].effect->mask_source.size() == 1 &&
		            area(paint_independent_clip.geometry[1].effect->mask_source.front()) == 128.0f,
		        "clip-rule evenodd must cut a same-winding nested contour out of the clip");

		const auto transformed_clip = svg::parse(R"svg(<svg><defs>
		    <clipPath id="clip-transform" transform="translate(12 0)"><rect width="8" height="8"/></clipPath>
		    <clipPath id="child-transform"><rect transform="translate(12 0)" width="8" height="8"/></clipPath>
		    </defs><rect id="definition" width="32" height="8" transform="translate(5 3)"
		      clip-path="url(#clip-transform)"/>
		    <rect id="child" x="40" width="32" height="8" transform="translate(5 3)"
		      clip-path="url(#child-transform)"/></svg>)svg");
		require(transformed_clip.geometry.size() == 2 && transformed_clip.geometry[0].effect &&
		            transformed_clip.geometry[1].effect &&
		            transformed_clip.geometry[0].effect->hit_clip_triangles ==
		                transformed_clip.geometry[1].effect->hit_clip_triangles,
		        "A clipPath transform must compose with the target transform like an equivalent child transform");

		const auto symbol_overflow = svg::parse(R"svg(<svg viewBox="0 0 64 32"><defs>
		    <symbol id="tile" viewBox="0 0 10 10"><rect x="-2" y="0" width="14" height="10"/></symbol>
		    <symbol id="visible-tile" viewBox="0 0 10 10" overflow="visible">
		      <rect x="-2" y="0" width="14" height="10"/></symbol>
		    </defs><use id="hidden-use" href="#tile" x="2" y="2" width="10" height="10"/>
		    <use id="visible-use" href="#visible-tile" x="20" y="2" width="10" height="10"/></svg>)svg");
		require(symbol_overflow.geometry.size() == 2 && symbol_overflow.geometry[0].effect &&
		            !symbol_overflow.geometry[1].effect && symbol_overflow.geometry[0].effect->hit_clip,
		        "Symbol instances must clip overflow by default and honor overflow:visible");

		const auto symbol_paint_units = svg::parse(R"svg(<svg viewBox="0 0 100 100"><defs>
		    <linearGradient id="user" gradientUnits="userSpaceOnUse" x1="0%" y1="0%" x2="100%" y2="0%">
		      <stop stop-color="red"/><stop offset="1" stop-color="blue"/></linearGradient>
		    <filter id="user-filter" filterUnits="userSpaceOnUse" x="0%" y="0%" width="100%" height="100%">
		      <feOffset dx="0"/></filter>
		    <mask id="user-mask" maskUnits="userSpaceOnUse" x="0%" y="0%" width="100%" height="100%">
		      <rect x="20" y="30" width="10" height="10" fill="white"/></mask>
		    <symbol id="painted" viewBox="20 30 10 10"><rect x="20" y="30" width="10" height="10"
		      fill="url(#user)" filter="url(#user-filter)" mask="url(#user-mask)"/></symbol>
		    </defs><use href="#painted" width="10" height="10"/></svg>)svg");
		require(symbol_paint_units.geometry.size() == 1 && symbol_paint_units.geometry.front().effect &&
		            symbol_paint_units.geometry.front().effect->filter &&
		            symbol_paint_units.geometry.front().effect->mask &&
		            symbol_paint_units.geometry.front().effect->source.front().gradient,
		        "Symbol paint servers, masks, and filters must keep their own viewBox units");
		const auto& painted_layer = *symbol_paint_units.geometry.front().effect;
		require(painted_layer.filter->region == mngl::rectangle{20, 30, 10, 10} &&
		            painted_layer.mask->region == mngl::rectangle{20, 30, 10, 10} &&
		            painted_layer.source.front().gradient->parameters0 == glm::vec4(20, 30, 30, 30),
		        "User-space percentage paint coordinates must resolve against the symbol origin and size");
	}

	void path_length_calibrates_stroke_dashes()
	{
		const auto calibrated = svg::parse(R"svg(<svg><path d="M0 5H100" pathLength="10"
		    fill="none" stroke="red" stroke-dasharray="5 5"/></svg>)svg");
		const auto explicit_distances = svg::parse(R"svg(<svg><path d="M0 5H100" fill="none"
		    stroke="red" stroke-dasharray="50 50"/></svg>)svg");
		require(calibrated.geometry.size() == 1 && explicit_distances.geometry.size() == 1,
		        "A calibrated dash and its explicit equivalent must each produce one visible run");
		require(same_mesh(calibrated.geometry[0], explicit_distances.geometry[0]),
		        "pathLength must produce the same dash mesh as explicitly scaled distances");

		const auto calibrated_offset = svg::parse(R"svg(<svg><path d="M0 5H100" pathLength="10"
		    fill="none" stroke="red" stroke-dasharray="5 5" stroke-dashoffset="2"/></svg>)svg");
		const auto explicit_offset = svg::parse(R"svg(<svg><path d="M0 5H100" fill="none" stroke="red"
		    stroke-dasharray="50 50" stroke-dashoffset="20"/></svg>)svg");
		require(calibrated_offset.geometry.size() == 1 && explicit_offset.geometry.size() == 1 &&
		            same_mesh(calibrated_offset.geometry[0], explicit_offset.geometry[0]),
		        "pathLength must produce the same dash mesh with an explicitly scaled offset");

		const auto subpaths = svg::parse(R"svg(<svg><path d="M0 5H50 M0 10H50" pathLength="10"
		    fill="none" stroke="red" stroke-dasharray="5 5"/></svg>)svg");
		const auto subpath_control = svg::parse(R"svg(<svg><path d="M0 5H50 M0 10H50" fill="none"
		    stroke="red" stroke-dasharray="50 50"/></svg>)svg");
		require(subpaths.geometry.size() == subpath_control.geometry.size(),
		        "Calibration must preserve the paint-part structure across subpaths");
		for (std::size_t index = 0; index < subpaths.geometry.size(); ++index)
			require(same_mesh(subpaths.geometry[index], subpath_control.geometry[index]),
			        "Subpath meshes must use the document pathLength calibration");

		bool rejected_zero_path_length = false;
		try
		{
			(void)svg::parse(R"svg(<svg><path d="M0 5H100" pathLength="0"/></svg>)svg");
		}
		catch (const std::invalid_argument&)
		{
			rejected_zero_path_length = true;
		}
		require(rejected_zero_path_length, "A zero pathLength must be rejected");
	}

	void marker_and_stroke()
	{
		const auto document = svg::parse(R"svg(<svg viewBox="0 0 64 32">
		    <style>.curve { fill: none; stroke: url(#ink); marker: url(#arrow); }</style>
		    <defs>
		        <linearGradient id="ink" gradientUnits="userSpaceOnUse" x2="50">
		            <stop stop-color="red"/><stop offset="1" stop-color="blue"/>
		        </linearGradient>
		        <marker id="arrow" markerUnits="userSpaceOnUse" markerWidth="4" markerHeight="4"
		                orient="auto-start-reverse" refY="2">
		            <path fill="red" d="M0 0 L4 2 L0 4 Z"/>
		        </marker>
		    </defs>
		    <path class="curve" id="line" d="M5 16 C10 16 15 2 20 2 S30 16 35 16"/>
		    </svg>)svg");
		require(document.geometry.size() == 4, "Curves must get markers only at segment ends");
		require(document.geometry[0].gradient.has_value(), "Stroke must use the referenced gradient");
		require(document.geometry[1].transform[0].x < -0.99f, "Start marker must reverse");
		require(document.geometry[2].transform[0].x > 0.99f, "Middle marker must follow the smooth tangent");
		require(document.geometry[3].transform[0].x > 0.99f, "End marker must follow the end tangent");
		for (const auto& part : document.geometry)
			require(part.svg_element.id == "line", "Marker hits must identify the owning shape");

		const auto inherited_color = svg::parse(R"svg(<svg><defs color="blue">
		    <marker id="current" markerUnits="userSpaceOnUse" markerWidth="4" markerHeight="4">
		      <path d="M0 0H4V4Z" fill="currentColor"/>
		    </marker></defs>
		    <path id="colored-line" d="M10 10H20" color="red" stroke="black"
	          marker-end="url(#current)"/></svg>)svg");
		const auto marker = std::ranges::find_if(inherited_color.geometry,
		                                         [](const auto& part) { return part.svg_element.id == "colored-line" &&
			                                                            part.tint.b > 0.99f; });
		require(marker != inherited_color.geometry.end(),
		        "Marker currentColor must inherit from the definition ancestor, not the referencing path");
	}

	void css_descendant_selectors()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        .scene path { fill: #ff0000; }
		        g .shape { fill: #0000ff; }
		        #scene g.layer path.shape { fill: #00ff00; }
		    </style>
		    <g id="scene" class="scene">
		        <path id="flat" class="shape" d="M0 0 H4 V4 Z"/>
		        <g class="layer"><g><path id="deep" class="shape" d="M5 0 H9 V4 Z"/></g></g>
		        <rect id="box" class="shape" x="10" width="4" height="4"/>
		    </g>
		    </svg>)svg");
		require(document.geometry.size() == 3, "Descendant CSS rules must preserve all SVG shapes");

		const auto tint_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part.tint.rgba_glsl();
			}
			throw std::runtime_error("Expected SVG shape was not found");
		};
		const auto flat = tint_for("flat");
		const auto deep = tint_for("deep");
		const auto box = tint_for("box");
		require(flat.b > 0.95f && flat.r < 0.05f,
		        "Equal-specificity descendant selectors must follow stylesheet order");
		require(deep.g > 0.95f && deep.r < 0.05f && deep.b < 0.05f,
		        "A compound descendant chain must match through nested groups");
		require(box.b > 0.95f && box.r < 0.05f,
		        "A tag and class descendant selector must match non-path SVG shapes");

		const auto gradient = svg::parse(R"svg(<svg>
		    <style>g.theme linearGradient#paint stop.hot { stop-color: #00ff00; }</style>
		    <g class="theme"><defs><linearGradient id="paint">
		        <stop class="hot" stop-color="#ff0000"/>
		        <stop offset="1" stop-color="#0000ff"/>
		    </linearGradient></defs>
		    <rect width="4" height="4" fill="url(#paint)"/></g>
		    </svg>)svg");
		require(gradient.geometry.size() == 1 && gradient.geometry[0].gradient,
		        "Gradient stop CSS selectors must use their SVG ancestors");
		const auto stop = gradient.geometry[0].gradient->definition->stops.front().value.rgba_glsl();
		require(stop.g > 0.95f && stop.r < 0.05f,
		        "A descendant selector must override gradient stop presentation attributes");
	}

	void css_cascade_resolution_is_cached_per_tag()
	{
		svg::css_stylesheet stylesheet;
		auto root = svg::parse_xml_tree(R"svg(<svg>
		    <style>.shape { fill: #ff0000; }</style>
		    <rect id="cached" class="shape" color="blue" width="4" height="4"/>
		    </svg>)svg", &stylesheet);
		svg::link_xml_parents(root);
		svg::evaluate_css_media_conditions(stylesheet);
		const svg::xml_tag* target = nullptr;
		for (const auto& child : root.children)
		{
			if (svg::attribute_value(child.tag, "id") == "cached")
				target = &child.tag;
		}
		require(target != nullptr, "The CSS cache fixture must contain its target element");

		const auto& first = svg::resolve_css_properties(*target, stylesheet);
		require(stylesheet.resolved_properties.size() == 1,
		        "The first cascade lookup must create one per-tag entry");
		const auto& second = svg::resolve_css_properties(*target, stylesheet);
		require(&first == &second && stylesheet.resolved_properties.size() == 1,
		        "Repeated visits to one SVG source tag must reuse its immutable specified cascade");
		require(second.find("fill") != second.end() && second.at("fill").value == "#ff0000",
		        "Caching must preserve the cascade result for stylesheet declarations");
	}

	void css_structural_indexes_are_precomputed()
	{
		std::string source = "<svg>";
		constexpr std::size_t sibling_count = 4096;
		source.reserve(13 + sibling_count * 8);
		for (std::size_t index = 0; index < sibling_count; ++index)
			source += index % 2 == 0 ? "<rect/>" : "<circle/>";
		source += "</svg>";
		auto root = svg::parse_xml_tree(source);
		svg::link_xml_parents(root);
		require(root.children.size() == sibling_count,
		        "The structural selector fixture must keep every sibling in the XML tree");

		const auto& last_rect = root.children[sibling_count - 2].tag;
		const auto& last_circle = root.children.back().tag;
		require(last_rect.sibling_index == sibling_count - 1 && last_rect.sibling_index_from_end == 2 &&
		            last_rect.type_index == sibling_count / 2 && last_rect.type_index_from_end == 1,
		        "Forward, reverse, and of-type sibling positions must be linked in one tree pass");
		require(last_circle.sibling_index == sibling_count && last_circle.sibling_index_from_end == 1 &&
		            last_circle.type_index == sibling_count / 2 && last_circle.type_index_from_end == 1,
		        "Mixed element types must retain independent forward and reverse indexes");
		const auto nth_last = svg::parse_css_selector("rect:nth-last-child(2)");
		const auto nth_last_type = svg::parse_css_selector("rect:nth-last-of-type(1)");
		require(svg::matches_css_selector(nth_last, last_rect) &&
		            svg::matches_css_selector(nth_last_type, last_rect),
		        "Structural CSS selectors must use the precomputed indexes on large sibling lists");
	}

	void svg_filter_dependency_pruning()
	{
		const auto document = svg::parse(R"svg(<svg viewBox="0 0 32 32">
		    <defs><filter id="live-filter">
		      <feGaussianBlur in="SourceGraphic" stdDeviation="2" result="unused"/>
		      <feFlood flood-color="red" color-interpolation-filters="sRGB" result="flood"/>
		      <feComposite in="flood" in2="SourceGraphic" operator="over"/>
		    </filter></defs>
		    <rect width="24" height="24" fill="blue" filter="url(#live-filter)"/>
		    </svg>)svg");
		require(document.geometry.size() == 1 && document.geometry[0].effect &&
		            document.geometry[0].effect->filter,
		        "The filter liveness fixture must parse into one effect layer");
		const auto& steps = document.geometry[0].effect->filter->steps;
		const auto plan = svg::analyze_filter_dependencies(steps);
		require(steps.size() == 3 && plan.live_outputs.size() == steps.size() + 2 &&
		            !plan.live_outputs[2] && plan.live_outputs[3] && plan.live_outputs[4],
		        "Filter execution must skip an unused primitive branch and keep the final join");
		require(std::count(plan.live_outputs.begin() + 2, plan.live_outputs.end(), true) == 2,
		        "Filter pass planning must emit exactly the two live primitive outputs");
		require(steps[1].operation == svg::filter_op::flood && steps[1].input == 0 &&
		            plan.consumers[0] == 1 && plan.consumers[2] == 0 &&
		            !svg::filter_uses_primary_input(steps[1].operation),
		        "feFlood after another color space must not consume or convert the previous output");
	}

	void style_media_attributes_filter_rules()
	{
		const auto document = svg::parse(R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
		    <style media="screen">#screen { fill: #ff0000; }</style>
		    <style media="print">#print { fill: #00ff00; }</style>
		    <style media="screen">
		        @media (min-width: 20px) { #nested-screen { fill: #0000ff; } }
		        @media print { #nested-print { fill: #ffff00; } }
		    </style>
		    <rect id="screen" width="4" height="4"/>
		    <rect id="print" x="5" width="4" height="4"/>
		    <rect id="nested-screen" x="10" width="4" height="4"/>
		    <rect id="nested-print" x="15" width="4" height="4"/>
		    </svg>)svg");
		require(document.geometry.size() == 4, "Media-filtered styles must preserve all SVG shapes");

		const auto color_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part.tint.rgba_glsl();
			}
			throw std::runtime_error("Expected SVG shape was not found");
		};
		const auto screen = color_for("screen");
		const auto print = color_for("print");
		const auto nested_screen = color_for("nested-screen");
		const auto nested_print = color_for("nested-print");
		require(screen.r > 0.95f && screen.g < 0.05f,
		        "A style element with media=screen must apply on the screen medium");
		require(print.r < 0.05f && print.g < 0.05f && print.b < 0.05f,
		        "A style element with media=print must not apply on the screen medium");
		require(nested_screen.b > 0.95f && nested_screen.r < 0.05f,
		        "Nested @media rules must also satisfy their enclosing style media condition");
		require(nested_print.r < 0.05f && nested_print.g < 0.05f && nested_print.b < 0.05f,
		        "A nested print query must remain inactive inside a screen-only style element");
	}

	void presentation_attributes_are_lowest_priority_hints()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        * { fill: #0000ff; stroke: #00ff00; }
		        * { fill: #ffff00; }
		        rect.specific { fill: #ff0000; stroke: #0000ff; }
		        #important { fill: #00ff00 !important; stroke: #ff0000 !important; }
		        #inline-important { fill: #0000ff !important; stroke: #00ff00 !important; }
		    </style>
		    <rect id="hints" data-before="a" fill="#ff0000" data-between="b" stroke="#ff0000"
		          data-after="c" width="4" height="4"/>
		    <rect id="specific" class="specific" data-before="a" fill="#0000ff" stroke="#00ff00"
		          data-after="c" x="5" width="4" height="4"/>
		    <rect id="inline" data-before="a" fill="#ff0000" style="fill: #ff00ff; stroke: #00ffff"
		          stroke="#ff0000" data-after="c" x="10" width="4" height="4"/>
		    <rect id="important" data-before="a" fill="#ff0000" style="fill: #ff00ff; stroke: #00ffff"
		          stroke="#ff0000" data-after="c" x="15" width="4" height="4"/>
		    <rect id="inline-important" data-before="a" fill="#ff0000"
		          style="fill: #ff00ff !important; stroke: #00ffff !important" stroke="#ff0000"
		          x="20" width="4" height="4"/>
		    </svg>)svg");
		require(document.geometry.size() == 10, "Each hint-cascade shape must emit fill and stroke geometry");

		const auto expect_colors = [&](std::string_view id, glm::vec3 fill, glm::vec3 stroke)
		{
			std::vector<glm::vec3> colors;
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
				{
					const auto rgba = part.tint.rgba_glsl();
					colors.emplace_back(rgba.r, rgba.g, rgba.b);
				}
			}
			require(colors.size() == 2, "Each hint-cascade shape must preserve separate fill and stroke paints");
			const auto matches = [](glm::vec3 actual, glm::vec3 expected)
			{ return glm::length(actual - expected) < 0.01f; };
			require(matches(colors[0], fill) && matches(colors[1], stroke),
			        "Presentation hints must follow the CSS cascade and inline important precedence");
		};
		expect_colors("hints", {1, 1, 0}, {0, 1, 0});
		expect_colors("specific", {1, 0, 0}, {0, 0, 1});
		expect_colors("inline", {1, 0, 1}, {0, 1, 1});
		expect_colors("important", {0, 1, 0}, {1, 0, 0});
		expect_colors("inline-important", {1, 0, 1}, {0, 1, 1});
	}

	void css_attribute_selectors()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        rect[data-present] { fill: #ff0000; }
		        #specific { fill: #00ff00; }
		        rect[data-state=ready] { fill: #0000ff; }
		        rect[class~="token"] { fill: #ff00ff; }
		        rect[lang|="en"] { fill: #00ffff; }
		        rect[data-prefix^="pre"] { fill: #ffff00; }
		        rect[data-suffix$="end"] { fill: #123456; }
		        rect[data-value*="MIDDLE" i] { fill: #654321; }
		        rect[data-case="READY" i] { fill: #ffffff; }
		        rect[data-case="READY"] { fill: #ff0000; }
		        rect[data-label="lead,{tail"], rect[data-label="other"] { fill: #aabbcc; }
		    </style>
		    <rect id="exists" data-present="" width="2" height="2"/>
		    <rect id="specific" data-present="" x="3" width="2" height="2"/>
		    <rect id="equals" data-state="ready" x="6" width="2" height="2"/>
		    <rect id="includes" class="icon token" x="9" width="2" height="2"/>
		    <rect id="dash" lang="en-US" x="12" width="2" height="2"/>
		    <rect id="prefix" data-prefix="prefix-value" x="15" width="2" height="2"/>
		    <rect id="suffix" data-suffix="the-end" x="18" width="2" height="2"/>
		    <rect id="substring" data-value="a-MiDdLe-z" x="21" width="2" height="2"/>
		    <rect id="case" data-case="ready" x="24" width="2" height="2"/>
		    <rect id="comma" data-label="lead,{tail" x="27" width="2" height="2"/>
		    </svg>)svg");
		require(document.geometry.size() == 10, "Attribute selectors must preserve all matching shapes");

		const auto tint_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part.tint.rgba_glsl();
			}
			throw std::runtime_error("Expected SVG shape was not found");
		};
		const auto expect_color = [&](std::string_view id, glm::vec3 expected)
		{
			const auto actual = tint_for(id);
			require(std::abs(actual.r - expected.r) < 0.01f && std::abs(actual.g - expected.g) < 0.01f &&
			            std::abs(actual.b - expected.b) < 0.01f,
			        "CSS attribute selector produced the wrong fill");
		};
		expect_color("exists", {1, 0, 0});
		expect_color("specific", {0, 1, 0});
		expect_color("equals", {0, 0, 1});
		expect_color("includes", {1, 0, 1});
		expect_color("dash", {0, 1, 1});
		expect_color("prefix", {1, 1, 0});
		expect_color("suffix", {0x12 / 255.0f, 0x34 / 255.0f, 0x56 / 255.0f});
		expect_color("substring", {0x65 / 255.0f, 0x43 / 255.0f, 0x21 / 255.0f});
		expect_color("case", {1, 1, 1});
		expect_color("comma", {0xaa / 255.0f, 0xbb / 255.0f, 0xcc / 255.0f});
	}

	void css_media_orientation()
	{
		const auto parse_viewport = [](int width, int height)
		{
			return svg::parse(std::format(R"svg(<svg width="{}" height="{}">
			    <style>
			        @media (orientation: portrait) {{ rect {{ fill: red; }} }}
			        @media (orientation: landscape) {{ rect {{ fill: blue; }} }}
			    </style>
			    <rect width="8" height="8"/>
			    </svg>)svg", width, height));
		};
		const auto square = parse_viewport(100, 100);
		const auto wide = parse_viewport(120, 100);
		const auto tall = parse_viewport(100, 120);
		require(square.geometry.size() == 1 && wide.geometry.size() == 1 && tall.geometry.size() == 1,
		        "Both orientation rules must keep the SVG rectangle available");
		const auto square_color = square.geometry.front().tint.rgba_glsl();
		const auto wide_color = wide.geometry.front().tint.rgba_glsl();
		const auto tall_color = tall.geometry.front().tint.rgba_glsl();
		require(square_color.r > 0.95f && wide_color.b > 0.95f && tall_color.r > 0.95f,
		        "A square SVG viewport must match portrait, while wider and taller controls select their orientation");
	}

	void css_media_conditions_are_cached_with_nested_order()
	{
		const auto document = svg::parse(R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
		    <style>
		        .nested { fill: #ff0000; }
		        .inactive { fill: #ff8800; }
		        .outer-only { fill: #00ff00; }
		        @media screen and (min-width: 50px) {
		            .nested { fill: #00ff00; }
		            @media (max-width: 70px) { .nested { fill: #0000ff; } }
		            @media (min-width: 100px) { .inactive { fill: #ff00ff; } }
		        }
		        @media (max-width: 10px), (min-width: 60px) { .comma { fill: #ff00ff; } }
		        .ordered { fill: #ff0000; }
		        @media all { .ordered { fill: #00ff00; } }
		        .ordered { fill: #0000ff; }
		    </style>
		    <rect id="deep-match" class="nested" width="2" height="2"/>
		    <rect id="outer-only" class="outer-only" x="3" width="2" height="2"/>
		    <rect id="inactive" class="inactive" x="6" width="2" height="2"/>
		    <rect id="comma" class="comma" x="9" width="2" height="2"/>
		    <rect id="ordered" class="ordered" x="12" width="2" height="2"/>
		    </svg>)svg");
		require(document.geometry.size() == 5, "Media conditions must retain all stylesheet rules");
		const auto tint_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part.tint.rgba_glsl();
			}
			throw std::runtime_error("Expected SVG shape was not found");
		};
		const auto expect_color = [&](std::string_view id, glm::vec3 expected)
		{
			const auto actual = tint_for(id);
			require(std::abs(actual.r - expected.r) < .01f && std::abs(actual.g - expected.g) < .01f &&
			            std::abs(actual.b - expected.b) < .01f,
			        "Cached media conditions must preserve nested matches and source order");
		};
		expect_color("deep-match", {0, 0, 1});
		expect_color("outer-only", {0, 1, 0});
		expect_color("inactive", {0xff / 255.0f, 0x88 / 255.0f, 0});
		expect_color("comma", {1, 0, 1});
		expect_color("ordered", {0, 0, 1});
	}

	void inherited_current_color()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <g color="red" fill="currentColor" stroke="currentColor" stroke-width="2">
		        <rect id="child" color="blue" width="8" height="8"/>
		    </g>
		    </svg>)svg");
		require(document.geometry.size() == 2, "Inherited currentColor must preserve fill and stroke paint parts");
		for (const auto& part : document.geometry)
		{
			const auto actual = part.tint.rgba_glsl();
			require(part.svg_element.id == "child" && actual.b > 0.95f && actual.r < 0.05f &&
			            actual.g < 0.05f,
			        "Inherited currentColor must resolve against the child element's own color");
		}
	}

	void css_pseudo_classes()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        svg:root #rooted { fill: #010101; }
		        #positions rect:nth-child(2n+1) { fill: #112233; }
		        #positions rect:first-child { fill: #ff0000; }
		        #positions rect:last-child { fill: #00ff00; }
		        #positions rect:nth-of-type(2) { fill: #0000ff; }
		        #positions rect:nth-last-child(3) { fill: #aabbcc; }
		        #filter rect:not(.off, [data-disabled]) { fill: #123456; }
		        #filter rect:is(.special, [data-role="accent"]) { fill: #00ffff; }
		        #filter rect.off { fill: #ff0000; }
		        #filter rect:where(.off) { fill: #0000ff; }
		        #only rect:only-child { fill: #abcdef; }
		        #typed rect:first-of-type { fill: #111111; }
		        #typed rect:last-of-type { fill: #222222; }
		        #one-type rect:only-of-type { fill: #333333; }
		        :lang(en) #language { fill: #654321; }
		        :lang(fr) #french { fill: #fedcba; }
		        #empty:empty { fill: #a1b2c3; }
		    </style>
		    <rect id="rooted" width="2" height="2"/>
		    <g id="positions">
		        <rect id="first" y="3" width="2" height="2"/>
		        <circle id="circle-one" cx="4" cy="4" r="1"/>
		        <rect id="second" x="3" y="3" width="2" height="2"/>
		        <rect id="third" x="6" y="3" width="2" height="2"/>
		        <circle id="circle-two" cx="10" cy="4" r="1"/>
		        <rect id="last" x="9" y="3" width="2" height="2"/>
		    </g>
		    <g id="filter">
		        <rect id="excluded" class="off" x="12" width="2" height="2"/>
		        <rect id="not-match" class="visible" x="15" width="2" height="2"/>
		        <rect id="is-class" class="special" x="18" width="2" height="2"/>
		        <rect id="is-attribute" data-role="accent" x="21" width="2" height="2"/>
		    </g>
		    <g id="only"><rect id="single" x="24" width="2" height="2"/></g>
		    <g id="typed">
		        <circle cx="28" cy="1" r="1"/>
		        <rect id="first-type" x="27" width="2" height="2"/>
		        <rect id="last-type" x="30" width="2" height="2"/>
		    </g>
		    <g id="one-type"><circle cx="34" cy="1" r="1"/><rect id="only-type" x="33" width="2" height="2"/></g>
		    <g lang="en-US"><rect id="language" x="36" width="2" height="2"/></g>
		    <g lang="fr-CA"><rect id="french" x="39" width="2" height="2"/></g>
		    <rect id="empty" x="42" width="2" height="2"/>
		    </svg>)svg");
		require(document.geometry.size() == 20, "CSS pseudo-classes must preserve all SVG shapes");

		const auto tint_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part.tint.rgba_glsl();
			}
			throw std::runtime_error("Expected SVG shape was not found");
		};
		const auto expect_color = [&](std::string_view id, glm::vec3 expected)
		{
			const auto actual = tint_for(id);
			require(std::abs(actual.r - expected.r) < 0.01f && std::abs(actual.g - expected.g) < 0.01f &&
			            std::abs(actual.b - expected.b) < 0.01f,
			        "CSS pseudo-class produced the wrong fill");
		};
		expect_color("rooted", {1 / 255.0f, 1 / 255.0f, 1 / 255.0f});
		expect_color("first", {1, 0, 0});
		expect_color("second", {0, 0, 1});
		expect_color("third", {0xaa / 255.0f, 0xbb / 255.0f, 0xcc / 255.0f});
		expect_color("last", {0, 1, 0});
		expect_color("excluded", {1, 0, 0});
		expect_color("not-match", {0x12 / 255.0f, 0x34 / 255.0f, 0x56 / 255.0f});
		expect_color("is-class", {0, 1, 1});
		expect_color("is-attribute", {0, 1, 1});
		expect_color("single", {0xab / 255.0f, 0xcd / 255.0f, 0xef / 255.0f});
		expect_color("first-type", {0x11 / 255.0f, 0x11 / 255.0f, 0x11 / 255.0f});
		expect_color("last-type", {0x22 / 255.0f, 0x22 / 255.0f, 0x22 / 255.0f});
		expect_color("only-type", {0x33 / 255.0f, 0x33 / 255.0f, 0x33 / 255.0f});
		expect_color("language", {0x65 / 255.0f, 0x43 / 255.0f, 0x21 / 255.0f});
		expect_color("french", {0xfe / 255.0f, 0xdc / 255.0f, 0xba / 255.0f});
		expect_color("empty", {0xa1 / 255.0f, 0xb2 / 255.0f, 0xc3 / 255.0f});
	}

	void svg_issue_128_css_explicit_inherit()
	{
		const auto document = svg::parse(R"svg(<svg><g fill="blue" color="blue" stroke="lime" stroke-width="2">
		    <rect id="fill-inherit" width="6" height="6" fill="red" style="fill: inherit"/>
		    <rect id="color-inherit" x="8" width="6" height="6" color="red"
		          style="color: inherit; fill: currentColor"/>
		    <path id="stroke-inherit" d="M16 0H22" fill="none" stroke="red" style="stroke: inherit"/>
		    </g></svg>)svg");
		const auto color_for = [&](std::string_view id)
		{
			const auto found = std::ranges::find_if(document.geometry,
			                                       [&](const auto& part) { return part.svg_element.id == id; });
			require(found != document.geometry.end(), "Explicit inherit must preserve painted SVG geometry");
			return found->tint.rgba_glsl();
		};
		const auto inherited_fill = color_for("fill-inherit");
		const auto inherited_color = color_for("color-inherit");
		const auto inherited_stroke = color_for("stroke-inherit");
		require(inherited_fill.b > 0.99f && inherited_fill.r < 0.01f,
		        "Explicit fill: inherit must bypass a lower-priority presentation attribute");
		require(inherited_color.b > 0.99f && inherited_color.r < 0.01f,
		        "Explicit color: inherit must resolve through the parent before currentColor is used");
		require(inherited_stroke.g > 0.99f && inherited_stroke.r < 0.01f,
		        "Explicit stroke: inherit must preserve the parent paint");
	}

	void svg_issues_131_132_133_134_135_136_139_140()
	{
		const auto coarse_ellipses = svg::parse(R"svg(<svg><circle id="small" cx="2" cy="2" r=".1"/>
		    <ellipse id="rx-only" cx="10" cy="2" rx="4"/>
		    <ellipse id="ry-auto" cx="20" cy="2" rx="4" ry="auto"/>
		    <ellipse id="control" cx="30" cy="2" rx="4" ry="4"/></svg>)svg");
		require(coarse_ellipses.geometry.size() == 4 &&
		            std::ranges::all_of(coarse_ellipses.geometry,
		                               [](const auto& part) { return part.triangle_view().size() >= 3; }),
		        "Small ellipses must remain polygonal at coarse tolerances and radius auto must resolve");
		require(std::abs(area(coarse_ellipses.geometry[1]) - area(coarse_ellipses.geometry[2])) < 0.001f &&
		            std::abs(area(coarse_ellipses.geometry[1]) - area(coarse_ellipses.geometry[3])) < 0.001f,
		        "An omitted or auto ellipse radius must copy the explicitly specified other radius");
		bool ellipse_limit_rejected = false;
		try
		{
			(void)svg::parse(R"svg(<svg><circle r=".1"/></svg>)svg", 0.25f, 2);
		}
		catch (const std::length_error&)
		{
			ellipse_limit_rejected = true;
		}
		require(ellipse_limit_rejected, "An ellipse must reject a segment limit too small for a closed polygon");

		const auto empty_root = svg::parse(R"svg(<svg viewBox="0 0 0 12"><rect width="8" height="8"/></svg>)svg");
		require(empty_root.geometry.empty(), "A zero-sized root viewBox must disable rendering without division by zero");
		const auto zero_symbol = svg::parse(R"svg(<svg viewBox="0 0 20 20"><defs>
		    <symbol id="zero" viewBox="0 0 0 10"><rect width="10" height="10"/></symbol>
		    </defs><use href="#zero" x="2" y="2" width="8" height="8"/>
		    <rect id="visible" x="12" width="4" height="4"/></svg>)svg");
		require(zero_symbol.geometry.size() == 1 && zero_symbol.geometry.front().svg_element.id == "visible",
		        "A zero-sized symbol viewBox must suppress only that symbol instance");

		const auto forgiving_css = svg::parse(R"svg(<svg>
		    <style>.bad { broken; fill: red; stroke:; } .good { fill: blue; }
		           rect/**/.adjacent { fill: green; }
		           #separate/**/ .descendant { stroke: blue; }</style>
		    <rect class="bad adjacent" width="4" height="4" style="broken; fill: lime"/>
		    <rect class="good" x="5" width="4" height="4"/>
		    <g id="separate"><rect class="descendant" x="10" width="4" height="4" fill="none"/></g>
		    </svg>)svg");
		require(forgiving_css.geometry.size() == 3, "Malformed declarations must not abort the SVG document");
		require(forgiving_css.geometry[0].tint.g > 0.99f && forgiving_css.geometry[0].tint.r < 0.01f &&
		            forgiving_css.geometry[1].tint.b > 0.99f && forgiving_css.geometry[1].tint.r < 0.01f,
		        "Valid declarations after malformed ones and comments adjacent to selector tokens must apply");

		const auto ns = svg::parse(R"svg(<svg xmlns="http://www.w3.org/2000/svg" xmlns:ext="urn:extension"
		    xmlns:s="http://www.w3.org/2000/svg"><style>#typed rect:first-of-type,
		    #typed rect:only-of-type,#typed rect:nth-of-type(1) { fill: red; }</style>
		    <g id="typed"><ext:rect/><s:rect id="typed-svg" width="4" height="4"/></g></svg>)svg");
		require(ns.geometry.size() == 1 && ns.geometry.front().svg_element.id == "typed-svg" &&
		            ns.geometry.front().tint.r > 0.99f,
		        "SVG structural type selectors must ignore foreign elements with the same local name");

		const auto important = svg::parse(R"svg(<svg><style>.icon { fill: blue ! important; }</style>
		    <rect class="icon" width="4" height="4" style="fill:red"/></svg>)svg");
		require(important.geometry.size() == 1 && important.geometry.front().tint.b > 0.99f,
		        "CSS ! important must retain its cascade priority with whitespace between tokens");
		const auto invalid_geometry = svg::parse(R"svg(<svg><rect width="20" height="8" style="width:bogus"/>
		    <rect x="24" width="8" height="8"/></svg>)svg");
		require(invalid_geometry.geometry.size() == 2 && area(invalid_geometry.geometry.front()) == 160.0f,
		        "Invalid CSS geometry must be ignored without replacing a valid presentation attribute");
	}

	constexpr std::string_view image_uri =
	    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAADklEQVR4n"
	    "GP4z8AAQg0AD3oDfnfpf5cAAAAASUVORK5CYII=";

	void image_layout()
	{
		const auto element = std::string("<image x=\"2\" y=\"4\" width=\"8\" height=\"8\" href=\"") +
		                     std::string(image_uri) + "\"";
		const auto document = svg::parse("<svg>" + element + "/>" + element +
		                                 " preserveAspectRatio=\"xMidYMid slice\"/></svg>");
		require(document.geometry.size() == 2, "Both images must be present");
		const auto& fitted = *document.geometry[0].image;
		const auto& cropped = *document.geometry[1].image;
		require(fitted.pixels->size == glm::uvec2(2, 1) && fitted.pixels->rgba[7] == 128,
		        "Image decoder must retain RGBA bytes");
		require(fitted.bounds.pos == glm::vec2(2, 6) && fitted.bounds.size == glm::vec2(8, 4),
		        "Meet must center the image inside its viewport");
		require(cropped.bounds.size == glm::vec2(8, 8) && cropped.uv == glm::vec4(0.25f, 1, 0.75f, 0),
		        "Slice must crop source coordinates and keep the viewport");

		const auto preferred_image = svg::parse(
		    "<svg xmlns:xlink=\"http://www.w3.org/1999/xlink\"><image width=\"2\" height=\"1\" "
		    "xlink:href=\"unsupported://fallback\" href=\"" + std::string(image_uri) + "\"/>"
		    "<image width=\"2\" height=\"1\" href=\"" + std::string(image_uri) +
		    "\" xlink:href=\"unsupported://fallback\"/></svg>");
		require(preferred_image.geometry.size() == 2 && preferred_image.geometry[0].image &&
		            preferred_image.geometry[1].image,
		        "Plain image href must take precedence over XLink in either attribute order");

		const auto css_image = svg::parse(
		    "<svg viewBox=\"0 0 64 64\"><style>.thumbnail { x:12px; width:50%; }</style>"
		    "<image class=\"thumbnail\" width=\"8\" height=\"8\" href=\"" + std::string(image_uri) + "\"/>"
		    "<image x=\"12\" width=\"32\" height=\"8\" href=\"" + std::string(image_uri) + "\"/>"
		    "</svg>");
		require(css_image.geometry.size() == 2 && css_image.geometry[0].object_bounds &&
		            css_image.geometry[1].object_bounds &&
		            css_image.geometry[0].object_bounds->pos == css_image.geometry[1].object_bounds->pos &&
		            css_image.geometry[0].object_bounds->size == css_image.geometry[1].object_bounds->size,
		        "SVG image x and width CSS geometry must match equivalent presentation attributes");
	}

	void sync_view_reuses_decoded_external_images()
	{
		const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto directory = std::filesystem::temp_directory_path() /
		                       ("mngl-svg-image-cache-" + std::to_string(suffix));
		std::filesystem::create_directories(directory);
		try
		{
			const auto image_path = directory / "pixel.png";
			const auto svg_path = directory / "document.svg";
			const auto image_data = svg::decode_image_base64(image_uri.substr(image_uri.find(',') + 1));
			{
				std::ofstream image_file(image_path, std::ios::binary);
				image_file.write(reinterpret_cast<const char*>(image_data.data()),
				                 static_cast<std::streamsize>(image_data.size()));
			}
			{
				std::ofstream svg_file(svg_path, std::ios::binary);
				svg_file << R"svg(<svg width="8" height="4" viewBox="0 0 8 4">
				    <image width="4" height="4" href="pixel.png"/>
				    <image x="4" width="4" height="4" href="pixel.png"/>
				    </svg>)svg";
			}
			mngl::two_d::svg_load_options options;
			options.allow_external_images = true;
			auto object = mngl::two_d::svg_object::from_file(svg_path, options);
			require(object.geometry_count() == 2, "Repeated local SVG image references must produce geometry");
			std::filesystem::remove(image_path);
			object.sync_view(mngl::view{mngl::rectangle{0, 0, 8, 4}}, {64, 32});
			require(object.geometry_count() == 2,
			        "View synchronization must reuse decoded pixels for repeated references after file removal");
		}
		catch (...)
		{
			std::filesystem::remove_all(directory);
			throw;
		}
		std::filesystem::remove_all(directory);
	}

	void svg_object_sync_view_preserves_layout_viewport()
	{
		constexpr std::string_view source = R"svg(<svg width="100%" height="100%" viewBox="0 0 100 100">
		    <style>
		        @media (min-width: 500px) { #shape { fill: #ff0000; } }
		        @media (max-width: 499px) { #shape { fill: #0000ff; } }
		    </style>
		    <path id="shape" d="M5 50 C5 5 95 5 95 50 C95 95 5 95 5 50 Z"/>
		    </svg>)svg";
		mngl::two_d::svg_object object;
		object.load(source);
		const auto loaded_size = object.size();
		require(loaded_size.x > 0 && loaded_size.y > 0 && object.geometry_count() == 1,
		        "The SVG must load with a resolved percentage viewport and curve geometry");

		object.sync_view(mngl::view{mngl::rectangle{0, 0, 100, 100}}, {800, 600}, 0.25f);
		require(object.size() == loaded_size && object.geometry_count() == 1,
		        "sync_view must update curve tessellation without changing the loaded percentage layout viewport");

		svg::view_tessellation_context initial_context{};
		initial_context.viewport_size = {300, 150};
		initial_context.pixels_per_world_unit = 1.0;
		auto synchronized_context = initial_context;
		synchronized_context.pixels_per_world_unit = 8.0;
		const auto initial_document = svg::parse(source, 0.25f, 1'000'000, &initial_context);
		const auto synchronized_document = svg::parse(source, 0.25f, 1'000'000, &synchronized_context);
		require(initial_document.viewport_size == synchronized_document.viewport_size &&
		            initial_document.geometry.front().tint == synchronized_document.geometry.front().tint &&
		            initial_document.geometry.front().tint.b > initial_document.geometry.front().tint.r,
		        "A larger tessellation target must preserve loaded bounds and the original media-query style");
		require(synchronized_document.geometry.front().triangle_view().size() >
		            initial_document.geometry.front().triangle_view().size(),
		        "A larger screen-space target must add curve detail while retaining the loaded layout");
	}

	void effect_graph()
	{
		const auto document = svg::parse(R"svg(<svg viewBox="0 0 64 64">
		    <defs>
		        <mask id="fade" mask-type="alpha" maskContentUnits="objectBoundingBox">
		            <rect width="50%" height="100%" fill="white" fill-opacity=".5"/>
		        </mask>
		        <filter id="glow" x="0" y="0" width="1" height="1">
		            <feGaussianBlur in="SourceAlpha" stdDeviation="2" result="blur"/>
		            <feOffset in="blur" dx="2" result="shift"/>
		            <feMerge><feMergeNode in="shift"/><feMergeNode in="SourceGraphic"/></feMerge>
		        </filter>
		    </defs>
		    <g transform="translate(2, 3)" filter="url(#glow)" mask="url(#fade)" opacity=".6">
		        <rect id="body" x="10" y="15" width="20" height="12" fill="red"/>
		    </g>
		    </svg>)svg");
		require(document.geometry.size() == 1 && document.geometry[0].effect, "Effect must cover the group");
		const auto& part = document.geometry.front();
		const auto& layer = *part.effect;
		require(part.tint.a == .6f && layer.source.front().tint.a == 1,
		        "Group opacity must apply after effects");
		require(layer.region.pos == glm::vec2(10, 15) && layer.region.size == glm::vec2(20, 12),
		        "Filter region must be expressed in group coordinates");
		require(layer.mask && layer.mask->channel == svg::mask_channel::alpha &&
		            layer.mask_source.size() == 1,
		        "Alpha mask must retain its own drawing");
		require(area(layer.mask_source[0]) == .5f,
		        "Percentages in a normalized mask use its content viewport");
		const auto mask_transform = layer.mask_source[0].transform;
		require(svg::apply_transform(mask_transform, {.5f, 1.f}) == glm::vec2(20, 27),
		        "Mask content coordinates must scale to the object bounds");
		const auto& steps = layer.filter->steps;
		require(steps.size() == 4 && steps[0].input == 1 && steps[1].input == 2 && steps[2].input2 == 3,
		        "Filter graph must preserve named and built-in inputs");
		const auto primitive_regions = svg::parse(R"svg(<svg viewBox="0 0 64 64">
		    <defs><filter id="regions" x="0" y="0" width="64" height="64"
		        filterUnits="userSpaceOnUse" primitiveUnits="userSpaceOnUse">
		        <feFlood x="8" y="8" width="12" height="12" result="small"/>
		        <feColorMatrix in="small" result="same"/>
		        <feOffset in="same" x="4"/>
		        <feComposite in="small" in2="SourceGraphic" x="2"/>
		        <feFlood width="0" height="0" result="empty"/>
		        <feColorMatrix in="empty"/>
		    </filter></defs>
		    <rect width="64" height="64" filter="url(#regions)"/>
		    </svg>)svg");
		const auto& regions = primitive_regions.geometry.front().effect->filter->steps;
		require(regions.size() == 6 && regions[0].region == mngl::rectangle{8, 8, 12, 12} &&
		            regions[1].region == mngl::rectangle{8, 8, 12, 12} &&
		            regions[2].region == mngl::rectangle{4, 8, 12, 12},
		        "Filter primitive defaults must follow the actual bounds of named inputs");
		require(regions[3].region == mngl::rectangle{2, 0, 64, 64},
		        "Explicit primitive coordinates must override inherited input bounds");
		require(regions[5].region == mngl::rectangle{0, 0, 0, 0},
		        "An empty input region must remain empty through later primitives");
		const auto interpolation = svg::parse(R"svg(<svg viewBox="0 0 8 8">
		    <defs><filter id="spaces" color-interpolation-filters="sRGB">
		        <feFlood flood-color="#808080" result="srgb"/>
		        <feColorMatrix in="srgb" color-interpolation-filters="linearRGB" result="linear"/>
		        <feFlood flood-color="#808080" style="color-interpolation-filters:linearRGB" result="style"/>
		        <feColorMatrix in="style" result="inherited"/>
		    </filter></defs>
		    <rect width="8" height="8" filter="url(#spaces)"/>
		    </svg>)svg");
		const auto& color_steps = interpolation.geometry.front().effect->filter->steps;
		require(color_steps.size() == 4 && !color_steps[0].linear_rgb && color_steps[1].linear_rgb &&
		            color_steps[2].linear_rgb && !color_steps[3].linear_rgb,
		        "Primitive color space must override or inherit the filter-level value");
		require(std::abs(color_steps[0].parameters.r - 128 / 255.0f) < .001f &&
		            std::abs(color_steps[2].parameters.r - .21586f) < .002f,
		        "feFlood colors must be decoded in each primitive's selected color space");
		const auto lowercase_srgb = svg::parse(R"svg(<svg>
		    <defs><filter id="lowercase" color-interpolation-filters="srgb">
		        <feFlood flood-color="#808080"/>
		    </filter></defs>
		    <rect width="8" height="8" filter="url(#lowercase)"/>
		    </svg>)svg");
		const auto& lowercase_filter = lowercase_srgb.geometry.front().effect->filter;
		require(!lowercase_filter->linear_rgb && !lowercase_filter->steps.front().linear_rgb,
		        "Filter-level sRGB must be matched case-insensitively and inherited by its primitives");
		const auto definition_color_space = svg::parse(R"svg(<svg><defs><g color-interpolation-filters="sRGB">
		    <filter id="definition-space"><feFlood result="source"/>
		      <feFlood style="color-interpolation-filters:inherit" flood-color="#808080" result="inherited"/>
		      <feColorMatrix in="inherited"/></filter></g></defs>
		    <rect width="8" height="8" filter="url(#definition-space)"/></svg>)svg");
		const auto& definition_steps = definition_color_space.geometry.front().effect->filter->steps;
		require(definition_steps.size() == 3 && !definition_steps[0].linear_rgb &&
		            !definition_steps[1].linear_rgb && !definition_steps[2].linear_rgb &&
		            std::abs(definition_steps[1].parameters.r - 128 / 255.0f) < .001f,
		        "Filter interpolation must inherit through definition ancestors and explicit inherit");
		const auto inherited_flood_color = svg::parse(R"svg(<svg><defs color="red">
		    <filter id="current"><feFlood flood-color="currentColor"/></filter></defs>
		    <rect width="8" height="8" fill="blue" filter="url(#current)"/></svg>)svg");
		const auto flood = inherited_flood_color.geometry.front().effect->filter->steps.front().parameters;
		require(flood.r > .99f && flood.g < .01f && flood.b < .01f,
		        "Filter flood currentColor must resolve from its definition-site ancestors");
		const auto foreign_children = svg::parse(R"svg(<svg xmlns:ext="urn:extension"><defs>
		    <filter id="foreign"><ext:feFlood result="ignored"/><feFlood flood-color="red" result="painted"/>
		      <ext:metadata/><feMerge><ext:feMergeNode in="SourceGraphic"/>
		        <feMergeNode in="painted"/></feMerge></filter></defs>
		    <rect width="8" height="8" filter="url(#foreign)"/></svg>)svg");
		require(foreign_children.geometry.size() == 1 &&
		            foreign_children.geometry.front().effect->filter->steps.size() == 2,
		        "Foreign namespace children must not enter or invalidate an SVG filter graph");
		const auto merge_interpolation = svg::parse(R"svg(<svg>
		    <defs><filter id="merge" color-interpolation-filters="sRGB">
		        <feFlood result="first"/><feFlood result="second"/>
		        <feMerge><feMergeNode in="first"/><feMergeNode in="second"/></feMerge>
		    </filter></defs>
		    <rect width="8" height="8" filter="url(#merge)"/>
		    </svg>)svg");
		const auto& merge_filter = *merge_interpolation.geometry.front().effect->filter;
		require(!merge_filter.linear_rgb &&
		            std::ranges::all_of(merge_filter.steps, [](const auto& step) { return !step.linear_rgb; }),
		        "All generated steps for feMerge must retain the primitive's selected color space");
		const auto line = svg::parse(R"svg(<svg>
		    <defs><filter id="blur"><feGaussianBlur stdDeviation="2"/></filter></defs>
		    <line x2="5" stroke="red" opacity=".5" filter="url(#blur)"/>
		    </svg>)svg");
		require(line.geometry.size() == 1 && line.geometry.front().effect &&
		            line.geometry.front().tint.a == .5f && !line.geometry.front().effect->filter &&
		            line.geometry.front().effect->source.size() == 1 &&
		            line.geometry.front().effect->source.front().tint.a == 1,
		        "A degenerate filter must be ignored while line opacity is composited once");
	}

	void opacity_compositing()
	{
		const auto overlapping = svg::parse(R"svg(<svg>
		    <g opacity=".5"><rect id="red" width="4" height="4" fill="red"/>
		                     <rect id="blue" x="2" width="4" height="4" fill="blue"/></g>
		    </svg>)svg");
		require(overlapping.geometry.size() == 1 && overlapping.geometry.front().effect,
		        "An opacity group must produce one isolated composite");
		const auto& group = *overlapping.geometry.front().effect;
		require(overlapping.geometry.front().tint.a == .5f && group.source.size() == 2,
		        "Group opacity must be applied once after both children are painted");
		require(group.source[0].tint.a == 1 && group.source[1].tint.a == 1 &&
		            group.source[0].tint.r == 1 && group.source[0].tint.b == 0 &&
		            group.source[1].tint.r == 0 && group.source[1].tint.b == 1,
		        "Opaque child colors and alpha must survive inside the group composite");

		const auto nested = svg::parse(R"svg(<svg>
		    <g opacity=".5"><g opacity=".25"><rect width="4" height="4" fill="red"/></g></g>
		    </svg>)svg");
		require(nested.geometry.size() == 1 && nested.geometry.front().effect &&
		            nested.geometry.front().tint.a == .5f &&
		            nested.geometry.front().effect->source.size() == 1 &&
		            nested.geometry.front().effect->source.front().effect &&
		            nested.geometry.front().effect->source.front().tint.a == .25f &&
		            nested.geometry.front().effect->source.front().effect->source.front().tint.a == 1,
		        "Nested group opacities must compose at their separate boundaries");

		const auto inherited = svg::parse(R"svg(<svg>
		    <g opacity=".5"><g opacity="inherit"><rect width="4" height="4" fill="red"/></g></g>
		    </svg>)svg");
		require(inherited.geometry.size() == 1 && inherited.geometry.front().effect &&
		            inherited.geometry.front().tint.a == .5f &&
		            inherited.geometry.front().effect->source.size() == 1 &&
		            inherited.geometry.front().effect->source.front().effect &&
		            inherited.geometry.front().effect->source.front().tint.a == .5f,
		        "Explicit opacity: inherit must inherit the parent's opacity value");

		const auto fill_stroke = svg::parse(R"svg(<svg>
		    <path opacity=".5" d="M0 0 H8 V8 H0 Z" fill="red" stroke="blue" stroke-width="4"/>
		    </svg>)svg");
		require(fill_stroke.geometry.size() == 1 && fill_stroke.geometry.front().effect,
		        "Shape opacity must isolate its fill and stroke");
		const auto& shape = *fill_stroke.geometry.front().effect;
		require(fill_stroke.geometry.front().tint.a == .5f && shape.source.size() == 2 &&
		            shape.source[0].tint.a == 1 && shape.source[1].tint.a == 1 &&
		            shape.source[0].tint.r == 1 && shape.source[1].tint.b == 1,
		        "Fill and stroke must retain their colors and composite under shape opacity once");

		const auto masked = svg::parse(R"svg(<svg>
		    <defs><mask id="half" mask-type="alpha"><rect width="50%" height="100%" fill="white"/></mask></defs>
		    <g opacity=".5" mask="url(#half)">
		        <rect width="8" height="8" fill="red"/>
		        <rect x="2" width="8" height="8" fill="blue"/>
		    </g>
		    </svg>)svg");
		require(masked.geometry.size() == 1 && masked.geometry.front().effect &&
		            masked.geometry.front().effect->mask && masked.geometry.front().tint.a == .5f &&
		            masked.geometry.front().effect->source.size() == 2 &&
		            masked.geometry.front().effect->source[0].tint.a == 1 &&
		            masked.geometry.front().effect->source[1].tint.a == 1,
		        "Group opacity must follow painting and masking of overlapping children");
	}

	void svg_shader_format_selection()
	{
		const auto preferred = static_cast<mngl::gpu::shader_format>(mngl::gpu::default_shader_format);
		const auto supported = static_cast<SDL_GPUShaderFormat>(preferred) | mngl::gpu::shader_formats::spirv;
		require(svg::select_svg_shader_format(supported) == preferred,
		        "SVG shaders must prefer the platform format when the active device supports it");
		require(svg::select_svg_shader_format(mngl::gpu::shader_formats::dxil) ==
		            mngl::gpu::shader_format::dxil,
		        "SVG shaders must select DXIL when it is the active device's only supported format");
		require(svg::select_svg_shader_format(mngl::gpu::shader_formats::spirv) ==
		            mngl::gpu::shader_format::spirv,
		        "SVG shaders must select SPIR-V when it is the active device's only supported format");
		require(svg::select_svg_shader_format(mngl::gpu::shader_formats::dxbc) ==
		            mngl::gpu::shader_format::dxbc,
		        "SVG shaders must fall back to a supported non-default format");
		require(svg::select_svg_shader_format(SDL_GPU_SHADERFORMAT_INVALID) ==
		            mngl::gpu::shader_format::invalid,
		        "SVG shader selection must reject an empty supported-format set");

		const auto shader = std::filesystem::path("svg_filter.vert.hlsl");
		const auto check_binary_path = [&](mngl::gpu::shader_format format, std::string_view expected)
		{
			const auto path = svg::svg_shader_binary_path(shader, format);
			require(path && path->filename() == expected,
			        "SVG shader loader must resolve a supported precompiled extension");
		};
		check_binary_path(mngl::gpu::shader_format::dxil, "svg_filter.vert.dxil");
		check_binary_path(mngl::gpu::shader_format::spirv, "svg_filter.vert.spv");
		check_binary_path(mngl::gpu::shader_format::msl, "svg_filter.vert.msl");
		check_binary_path(mngl::gpu::shader_format::dxbc, "svg_filter.vert.dxbc");
		check_binary_path(mngl::gpu::shader_format::metallib, "svg_filter.vert.metallib");

		const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto directory = std::filesystem::temp_directory_path() /
		                       ("mngl-svg-shader-format-" + std::to_string(suffix));
		std::filesystem::create_directories(directory);
		try
		{
			for (const auto format : {mngl::gpu::shader_format::dxil, mngl::gpu::shader_format::spirv})
			{
				const auto source = directory / "svg_filter.frag.hlsl";
				const auto binary = svg::svg_shader_binary_path(source, format);
				require(binary.has_value(), "DXIL and SPIR-V need precompiled SVG shader paths");
				{
					std::ofstream output(*binary, std::ios::binary);
					output << "compiled shader fixture";
				}

				mngl::gpu::shader_compile_options options{};
				options.stage = mngl::gpu::graphics_shader_stage::fragment;
				options.format = format;
				const auto asset = svg::load_svg_shader(source, options);
				require(asset.valid() && asset.code.format == format,
				        "Precompiled SVG shader loading must keep the selected DXIL/SPIR-V format");
			}
		}
		catch (...)
		{
			std::filesystem::remove_all(directory);
			throw;
		}
		std::filesystem::remove_all(directory);
	}

	void namespace_aware_svg_dispatch()
	{
		const auto foreign_metadata = svg::parse(R"svg(<svg xmlns="http://www.w3.org/2000/svg"
		    xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
		    xmlns:ext="urn:example:extension">
		    <sodipodi:namedview id="namedview"><ext:rect width="40" height="40"/></sodipodi:namedview>
		    <rect id="actual" width="16" height="16"><ext:notes/></rect>
		    <ext:rect id="foreign-rect" x="20" width="16" height="16"/>
		    </svg>)svg");
		require(foreign_metadata.geometry.size() == 1 &&
		            foreign_metadata.geometry.front().svg_element.id == "actual",
		        "Foreign editor metadata and extension shapes must be ignored");

		const auto prefixed_svg = svg::parse(R"svg(<svg:svg xmlns:svg="http://www.w3.org/2000/svg">
		    <svg:rect id="prefixed" width="16" height="16"/>
		    </svg:svg>)svg");
		require(prefixed_svg.geometry.size() == 1 &&
		            prefixed_svg.geometry.front().svg_element.id == "prefixed",
		        "SVG elements using a declared SVG namespace prefix must still render");
	}

	void css_lang_identifier_escapes()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		      :lang(en\2d US) #hex-escaped { fill: red; }
		      :lang("en\-GB") #simple-escaped { fill: green; }
		      :lang(en-CA) #plain { fill: blue; }
		    </style>
		    <g lang="en-US"><rect id="hex-escaped" width="2" height="2"/></g>
		    <g lang="en-GB"><rect id="simple-escaped" x="3" width="2" height="2"/></g>
		    <g lang="en-CA"><rect id="plain" x="6" width="2" height="2"/></g>
		    </svg>)svg");
		require(document.geometry.size() == 3, "Escaped :lang() selectors must retain all SVG shapes");
		const auto tint_for = [&](std::string_view id)
		{
			const auto found = std::ranges::find_if(document.geometry,
			                                        [&](const auto& part) { return part.svg_element.id == id; });
			require(found != document.geometry.end(), "Expected :lang() test shape was not parsed");
			return found->tint.rgba_glsl();
		};
		const auto hex_escaped = tint_for("hex-escaped");
		const auto simple_escaped = tint_for("simple-escaped");
		const auto plain = tint_for("plain");
		require(hex_escaped.r > .95f && hex_escaped.g < .05f &&
		            simple_escaped.r < .05f && simple_escaped.g > .45f && simple_escaped.b < .05f &&
		            plain.r < .05f && plain.g < .05f && plain.b > .95f,
		        "Hexadecimal and simple CSS escapes must decode in quoted and unquoted :lang() arguments");
	}

	void foreign_namespace_attributes_are_not_svg_properties()
	{
		const auto document = svg::parse(R"svg(<svg xmlns="http://www.w3.org/2000/svg"
		    xmlns:ext="urn:example:extension">
		    <style>
		      #foreign-id { fill: red; }
		      .foreign-class { fill: green; }
		      rect[data-kind] { fill: blue; }
		      #xml-language:lang(en) { fill: yellow; }
		      #xml-identified { fill: magenta; }
		    </style>
		    <rect ext:id="foreign-id" width="4" height="4"/>
		    <rect ext:class="foreign-class" x="5" width="4" height="4"/>
		    <rect ext:data-kind="extension" x="10" width="4" height="4"/>
		    <rect ext:fill="red" x="15" width="4" height="4"/>
		    <g xml:lang="en-US"><rect id="xml-language" x="20" width="4" height="4"/></g>
		    <rect xml:id="xml-identified" x="25" width="4" height="4"/>
		    </svg>)svg");
		require(document.geometry.size() == 6, "Foreign attributes must not suppress SVG geometry");
		for (std::size_t index = 0; index < 4; ++index)
		{
			const auto& part = document.geometry[index];
			const auto color = part.tint.rgba_glsl();
			require(part.svg_element.id.empty(), "A namespaced id attribute must not become the SVG element id");
			require(color.r < 0.01f && color.g < 0.01f && color.b < 0.01f,
			        "Namespaced attributes must not match unqualified CSS selectors or presentation properties");
		}
		const auto& language_shape = document.geometry[4];
		const auto language_color = language_shape.tint.rgba_glsl();
		require(language_shape.svg_element.id == "xml-language" && language_color.r > 0.95f &&
		            language_color.g > 0.95f && language_color.b < 0.05f,
		        "The XML namespace form of lang must still drive :lang() matching");
		const auto& xml_id_shape = document.geometry.back();
		const auto xml_id_color = xml_id_shape.tint.rgba_glsl();
		require(xml_id_shape.svg_element.id == "xml-identified" && xml_id_color.r > 0.95f &&
		            xml_id_color.b > 0.95f,
		        "The standardized XML id attribute must remain available as the SVG element id");
	}

	void css_escaped_identifiers_and_attribute_values()
	{
		svg::xml_tag attribute_target;
		attribute_target.name = "rect";
		attribute_target.attributes.push_back({"data-flag", "comma, value", {}});
		const auto attribute_selector = svg::parse_css_selector(R"(rect[data-\66 lag="comma\2c \20 value"])");
		require(svg::matches_css_selector(attribute_selector, attribute_target),
		        "Hex CSS escapes must decode attribute names and quoted selector values");
		const auto escaped_type_selector = svg::parse_css_selector(R"(\72 ect[data-flag="comma, value"])");
		require(svg::matches_css_selector(escaped_type_selector, attribute_target),
		        "An escaped first character in a type selector must decode as a CSS identifier");
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		      #\31 23 { fill: red; }
		      rect[data-\66 lag="comma\2c \20 value"] { fill: green; }
		      rect[data-token=space\ value] { fill: blue; }
		    </style>
		    <rect id="123" width="2" height="2"/>
		    <rect id="escaped-attribute" data-flag="comma, value" x="3" width="2" height="2"/>
		    <rect id="escaped-space" data-token="space value" x="6" width="2" height="2"/>
		  </svg>)svg");
		require(document.geometry.size() == 3, "CSS escaped selector names must retain all matched elements");
		const auto expect_color = [&](std::string_view id, glm::vec3 expected)
		{
			const auto found = std::ranges::find_if(document.geometry,
			                                        [&](const auto& part) { return part.svg_element.id == id; });
			require(found != document.geometry.end(), "CSS escaped selector target was not parsed");
			const auto color = found->tint.rgba_glsl();
			if (!(std::abs(color.r - expected.r) < 0.01f && std::abs(color.g - expected.g) < 0.01f &&
			      std::abs(color.b - expected.b) < 0.01f))
				throw std::runtime_error("CSS escaped selector produced wrong color for " + std::string(id) +
				                         ": " + std::to_string(color.r) + "," + std::to_string(color.g) + "," +
				                         std::to_string(color.b));
		};
		expect_color("123", {1, 0, 0});
		expect_color("escaped-attribute", {0, 0.5f, 0});
		expect_color("escaped-space", {0, 0, 1});
	}

	void css_media_query_negation()
	{
		const auto document = svg::parse(R"svg(<svg width="100" height="100">
		    <style>
		        @media not print { #not-print { fill: #00ff00; } }
		        @media not (min-width: 900px) { #valid-inverse { fill: #0000ff; } }
		        @media not (unknown-feature: 1) { #unknown-feature { fill: #ff0000; } }
		        @media not (width: invalid) { #malformed-value { fill: #ff0000; } }
		        @media not print and (unknown-feature: 1) { #false-before-invalid { fill: #ff0000; } }
		        @media not print, (min-width: 900px) { #comma-valid { fill: #00ffff; } }
		        @media not (width: invalid), (min-width: 900px) { #comma-invalid { fill: #ff0000; } }
		        @media (min-width: 50px) {
		            @media not (unknown-feature: 1) { #nested-invalid { fill: #ff0000; } }
		            @media not print { #nested-valid { fill: #ff00ff; } }
		        }
		    </style>
		    <rect id="not-print" width="2" height="2"/>
		    <rect id="valid-inverse" x="3" width="2" height="2"/>
		    <rect id="unknown-feature" x="6" width="2" height="2"/>
		    <rect id="malformed-value" x="9" width="2" height="2"/>
		    <rect id="false-before-invalid" x="12" width="2" height="2"/>
		    <rect id="comma-valid" x="15" width="2" height="2"/>
		    <rect id="comma-invalid" x="18" width="2" height="2"/>
		    <rect id="nested-invalid" x="21" width="2" height="2"/>
		    <rect id="nested-valid" x="24" width="2" height="2"/>
		    </svg>)svg");
		require(document.geometry.size() == 9, "Invalid media conditions must not discard SVG elements");

		const auto tint_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part.tint.rgba_glsl();
			}
			throw std::runtime_error("Expected SVG shape was not found");
		};
		const auto expect_color = [&](std::string_view id, glm::vec3 expected)
		{
			const auto actual = tint_for(id);
			require(std::abs(actual.r - expected.r) < 0.01f && std::abs(actual.g - expected.g) < 0.01f &&
			            std::abs(actual.b - expected.b) < 0.01f,
			        "Media query matching or negation produced the wrong result");
		};
		expect_color("not-print", {0, 1, 0});
		expect_color("valid-inverse", {0, 0, 1});
		expect_color("unknown-feature", {0, 0, 0});
		expect_color("malformed-value", {0, 0, 0});
		expect_color("false-before-invalid", {0, 0, 0});
		expect_color("comma-valid", {0, 1, 1});
		expect_color("comma-invalid", {0, 0, 0});
		expect_color("nested-invalid", {0, 0, 0});
		expect_color("nested-valid", {1, 0, 1});
	}

	void svg_href_precedence()
	{
		const auto document = svg::parse(R"svg(<svg xmlns:xlink="http://www.w3.org/1999/xlink"
		    xmlns:other="http://www.w3.org/1999/xlink" viewBox="0 0 64 16">
		    <defs>
		        <rect id="a" width="8" height="8" fill="red"/>
		        <rect id="b" width="8" height="8" fill="blue"/>
		    </defs>
		    <use id="plain-wins-first" xlink:href="#a" href="#b" x="10"/>
		    <use id="plain-wins-last" href="#b" xlink:href="#a" x="20"/>
		    <use id="alias-wins" other:href="#a" href="#b" x="30"/>
		    <use id="fallback" xlink:href="#a" x="40"/>
		    </svg>)svg");
		require(document.geometry.size() == 4, "Each SVG use instance must render exactly once");
		for (std::size_t index = 0; index < 3; ++index)
		{
			const auto color = document.geometry[index].tint.rgba_glsl();
			require(color.b > 0.99f && color.r < 0.01f,
			        "Unprefixed href must win over any XLink-namespaced href regardless of XML order");
		}
		const auto fallback = document.geometry[3].tint.rgba_glsl();
		require(fallback.r > 0.99f && fallback.b < 0.01f,
		        "The XLink href must be used when an unprefixed href is absent");

		const auto rejects = [](std::string_view source)
		{
			try
			{
				(void)svg::parse(source);
				return false;
			}
			catch (const std::invalid_argument&)
			{
				return true;
			}
		};
		require(rejects(R"svg(<svg xmlns="http://www.w3.org/2000/svg" xmlns:foreign="urn:example:foreign">
		    <defs><rect id="target" width="2" height="2"/></defs>
		    <use foreign:href="#target"/>
		    </svg>)svg"),
		        "A foreign-namespace href must not serve as an SVG use reference");
		const auto foreign_image = std::string(R"svg(<svg xmlns="http://www.w3.org/2000/svg"
		    xmlns:foreign="urn:example:foreign"><image foreign:href=")svg") +
		                           std::string(image_uri) + R"svg(" width="2" height="1"/></svg>)svg";
		require(rejects(foreign_image), "A foreign-namespace href must not serve as an image source");
	}

	void css_color_and_percentage_opacity()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <defs><linearGradient id="stops"><stop stop-color="rgba(0,255,0,.5)" stop-opacity="50%"/>
		        <stop offset="1" stop-color="rgb(100%,0%,0%)" stop-opacity="25%"/></linearGradient></defs>
		    <rect id="rgba" width="4" height="4" fill="rgba(255,0,0,.5)"/>
		    <path id="rgb-percent" d="M0 6H4" fill="none" stroke="rgb(100%,0%,0%)"
		          stroke-width="2" stroke-opacity="50%"/>
		    <rect id="hex-rgba" x="5" width="4" height="4" fill="#f008"/>
		    <rect id="hex-rgb-hex-alpha" x="10" width="4" height="4" fill="#ff000080"/>
		    <rect id="fill-opacity" x="15" width="4" height="4" fill="red" fill-opacity="50%"/>
		    <rect id="shape-opacity" x="20" width="4" height="4" fill="red" opacity="50%"/>
		    <rect id="gradient" x="25" width="4" height="4" fill="url(#stops)"/>
		    </svg>)svg");
		const auto paint_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part;
			}
			throw std::runtime_error("Expected SVG color test geometry was not found");
		};
		const auto rgba = paint_for("rgba").tint.rgba_glsl();
		const auto rgb_percent = paint_for("rgb-percent").tint.rgba_glsl();
		const auto hex_rgba = paint_for("hex-rgba").tint.rgba_glsl();
		const auto hex_alpha = paint_for("hex-rgb-hex-alpha").tint.rgba_glsl();
		const auto fill_opacity = paint_for("fill-opacity").tint.rgba_glsl();
		const auto shape_opacity = std::ranges::find_if(document.geometry,
		                                              [](const auto& part) { return part.effect != nullptr; });
		require(rgba.r > 0.99f && rgba.g < 0.01f && std::abs(rgba.a - 0.5f) < 0.01f,
		        "Comma-separated rgba() must preserve its alpha component");
		require(rgb_percent.r > 0.99f && rgb_percent.g < 0.01f &&
		            std::abs(rgb_percent.a - 0.5f) < 0.01f,
		        "Percentage rgb() channels and stroke-opacity percentages must be accepted");
		require(hex_rgba.r > 0.99f && hex_rgba.g < 0.01f &&
		            std::abs(hex_rgba.a - 0x88 / 255.0f) < 0.01f,
		        "Four-digit hex colors must decode their alpha nibble");
		require(hex_alpha.r > 0.99f && hex_alpha.g < 0.01f &&
		            std::abs(hex_alpha.a - 128.0f / 255.0f) < 0.01f,
		        "Eight-digit hex colors must decode their alpha byte");
		require(std::abs(fill_opacity.a - 0.5f) < 0.01f,
		        "fill-opacity must accept a percentage value");
		require(shape_opacity != document.geometry.end() &&
		            std::abs(shape_opacity->tint.a - 0.5f) < 0.01f,
		        "Element opacity must accept a percentage value");
		const auto& stops = paint_for("gradient").gradient->definition->stops;
		require(stops.size() == 2 && std::abs(stops[0].value.rgba_glsl().a - 0.25f) < 0.01f &&
		            std::abs(stops[1].value.rgba_glsl().a - 0.25f) < 0.01f,
		        "Gradient stop colors and stop-opacity must both accept alpha and percentages");

		const auto fallback = svg::parse(R"svg(<svg>
		    <style>rect { fill: #00ff00; } #local-invalid { fill: not-a-color; }</style>
		    <rect id="local-invalid" width="4" height="4"/>
		    <rect id="bad-attribute" x="5" width="4" height="4" fill="not-a-color"/>
		    <rect id="good-neighbor" x="10" width="4" height="4"
		          style="fill: rgba(0,0,255,50%)"/>
		    </svg>)svg");
		for (const auto& part : fallback.geometry)
		{
			const auto color = part.tint.rgba_glsl();
			if (part.svg_element.id == "local-invalid" || part.svg_element.id == "bad-attribute")
			{
				require(color.g > 0.99f && color.r < 0.01f,
				        "An invalid color declaration must leave valid lower-priority paint available");
			}
			else if (part.svg_element.id == "good-neighbor")
			{
				require(color.b > 0.99f && std::abs(color.a - 0.5f) < 0.01f,
				        "An invalid color on another shape must not block valid alpha colors");
			}
		}
	}

	void css_named_srgb_colors()
	{
		require(std::ranges::count_if(svg::named_srgb_colors,
		                              [](const auto& entry) { return !entry.name.empty(); }) == 148,
		        "The complete 148 opaque CSS named sRGB colors must be present");
		for (std::size_t index = 0; index < svg::named_srgb_colors.size(); ++index)
		{
			const auto& entry = svg::named_srgb_colors[index];
			if (entry.name.empty())
				continue;
			if (index != 0)
				require(svg::named_srgb_colors[index - 1].name < entry.name,
				        "CSS named colors must be unique and sorted for binary lookup");
			const auto expected = [&](std::uint32_t shift)
			{
				return static_cast<float>((entry.rgb >> shift) & 0xffu) / 255.0f;
			};
			const auto parsed = svg::parse_color(entry.name).rgba_glsl();
			require(std::abs(parsed.r - expected(16)) < 1.0e-7f &&
			            std::abs(parsed.g - expected(8)) < 1.0e-7f &&
			            std::abs(parsed.b - expected(0)) < 1.0e-7f && parsed.a == 1.0f,
			        "Every CSS named sRGB color must decode to its exact opaque 8-bit channels");
			auto uppercase = std::string(entry.name);
			std::ranges::transform(uppercase, uppercase.begin(), [](char character)
			                       {
				                       return character >= 'a' && character <= 'z'
				                                  ? static_cast<char>(character - 'a' + 'A')
				                                  : character;
			                       });
			const auto case_insensitive = svg::parse_color(uppercase).rgba_glsl();
			require(case_insensitive.r == parsed.r && case_insensitive.g == parsed.g &&
			            case_insensitive.b == parsed.b && case_insensitive.a == parsed.a,
			        "CSS named colors must be ASCII case-insensitive");
		}
		const auto expect_reference_color = [](std::string_view name, std::uint32_t expected_rgb)
		{
			const auto actual = svg::parse_color(name).rgba_glsl();
			const auto expected = [&](std::uint32_t shift)
			{
				return static_cast<float>((expected_rgb >> shift) & 0xffu) / 255.0f;
			};
			require(std::abs(actual.r - expected(16)) < 1.0e-7f &&
			            std::abs(actual.g - expected(8)) < 1.0e-7f &&
			            std::abs(actual.b - expected(0)) < 1.0e-7f && actual.a == 1.0f,
			        "CSS named colors must match independent sRGB reference values");
		};
		expect_reference_color("aliceblue", 0xf0f8ffu);
		expect_reference_color("darkslategray", 0x2f4f4fu);
		expect_reference_color("rebeccapurple", 0x663399u);
		expect_reference_color("yellowgreen", 0x9acd32u);

		const auto document = svg::parse(R"svg(<svg>
		    <defs>
		        <linearGradient id="named-stops">
		            <stop stop-color="Lime"/><stop offset="1" stop-color="RebeccaPurple"/>
		        </linearGradient>
		        <filter id="flood-filter" color-interpolation-filters="sRGB">
		            <feFlood flood-color="Navy" flood-opacity="150%"/>
		        </filter>
		        <filter id="clamp-filter" color-interpolation-filters="sRGB">
		            <feFlood flood-color="rgba(300,-10,128,150%)" flood-opacity="150%"/>
		            <feFlood flood-color="lime" flood-opacity="-10%"/>
		        </filter>
		    </defs>
		    <rect id="navy-fill" width="4" height="4" fill="NAVY"/>
		    <path id="lime-stroke" d="M0 6H4" fill="none" stroke="lime" stroke-width="2" stroke-opacity="150%"/>
		    <rect id="green" x="5" width="4" height="4" fill="GREEN"/>
		    <rect id="gray" x="10" width="4" height="4" fill="Gray"/>
		    <rect id="named-gradient" x="15" width="4" height="4" fill="url(#named-stops)"/>
		    <rect id="named-flood" x="20" width="4" height="4" filter="url(#flood-filter)"/>
		    <rect id="clamped-color" x="25" width="4" height="4" fill="rgba(300,-10,128,150%)"/>
		    <rect id="fill-opacity-high" x="30" width="4" height="4" fill="navy" fill-opacity="150%"/>
		    <rect id="fill-opacity-low" x="35" width="4" height="4" fill="navy" fill-opacity="-10%"/>
		    <rect id="current-color" x="40" width="4" height="4" color="RebeccaPurple" fill="currentColor"/>
		    <g opacity="150%"><rect id="opacity-high" x="45" width="4" height="4" fill="lime"/></g>
		    <g opacity="-10%"><rect id="opacity-low" x="50" width="4" height="4" fill="lime"/></g>
		    <rect id="clamped-filter" x="55" width="4" height="4" filter="url(#clamp-filter)"/>
		    </svg>)svg");
		const auto paint_for = [&](std::string_view id) -> const svg::geometry_data&
		{
			const auto found = std::ranges::find_if(document.geometry,
			                                      [&](const auto& part) { return part.svg_element.id == id; });
			if (found == document.geometry.end())
				throw std::runtime_error("Expected named-color SVG geometry was not found");
			return *found;
		};
		const auto layer_for_source = [&](std::string_view id) -> const svg::geometry_data&
		{
			const auto found = std::ranges::find_if(document.geometry, [&](const auto& part)
			                                       {
				                                       return part.effect &&
				                                              std::ranges::any_of(part.effect->source,
				                                                  [&](const auto& source)
				                                                  { return source.svg_element.id == id; });
			                                       });
			if (found == document.geometry.end())
				throw std::runtime_error("Expected named-color SVG effect layer was not found");
			return *found;
		};
		const auto rgba = [&](std::string_view id) { return paint_for(id).tint.rgba_glsl(); };
		const float byte_128 = 128.0f / 255.0f;
		const auto navy = rgba("navy-fill");
		const auto lime = rgba("lime-stroke");
		const auto green = rgba("green");
		const auto gray = rgba("gray");
		const auto clamped = rgba("clamped-color");
		const auto alpha_low = svg::parse_color("rgba(0,10,20,-10%)").rgba_glsl();
		require(navy.r == 0.0f && navy.g == 0.0f && std::abs(navy.b - byte_128) < 1.0e-7f,
		        "Navy must use the exact #000080 channel values");
		require(lime.g == 1.0f && lime.r == 0.0f && lime.b == 0.0f && lime.a == 1.0f,
		        "Lime must be an opaque full-green sRGB color");
		require(std::abs(green.g - byte_128) < 1.0e-7f &&
		            std::abs(gray.r - byte_128) < 1.0e-7f && std::abs(gray.g - byte_128) < 1.0e-7f &&
		            std::abs(gray.b - byte_128) < 1.0e-7f,
		        "Green and gray must resolve to exact 8-bit channel values divided by 255");
		require(clamped.r == 1.0f && clamped.g == 0.0f &&
		            std::abs(clamped.b - byte_128) < 1.0e-7f && clamped.a == 1.0f,
		        "rgba() RGB channels and alpha must clamp to their specified ranges");
		require(alpha_low.a == 0.0f, "Negative rgba() alpha must clamp to zero");
		require(std::abs(rgba("fill-opacity-high").a - 1.0f) < 1.0e-7f &&
		            std::abs(rgba("fill-opacity-low").a) < 1.0e-7f &&
		            std::abs(rgba("current-color").r - 102.0f / 255.0f) < 1.0e-7f &&
		            std::abs(rgba("current-color").b - 153.0f / 255.0f) < 1.0e-7f,
		        "Opacity must clamp and currentColor must remain contextually resolved");
		const auto& gradient_stops = paint_for("named-gradient").gradient->definition->stops;
		require(gradient_stops.size() == 2 &&
		            gradient_stops[0].value.rgba_glsl().g == 1.0f &&
		            std::abs(gradient_stops[1].value.rgba_glsl().r - 102.0f / 255.0f) < 1.0e-7f &&
		            std::abs(gradient_stops[1].value.rgba_glsl().g - 51.0f / 255.0f) < 1.0e-7f &&
		            std::abs(gradient_stops[1].value.rgba_glsl().b - 153.0f / 255.0f) < 1.0e-7f,
		        "Named colors must work for gradient stops including rebeccapurple");
		const auto& flood_steps = layer_for_source("named-flood").effect->filter->steps;
		require(flood_steps.size() == 1 && flood_steps[0].parameters.r == 0.0f &&
		            flood_steps[0].parameters.g == 0.0f &&
		            std::abs(flood_steps[0].parameters.b - byte_128) < 1.0e-7f &&
		            flood_steps[0].parameters.a == 1.0f,
		        "Named feFlood colors and flood-opacity clamping must preserve exact sRGB channels");
		const auto& clamp_steps = layer_for_source("clamped-filter").effect->filter->steps;
		require(clamp_steps.size() == 2 && clamp_steps[0].parameters.r == 1.0f &&
		            clamp_steps[0].parameters.g == 0.0f &&
		            std::abs(clamp_steps[0].parameters.b - byte_128) < 1.0e-7f &&
		            clamp_steps[0].parameters.a == 1.0f &&
		            clamp_steps[1].parameters.g == 1.0f && clamp_steps[1].parameters.a == 0.0f,
		        "Filter color channels, color alpha, and flood-opacity must clamp to their ranges");
		const auto high_opacity = rgba("opacity-high");
		const auto& low_opacity = layer_for_source("opacity-low");
		require(high_opacity.a == 1.0f && low_opacity.tint.rgba_glsl().a == 0.0f,
		        "Element opacity values outside the range must clamp for both bounds");

		bool system_color_rejected = false;
		try
		{
			(void)svg::parse_color("Canvas");
		}
		catch (const std::invalid_argument&)
		{
			system_color_rejected = true;
		}
		require(system_color_rejected, "CSS system colors must remain unsupported");
	}

	void paint_server_fallbacks()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        #cascade { fill: url(#missing) #ffa500; }
		    </style>
		    <defs><linearGradient id="valid"><stop stop-color="red"/>
		        <stop offset="1" stop-color="blue"/></linearGradient></defs>
		    <rect id="valid-gradient" width="8" height="8" fill="url(#valid) green"/>
		    <rect id="missing-fill" x="10" width="8" height="8" fill="url(#missing) red"/>
		    <path id="missing-stroke" d="M0 12H8" fill="none" stroke="url(#missing) green"
		          stroke-width="2"/>
		    <rect id="none-fallback" x="20" width="8" height="8" fill="url(#missing) none"/>
		    <rect id="cascade" x="30" width="8" height="8" fill="url(#valid) red"/>
		    <rect id="single-quoted-valid" x="40" width="8" height="8" fill="url('#valid') green"/>
		    <rect id="double-quoted-valid" x="50" width="8" height="8" fill='url("#valid") red'/>
		    <rect id="single-quoted-fallback" x="60" width="8" height="8" fill="url('#missing') orange"/>
		    <path id="double-quoted-fallback" d="M0 22H8" fill="none"
		          stroke='url("#missing") blue' stroke-width="2"/>
		    <rect id="single-quoted-none" x="70" width="8" height="8" fill="url('#missing') none"/>
		    <rect id="double-quoted-none" x="80" width="8" height="8" fill='url("#missing") none'/>
		    </svg>)svg");
		const auto find = [&](std::string_view id) -> const svg::geometry_data*
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return &part;
			}
			return nullptr;
		};
		const auto* valid = find("valid-gradient");
		const auto* missing_fill = find("missing-fill");
		const auto* missing_stroke = find("missing-stroke");
		const auto* cascade = find("cascade");
		const auto* single_quoted_valid = find("single-quoted-valid");
		const auto* double_quoted_valid = find("double-quoted-valid");
		const auto* single_quoted_fallback = find("single-quoted-fallback");
		const auto* double_quoted_fallback = find("double-quoted-fallback");
		require(valid && valid->gradient, "A usable gradient must win over its fallback color");
		require(missing_fill && !missing_fill->gradient && missing_fill->tint.r > 0.99f,
		        "A missing fill gradient must use its fallback color");
		require(missing_stroke && !missing_stroke->gradient && missing_stroke->tint.g > 0.49f,
		        "A missing stroke gradient must use its fallback color");
		require(find("none-fallback") == nullptr,
		        "A missing paint server with none fallback must produce no fill paint");
		require(cascade && cascade->tint.r > 0.99f && cascade->tint.g > 0.49f,
		        "The higher-specificity CSS paint declaration must use its own fallback color");
		require(single_quoted_valid && single_quoted_valid->gradient && double_quoted_valid &&
		            double_quoted_valid->gradient,
		        "Single- and double-quoted local paint URLs must resolve valid gradients");
		require(single_quoted_fallback && !single_quoted_fallback->gradient &&
		            single_quoted_fallback->tint.r > .99f && single_quoted_fallback->tint.g > .49f &&
		            single_quoted_fallback->tint.b < .01f && double_quoted_fallback &&
		            !double_quoted_fallback->gradient && double_quoted_fallback->tint.b > .99f,
		        "Quoted missing paint URLs must use their fill or stroke fallback colors");
		require(find("single-quoted-none") == nullptr && find("double-quoted-none") == nullptr,
		        "Quoted missing paint URLs with none fallback must emit no paint");
	}

	void definition_site_inherited_styles()
	{
		const auto document = svg::parse(R"svg(<svg viewBox="0 0 64 32">
		    <defs color="red" fill="white" stop-color="red" stop-opacity=".25">
		        <linearGradient id="inherited-gradient">
		            <stop stop-color="currentColor"/><stop offset="1" stop-color="currentColor"/>
		        </linearGradient>
		        <linearGradient id="stop-defaults">
		            <stop offset="0"/><stop offset=".5" stop-color="currentColor" stop-opacity=".5" opacity=".1"/>
		            <stop offset="1" stop-color="inherit" stop-opacity="inherit"/>
		        </linearGradient>
	        <mask id="inherited-mask"><rect width="8" height="8"/></mask>
		        <g color="blue" fill="black">
		            <linearGradient id="local-gradient">
		                <stop stop-color="currentColor"/><stop offset="1" stop-color="currentColor"/>
		            </linearGradient>
		            <mask id="local-mask"><rect width="8" height="8"/></mask>
		        </g>
		    </defs>
		    <rect id="gradient-one" width="8" height="8" fill="url(#inherited-gradient)"/>
		    <rect id="gradient-two" x="10" width="8" height="8" fill="url(#local-gradient)"/>
		    <rect id="gradient-stops" x="40" width="8" height="8" fill="url(#stop-defaults)"/>
		    <rect x="20" width="8" height="8" fill="red" mask="url(#inherited-mask)"/>
		    <rect x="30" width="8" height="8" fill="red" mask="url(#local-mask)"/>
		    </svg>)svg");
		const auto find_gradient = [&](std::string_view id) -> const svg::geometry_data*
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return &part;
			}
			return nullptr;
		};
		const auto* inherited_gradient = find_gradient("gradient-one");
		const auto* local_gradient = find_gradient("gradient-two");
		require(inherited_gradient && inherited_gradient->gradient && local_gradient && local_gradient->gradient,
		        "Gradients styled at their definition sites must resolve on painted elements");
		const auto inherited_stop = inherited_gradient->gradient->definition->stops.front().value.rgba_glsl();
		const auto local_stop = local_gradient->gradient->definition->stops.front().value.rgba_glsl();
		require(inherited_stop.r > 0.99f && inherited_stop.g < 0.01f,
		        "A gradient stop must inherit currentColor from its original defs ancestor");
		require(local_stop.b > 0.99f && local_stop.r < 0.01f,
		        "A gradient stop must inherit currentColor from its local definition group");
		const auto* stop_defaults = find_gradient("gradient-stops");
		require(stop_defaults && stop_defaults->gradient && stop_defaults->gradient->definition->stops.size() == 3,
		        "Stops with explicit inheritance must remain available to their gradient");
		const auto stops = stop_defaults->gradient->definition->stops;
		const auto default_stop = stops[0].value.rgba_glsl();
		const auto current_stop = stops[1].value.rgba_glsl();
		const auto inherited_stop_value = stops[2].value.rgba_glsl();
		require(default_stop.r < .01f && default_stop.g < .01f && default_stop.b < .01f && default_stop.a == 1.0f,
		        "An unstyled stop must use its own opaque black defaults instead of ancestor stop paint");
		require(current_stop.r > .99f && current_stop.g < .01f && current_stop.b < .01f &&
		            std::abs(current_stop.a - .5f) < .001f,
		        "currentColor and stop-opacity apply locally while ordinary opacity does not replace stop-opacity");
		require(inherited_stop_value.r > .99f && inherited_stop_value.g < .01f &&
		            std::abs(inherited_stop_value.a - .25f) < .001f,
		        "Explicit stop-color and stop-opacity inherit must use definition ancestors");

		std::vector<glm::vec3> mask_colors;
		for (const auto& part : document.geometry)
		{
			if (part.effect && part.effect->mask && !part.effect->mask_source.empty())
			{
				const auto tint = part.effect->mask_source.front().tint.rgba_glsl();
				mask_colors.emplace_back(tint.r, tint.g, tint.b);
			}
		}
		require(mask_colors.size() == 2, "Both mask definitions must produce mask content");
		require(glm::length(mask_colors[0] - glm::vec3(1.0f)) < 0.01f,
		        "Mask content must inherit fill from its original defs ancestor");
		require(glm::length(mask_colors[1] - glm::vec3(0.0f)) < 0.01f,
		        "Mask content must inherit fill from its local definition group");
	}

	void empty_gradients_produce_no_paint()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <defs><linearGradient id="empty-fill"/><radialGradient id="empty-stroke"/></defs>
		    <rect id="no-fill" width="8" height="8" fill="url(#empty-fill)"/>
		    <path id="no-stroke" d="M0 10H8" fill="none" stroke="url(#empty-stroke)"/>
		    <rect id="fill-fallback" x="10" width="8" height="8" fill="url(#empty-fill) red"/>
		    <path id="stroke-fallback" d="M20 10H28" fill="none"
		          stroke="url(#empty-stroke) blue" stroke-width="2"/>
		    </svg>)svg");
		const auto find = [&](std::string_view id) -> const svg::geometry_data*
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return &part;
			}
			return nullptr;
		};
		require(find("no-fill") == nullptr && find("no-stroke") == nullptr,
		        "A gradient without stops must not create fill or stroke paint");
		const auto* fill_fallback = find("fill-fallback");
		const auto* stroke_fallback = find("stroke-fallback");
		require(fill_fallback && fill_fallback->tint.r > 0.99f && fill_fallback->tint.b < 0.01f,
		        "An empty fill gradient must use its fallback color");
		require(stroke_fallback && stroke_fallback->tint.b > 0.99f && stroke_fallback->tint.r < 0.01f,
		        "An empty stroke gradient must use its fallback color");
	}

	void pointer_events_none_preserves_paint_without_hit_geometry()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <defs><filter id="blur"><feGaussianBlur stdDeviation="1"/></filter></defs>
		    <g pointer-events="none">
		      <path id="inherited" d="M0 0H10V10Z" fill="red" stroke="blue" stroke-width="2"/>
		      <rect id="reenabled" x="20" width="10" height="10" pointer-events="auto"/>
		      <rect id="effect" x="40" width="10" height="10" filter="url(#blur)"/>
		    </g>
		  </svg>)svg");
		const auto find = [&](std::string_view id)
		{
			std::vector<const svg::geometry_data*> parts;
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					parts.push_back(&part);
			}
			return parts;
		};
		const auto inherited = find("inherited");
		require(inherited.size() == 2, "pointer-events must preserve fill and stroke paint parts");
		require(std::ranges::all_of(inherited, [](const auto* part) { return !part->pointer_events; }),
		        "pointer-events none must disable hit geometry for both fill and stroke");
		require(!inherited[0]->triangles.empty() && !inherited[1]->triangles.empty(),
		        "pointer-events none must leave fill and stroke rendering geometry intact");

		const auto reenabled = find("reenabled");
		require(reenabled.size() == 1 && reenabled.front()->pointer_events,
		        "A descendant's pointer-events auto must override inherited none");

		const auto effect = std::ranges::find_if(document.geometry,
		                                         [](const auto& part) { return part.effect != nullptr; });
		require(effect != document.geometry.end(), "The filtered shape must retain its effect paint layer");
		require(!effect->effect->source.empty() &&
		            std::ranges::all_of(effect->effect->source,
		                                [](const auto& part) { return !part.pointer_events; }),
		        "pointer-events none must propagate to hit-tested geometry inside effects");
	}

	void gaussian_blur_edge_modes()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <defs>
		      <filter id="default"><feGaussianBlur stdDeviation="1"/></filter>
		      <filter id="duplicate"><feGaussianBlur stdDeviation="1" edgeMode="duplicate"/></filter>
		      <filter id="wrap"><feGaussianBlur stdDeviation="1" edgeMode="wrap"/></filter>
		    </defs>
		    <rect width="10" height="10" filter="url(#default)"/>
		    <rect x="20" width="10" height="10" filter="url(#duplicate)"/>
		    <rect x="40" width="10" height="10" filter="url(#wrap)"/>
		  </svg>)svg");
		std::vector<std::uint32_t> modes;
		for (const auto& part : document.geometry)
		{
			if (!part.effect || !part.effect->filter)
				continue;
			const auto blur = std::ranges::find_if(part.effect->filter->steps,
			                                       [](const auto& step) { return step.operation == svg::filter_op::blur; });
			if (blur != part.effect->filter->steps.end())
				modes.push_back(blur->mode);
		}
		require(modes == std::vector<std::uint32_t>{0, 1, 2},
		        "Gaussian blur must preserve none, duplicate, and wrap edgeMode values");

		bool rejected = false;
		try
		{
			(void)svg::parse(R"svg(<svg><defs><filter id="bad">
			    <feGaussianBlur stdDeviation="1" edgeMode="clamp"/>
			  </filter></defs><rect width="10" height="10" filter="url(#bad)"/></svg>)svg");
		}
		catch (const std::invalid_argument&)
		{
			rejected = true;
		}
		require(rejected, "An unsupported Gaussian blur edgeMode must be rejected");
	}

	void svg_temporary_targets_share_a_bounded_cache()
	{
		struct fake_target
		{
			glm::uvec2 size{};
			[[nodiscard]] glm::uvec2 get_size() const noexcept { return size; }
		};
		auto budget = std::make_shared<svg::temporary_target_budget>();
		budget->byte_limit = 16u * 16u * 8u;
		budget->target_limit = 2;
		svg::temporary_target_pool<fake_target> output_targets(budget);
		svg::temporary_target_pool<fake_target> filter_targets(budget);
		std::size_t allocations = 0;
		const auto create = [&](glm::uvec2 size)
		{
			++allocations;
			return std::make_shared<fake_target>(fake_target{size});
		};

		auto first = output_targets.acquire({16, 16}, create);
		std::weak_ptr<fake_target> previous = first;
		const auto first_identity = first.get();
		first.reset();
		auto reused = output_targets.acquire({16, 16}, create);
		require(reused.get() == first_identity && allocations == 1,
		        "Repeated effect frames must reuse an idle render target without a new allocation");

		bool over_budget_rejected = false;
		try
		{
			(void)filter_targets.acquire({8, 8}, create);
		}
		catch (const std::length_error&)
		{
			over_budget_rejected = true;
		}
		require(over_budget_rejected && allocations == 1 && output_targets.size() == 1 &&
		            filter_targets.size() == 0 && budget->cached_bytes == 16u * 16u * 8u,
		        "An active target that fills the shared budget must reject another allocation before its factory runs");
		reused.reset();
		auto cached_small = filter_targets.acquire({8, 8}, create);
		require(output_targets.size() == 0 && filter_targets.size() == 1 &&
		            budget->cached_bytes == 8u * 8u * 8u && previous.expired() && allocations == 2,
		        "The aggregate cache must evict idle targets across effect stages to meet its byte budget");
		const auto small_identity = cached_small.get();
		cached_small.reset();
		auto reused_small = filter_targets.acquire({8, 8}, create);
		require(reused_small.get() == small_identity && allocations == 2,
		        "Targets of a repeated size must remain reusable after aggregate eviction");
	}

	void css_forgiving_selector_lists()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        @media (min-width: 900px) { rect:unknown-media-pseudo { fill: red; } }
		        .comma { fill: #00ff00; }
		        .comma, rect:unknown-comma-pseudo { fill: #ff0000; }
		        .not-target:not(.skip, :unknown-not-pseudo) { fill: #ff0000; }
		        .after { fill: #0000ff; }
		        .shown { fill: #00ff00; }
		        .shown:is(:unknown-is-pseudo, .active) { fill: #0000ff; }
		        .where-target:where(:unknown-where-pseudo, .active) { fill: #ff00ff; }
		    </style>
		    <rect id="comma" class="comma" width="2" height="2"/>
		    <rect id="not-target" class="not-target" x="3" width="2" height="2"/>
		    <rect id="after" class="after" x="6" width="2" height="2"/>
		    <rect id="shown-active" class="shown active" x="9" width="2" height="2"/>
		    <rect id="shown-plain" class="shown" x="12" width="2" height="2"/>
		    <rect id="where-active" class="where-target active" x="15" width="2" height="2"/>
		    </svg>)svg");
		require(document.geometry.size() == 6,
		        "Unsupported CSS selectors must not prevent valid rules and shapes from loading");

		const auto tint_for = [&](std::string_view id)
		{
			for (const auto& part : document.geometry)
			{
				if (part.svg_element.id == id)
					return part.tint.rgba_glsl();
			}
			throw std::runtime_error("Expected SVG shape was not found");
		};
		const auto expect_color = [&](std::string_view id, glm::vec3 expected)
		{
			const auto actual = tint_for(id);
			require(std::abs(actual.r - expected.r) < 0.01f && std::abs(actual.g - expected.g) < 0.01f &&
			            std::abs(actual.b - expected.b) < 0.01f,
			        "Forgiving selector handling produced the wrong CSS cascade");
		};
		expect_color("comma", {0, 1, 0});
		expect_color("not-target", {0, 0, 0});
		expect_color("after", {0, 0, 1});
		expect_color("shown-active", {0, 0, 1});
		expect_color("shown-plain", {0, 1, 0});
		expect_color("where-active", {1, 0, 1});
	}

	void css_rule_index_preserves_cascade_candidates()
	{
		std::string source = R"svg(<svg><style>
		    rect { fill: #000000; }
		    .common { fill: #00ff00; }
		    .parent .target { fill: #0000ff; }
		    rect:is(.functional) { fill: #ffff00; }
		    #indexed-last { fill: #ff00ff; }
		    )svg";
		for (int index = 0; index < 128; ++index)
		{
			source += ".token-" + std::to_string(index) + " { fill: #ff0000; }\n";
		}
		source += R"svg(</style>)svg";
		for (int index = 0; index < 128; ++index)
		{
			source += "<rect id=\"indexed-" + std::to_string(index) + "\" class=\"common token-" +
			          std::to_string(index) + "\" width=\"1\" height=\"1\"/>";
		}
		source += R"svg(<g class="parent"><rect id="descendant" class="target" width="1" height="1"/></g>
		    <rect id="indexed-last" width="1" height="1"/>
		    <rect id="functional" class="functional" width="1" height="1"/>
		    </svg>)svg";
		const auto document = svg::parse(source);
		require(document.geometry.size() == 131,
		        "Indexed class, type, ID, and universal selector candidates must retain every shape");
		const auto color_for = [&](std::string_view id)
		{
			const auto found = std::ranges::find_if(document.geometry,
			                                       [&](const auto& part) { return part.svg_element.id == id; });
			require(found != document.geometry.end(), "Expected CSS-indexed shape was not found");
			return found->tint.rgba_glsl();
		};
		for (int index : {0, 63, 127})
		{
			const auto actual = color_for("indexed-" + std::to_string(index));
			require(actual.r > .95f && actual.g < .05f && actual.b < .05f,
			        "Rightmost class indexing must preserve same-specificity source order");
		}
		const auto descendant = color_for("descendant");
		const auto by_id = color_for("indexed-last");
		const auto functional = color_for("functional");
		require(descendant.b > .95f && by_id.r > .95f && by_id.b > .95f &&
		            functional.r > .95f && functional.g > .95f && functional.b < .05f,
		        "Selector indexing must retain descendant, ID, and functional pseudo-class matches");
	}

	void xml_character_references()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        [data-label="A&amp;B"] { fill: red; }
		        [data-label="C&#xE9;D"] { fill: yellow; }
		        <![CDATA[[data-label="A&amp;B"] { fill: blue; }]]>
		    </style>
		    <rect id="normal" data-label="A&amp;B" width="2" height="2"/>
		    <rect id="cdata-literal" data-label="A&amp;amp;B" x="3" width="2" height="2"/>
		    <rect id="r&#xE9;sum&#233;" x="6" width="2" height="2" fill="yellow"/>
		    <rect id="numeric-selector" data-label="C&#233;D" x="9" width="2" height="2"/>
		    </svg>)svg");
		require(document.geometry.size() == 4, "Character references must preserve all SVG elements");
		require(document.geometry[0].svg_element.id == "normal" &&
		            document.geometry[1].svg_element.id == "cdata-literal" &&
		            document.geometry[2].svg_element.id == "r\xc3\xa9sum\xc3\xa9" &&
		            document.geometry[3].svg_element.id == "numeric-selector",
		        "Decimal and hexadecimal character references must decode to UTF-8 in attributes");
		const auto normal = document.geometry[0].tint.rgba_glsl();
		const auto cdata_literal = document.geometry[1].tint.rgba_glsl();
		const auto numeric_selector = document.geometry[3].tint.rgba_glsl();
		require(normal.r > 0.95f && normal.g < 0.05f && normal.b < 0.05f,
		        "Entity references in stylesheet text must decode before CSS selector parsing");
		require(cdata_literal.b > 0.95f && cdata_literal.r < 0.05f,
		        "Entity references inside CDATA must remain literal stylesheet characters");
		require(numeric_selector.r > 0.95f && numeric_selector.g > 0.95f && numeric_selector.b < 0.05f,
		        "Numeric character references in stylesheet text and attributes must match after UTF-8 decoding");
	}

	void css_geometry_properties_and_empty_paths()
	{
		const auto document = svg::parse(R"svg(<svg width="100" height="80" viewBox="0 0 100 80">
		    <style>
		        rect { x: 10px; y: 7px; width: 22px; height: 16px; fill: red; }
		        #css-path { d: path("M40 0 H50 V10 H40 Z"); fill: blue; }
		        #no-path { d: none; }
		    </style>
	    <rect id="css-rect" width="8" height="8"/>
	    <path id="placeholder"/>
	    <path id="no-path" d="M0 0 H4 V4 Z"/>
	    <path id="css-path"/>
	    </svg>)svg");
		require(document.geometry.size() == 2,
		        "Paths with missing or none d values must contribute no geometry while CSS d is supported");
		require(document.geometry[0].svg_element.id == "css-rect" &&
		            document.geometry[1].svg_element.id == "css-path",
		        "Only the cascaded rectangle and CSS-defined path should create geometry");
		require(area(document.geometry[0]) == 22.0f * 16.0f && area(document.geometry[1]) == 100.0f,
		        "CSS geometry properties must determine rectangle bounds and path data");
		const auto rectangle_color = document.geometry[0].tint.rgba_glsl();
		const auto path_color = document.geometry[1].tint.rgba_glsl();
		require(rectangle_color.r > 0.95f && rectangle_color.g < 0.05f &&
		            path_color.b > 0.95f && path_color.r < 0.05f,
		        "CSS geometry declarations must coexist with paint declarations in the cascade");
		const auto bounds_of = [](const svg::geometry_data& part)
		{
			glm::vec2 minimum(std::numeric_limits<float>::max());
			glm::vec2 maximum(std::numeric_limits<float>::lowest());
			for (const auto point : part.triangle_view())
			{
				const auto transformed = svg::apply_transform(part.transform, point);
				minimum = glm::min(minimum, transformed);
				maximum = glm::max(maximum, transformed);
			}
			return glm::vec4(minimum, maximum - minimum);
		};
		const auto close_bounds = [](glm::vec4 actual, glm::vec4 expected)
		{
			return glm::length(glm::vec2(actual.x, actual.y) - glm::vec2(expected.x, expected.y)) < .001f &&
			       glm::length(glm::vec2(actual.z, actual.w) - glm::vec2(expected.z, expected.w)) < .001f;
		};
		require(close_bounds(bounds_of(document.geometry[0]), {10, 57, 22, 16}),
		        "CSS x, y, width, and height must set exact transformed rectangle bounds");
		const auto ordinary_path = svg::parse(R"svg(<svg width="100" height="80" viewBox="0 0 100 80">
		    <path d="M40 0 H50 V10 H40 Z"/>
		    </svg>)svg");
		require(ordinary_path.geometry.size() == 1 && same_mesh(document.geometry[1], ordinary_path.geometry.front()) &&
		            close_bounds(bounds_of(document.geometry[1]), {40, 70, 10, 10}),
		        "CSS d:path() must match the mesh and bounds from equivalent d attribute data");
	}

	void use_references_and_symbol_viewports()
	{
		const auto document = svg::parse(R"svg(<svg width="100" height="100" viewBox="0 0 100 100">
		    <rect id="tile" width="8" height="8" fill="red"/>
		    <use href="#tile" x="50%" y="25%"/>
		    <defs>
		        <symbol id="icon" viewBox="0 0 8 8">
		            <rect id="icon-shape" width="8" height="8" fill="blue"/>
		        </symbol>
		    </defs>
		    <use href="#icon" x="25%" y="10%" width="16" height="16"/>
		    </svg>)svg");
		require(document.geometry.size() == 3, "Ordinary targets and symbol instances must both render");
		require(document.geometry[0].svg_element.id == "tile" &&
		            document.geometry[1].svg_element.id == "tile" &&
		            document.geometry[2].svg_element.id == "icon-shape",
		        "Use instances must preserve the referenced element identity");

		const auto transformed_bounds = [](const svg::geometry_data& part)
		{
			glm::vec2 minimum(std::numeric_limits<float>::max());
			glm::vec2 maximum(std::numeric_limits<float>::lowest());
			for (const auto point : part.triangle_view())
			{
				const auto transformed = svg::apply_transform(part.transform, point);
				minimum = glm::min(minimum, transformed);
				maximum = glm::max(maximum, transformed);
			}
			return std::pair{minimum, maximum};
		};
		const auto original = transformed_bounds(document.geometry[0]);
		const auto translated = transformed_bounds(document.geometry[1]);
		const auto symbol = transformed_bounds(document.geometry[2]);
		require(original.first.x == 0.0f && original.second.x == 8.0f &&
		            translated.first.x == 50.0f && translated.second.x == 58.0f &&
		            translated.first.y == 67.0f && translated.second.y == 75.0f,
		        "An ordinary same-document use target must honor its percentage x and y placement");
		require(symbol.first.x == 25.0f && symbol.second.x == 41.0f &&
		            std::abs(symbol.second.y - symbol.first.y) == 16.0f,
		        "A symbol viewBox must scale its child geometry into the use viewport");

		const auto percentage_symbol = svg::parse(R"svg(<svg width="100" height="100" viewBox="0 0 100 100">
		    <defs><symbol id="percent" viewBox="0 0 8 8">
		        <rect width="50%" height="50%" rx="25%" ry="25%"/>
		        <image x="50%" y="25%" width="25%" height="50%" href=")svg" +
		                                         std::string(image_uri) + R"svg("/>
		    </symbol><symbol id="absolute" viewBox="0 0 8 8">
		        <rect width="4" height="4" rx="1" ry="1"/>
		    </symbol></defs>
		    <use href="#percent" width="16" height="16"/>
		    <use href="#absolute" x="20" width="16" height="16"/>
		    </svg>)svg");
		require(percentage_symbol.geometry.size() == 3 &&
		            same_mesh(percentage_symbol.geometry[0], percentage_symbol.geometry[2]),
		        "Symbol geometry percentages must use its viewport dimensions for width, height, rx, and ry");
		const auto& symbol_image = *percentage_symbol.geometry[1].image;
		require(symbol_image.bounds.pos == glm::vec2(4, 3.5f) &&
		            symbol_image.bounds.size == glm::vec2(2, 1),
		        "Image percentages in a symbol must use the symbol viewport dimensions");

		const auto nonzero_symbol_view_box = svg::parse(R"svg(<svg width="100" height="80" viewBox="0 0 100 80">
		    <defs><symbol id="offset-icon" viewBox="10 20 8 4">
		        <rect id="offset-icon-shape" x="10" y="20" width="4" height="2"/>
		    </symbol></defs>
		    <use href="#offset-icon" x="40" y="30" width="32" height="32"/>
		    </svg>)svg");
		require(nonzero_symbol_view_box.geometry.size() == 1 &&
		            nonzero_symbol_view_box.geometry.front().svg_element.id == "offset-icon-shape",
		        "A symbol with a nonzero viewBox origin must retain its child geometry");
		const auto offset_symbol_bounds = transformed_bounds(nonzero_symbol_view_box.geometry.front());
		require(offset_symbol_bounds.first == glm::vec2(40, 34) &&
		            offset_symbol_bounds.second == glm::vec2(56, 42),
		        "Nested symbol viewBox origin must be removed before fitting into the use viewport");

		bool rejected_cycle = false;
		try
		{
			(void)svg::parse(R"svg(<svg><use id="first" href="#second"/>
			    <use id="second" href="#first"/></svg>)svg");
		}
		catch (const std::invalid_argument&)
		{
			rejected_cycle = true;
		}
		require(rejected_cycle, "Expanding ordinary use targets must retain cycle detection");
	}

	void gradient_templates()
	{
		const auto document = svg::parse(R"svg(<svg width="100" height="100" viewBox="0 0 100 100">
		    <defs>
		        <linearGradient id="base" gradientUnits="userSpaceOnUse" spreadMethod="reflect"
		                        gradientTransform="translate(2, 3)" x1="10%" x2="90%">
		            <stop stop-color="red"/><stop offset="1" stop-color="blue"/>
		        </linearGradient>
		        <linearGradient id="middle" href="#base" y2="20%"/>
		        <linearGradient id="derived" href="#middle" x2="0" y2="100%"/>
		        <linearGradient id="local-stops" href="#derived" x1="25%">
		            <stop stop-color="green"/>
		        </linearGradient>
		    </defs>
		    <rect id="inherited" width="10" height="10" fill="url(#derived)"/>
		    <rect id="local" x="12" width="10" height="10" fill="url(#local-stops)"/>
		    </svg>)svg");
		require(document.geometry.size() == 2 && document.geometry[0].gradient &&
		            document.geometry[1].gradient,
		        "Gradient templates must resolve to paint definitions");
		const auto& inherited = *document.geometry[0].gradient->definition;
		require(inherited.id == "derived" && inherited.stops.size() == 2 &&
		            inherited.spread == svg::gradient_spread::reflect && !inherited.object_bounding_box &&
		            inherited.transform[3].x == 2.0f && inherited.transform[3].y == 3.0f,
		        "Chained gradient templates must inherit stops and unspecified attributes");
		require(inherited.stops[0].value.rgba_glsl().r > 0.95f &&
		            inherited.stops[1].value.rgba_glsl().b > 0.95f &&
		            document.geometry[0].gradient->parameters0.x == 10.0f &&
		            document.geometry[0].gradient->parameters0.z == 0.0f &&
		            document.geometry[0].gradient->parameters0.w == 100.0f,
		        "Derived gradients must inherit x1 while allowing local x2 and y2 overrides");
		const auto& local = *document.geometry[1].gradient->definition;
		require(local.id == "local-stops" && local.stops.size() == 1 &&
		            local.stops.front().value.g > 0.45f && local.stops.front().value.g < 0.55f &&
		            document.geometry[1].gradient->parameters0.x == 25.0f,
		        "Local gradient stops must replace inherited stops while other attributes remain inherited");
		const auto explicit_defaults = svg::parse(R"svg(<svg><defs>
		    <linearGradient id="base-defaults" gradientUnits="userSpaceOnUse" spreadMethod="reflect">
		      <stop stop-color="red"/><stop offset="1" stop-color="blue"/></linearGradient>
		    <linearGradient id="overrides" href="#base-defaults" gradientUnits="objectBoundingBox" spreadMethod="pad"/>
		    <radialGradient id="radial-base" cx="25%" cy="35%"><stop stop-color="red"/></radialGradient>
		    <radialGradient id="radial-derived" href="#radial-base" cx="75%"/>
		    </defs><rect id="override" width="8" height="8" fill="url(#overrides)"/>
		    <rect id="radial" x="10" width="8" height="8" fill="url(#radial-derived)"/></svg>)svg");
		const auto& overridden = *explicit_defaults.geometry[0].gradient->definition;
		const auto& radial = *explicit_defaults.geometry[1].gradient->definition;
		require(overridden.object_bounding_box && overridden.spread == svg::gradient_spread::pad,
		        "Explicit default gradient units and spread values must override inherited template values");
		require(radial.cx.value == 75.0f && radial.fx.value == radial.cx.value &&
		            radial.cy.value == 35.0f && radial.fy.value == radial.cy.value && !radial.fx_explicit,
		        "An implicit radial focal point must track the derived center unless explicitly inherited");

		const auto rejects = [](std::string_view source)
		{
			try
			{
				(void)svg::parse(source);
				return false;
			}
			catch (const std::invalid_argument&)
			{
				return true;
			}
		};
		require(rejects(R"svg(<svg><defs>
		    <linearGradient id="first" href="#second"/>
		    <linearGradient id="second" href="#first"/>
		    </defs><rect width="2" height="2" fill="url(#first)"/></svg>)svg"),
		        "Cyclic gradient href chains must be rejected safely");
		require(rejects(R"svg(<svg><defs>
		    <linearGradient id="linear" href="#radial"/>
		    <radialGradient id="radial"><stop stop-color="red"/></radialGradient>
		    </defs><rect width="2" height="2" fill="url(#linear)"/></svg>)svg"),
		        "Linear and radial gradients must not inherit across incompatible types");
	}

	void percentage_use_coordinates_in_marker_viewport()
	{
		const auto percentage = svg::parse(R"svg(<svg width="100" height="100" viewBox="0 0 100 100">
		    <defs>
		        <rect id="tile" width="2" height="2" fill="blue"/>
		        <marker id="arrow" markerUnits="userSpaceOnUse" markerWidth="20" markerHeight="10"
		                viewBox="10 2 20 10">
		            <use href="#tile" x="50%" y="25%"/>
		        </marker>
		    </defs>
		    <line id="guide" x1="10" y1="50" x2="30" y2="50" stroke="black" stroke-width="1"
		          marker-end="url(#arrow)"/>
		    </svg>)svg");
		const auto absolute = svg::parse(R"svg(<svg width="100" height="100" viewBox="0 0 100 100">
		    <defs>
		        <rect id="tile" width="2" height="2" fill="blue"/>
		        <marker id="arrow" markerUnits="userSpaceOnUse" markerWidth="20" markerHeight="10"
		                viewBox="10 2 20 10">
		            <use href="#tile" x="10" y="2.5"/>
		        </marker>
	    </defs>
	    <line id="guide" x1="10" y1="50" x2="30" y2="50" stroke="black" stroke-width="1"
	          marker-end="url(#arrow)"/>
	    </svg>)svg");
		require(percentage.geometry.size() == absolute.geometry.size() && percentage.geometry.size() == 2,
		        "Percentage use coordinates inside a marker must preserve marker geometry");
		for (std::size_t part = 0; part < percentage.geometry.size(); ++part)
		{
			const auto& actual = percentage.geometry[part];
			const auto& expected = absolute.geometry[part];
			const auto actual_triangles = actual.triangle_view();
			const auto expected_triangles = expected.triangle_view();
			require(actual.svg_element.id == expected.svg_element.id &&
			            actual_triangles.size() == expected_triangles.size(),
			        "Marker use percentages must keep the owning element and mesh structure");
			for (std::size_t point = 0; point < actual_triangles.size(); ++point)
			{
				const auto actual_world = svg::apply_transform(actual.transform, actual_triangles[point]);
				const auto expected_world = svg::apply_transform(expected.transform, expected_triangles[point]);
				require(glm::length(actual_world - expected_world) < 1.0e-5f,
				        "Marker use percentages must resolve against its local viewport");
			}
		}
	}

	void gradient_color_interpolation()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        .linear { color-interpolation: linearRGB; color-interpolation: invalid; }
		        .parent-linear { color-interpolation: linearRGB; }
		    </style>
		    <defs>
		        <linearGradient id="default"><stop stop-color="black"/><stop offset="1" stop-color="white"/></linearGradient>
		        <linearGradient id="linear" class="linear"><stop stop-color="black"/><stop offset="1" stop-color="white"/></linearGradient>
		        <linearGradient id="inline" style="color-interpolation: linearRGB"><stop stop-color="black"/><stop offset="1" stop-color="white"/></linearGradient>
		    </defs>
		    <defs class="parent-linear">
		        <linearGradient id="inherited"><stop stop-color="black"/><stop offset="1" stop-color="white"/></linearGradient>
		        <linearGradient id="override" color-interpolation="sRGB"><stop stop-color="black"/><stop offset="1" stop-color="white"/></linearGradient>
		    </defs>
		    <rect width="4" height="4" fill="url(#default)"/>
		    <rect x="4" width="4" height="4" fill="url(#linear)"/>
		    <rect x="8" width="4" height="4" fill="url(#inline)"/>
		    <rect x="12" width="4" height="4" fill="url(#inherited)"/>
		    <rect x="16" width="4" height="4" fill="url(#override)"/>
		    </svg>)svg");
		require(document.geometry.size() == 5, "Gradient interpolation fixtures must render");
		require(!document.geometry[0].gradient->definition->linear_rgb,
		        "SVG gradient interpolation must default to sRGB");
		require(document.geometry[1].gradient->definition->linear_rgb,
		        "A valid stylesheet linearRGB value must reach the gradient definition");
		require(document.geometry[2].gradient->definition->linear_rgb,
		        "An inline linearRGB value must reach the gradient definition");
		require(document.geometry[3].gradient->definition->linear_rgb,
		        "A color-interpolation class on defs must inherit into its gradient definitions");
		require(!document.geometry[4].gradient->definition->linear_rgb,
		        "An explicit sRGB value on a gradient must override inherited linearRGB");
	}

	void invalid_css_values_do_not_override_valid_declarations()
	{
		const auto document = svg::parse(R"svg(<svg>
		    <style>
		        .scene path { fill: red; fill: not-a-color; fill-opacity: .5; fill-opacity: invalid; }
		        .important { fill: red !important; }
		        #important { fill: not-a-color !important; fill: blue; }
		    </style>
		    <g class="scene">
		        <path id="sheet" d="M0 0H10V10H0Z"/>
		        <path id="inline" transform="translate(12)" d="M0 0H10V10H0Z"
		              style="fill: blue; fill: invalid; fill-opacity: .25; fill-opacity: invalid"/>
		        <path id="important" class="important" transform="translate(24)" d="M0 0H10V10H0Z"/>
		    </g>
		    </svg>)svg");
		require(document.geometry.size() == 3, "Invalid CSS values must not discard their SVG shapes");
		const auto stylesheet = document.geometry[0].tint.rgba_glsl();
		require(stylesheet.r > .99f && stylesheet.g < .01f && stylesheet.b < .01f &&
		            std::abs(stylesheet.a - .5f) < .001f,
		        "Invalid stylesheet paint and fill-opacity values must leave earlier valid declarations in effect");
		const auto inline_style = document.geometry[1].tint.rgba_glsl();
		require(inline_style.b > .99f && inline_style.r < .01f && inline_style.g < .01f &&
		            std::abs(inline_style.a - .25f) < .001f,
		        "Invalid inline paint and fill-opacity values must leave earlier valid declarations in effect");
		const auto important = document.geometry[2].tint.rgba_glsl();
		require(important.r > .99f && important.g < .01f && important.b < .01f,
		        "Ignoring an invalid important value must preserve valid !important cascade precedence");

		const auto stroke_widths = svg::parse(R"svg(<svg>
		    <style>.css-stroke { stroke: red; stroke-width: 2; stroke-width: invalid; }</style>
		    <path class="css-stroke" d="M0 0H10"/>
		    <path style="stroke: red; stroke-width: 2; stroke-width: invalid" d="M0 5H10"/>
		    </svg>)svg");
		require(stroke_widths.geometry.size() == 2, "Valid stroke widths must survive later invalid values");
		for (const auto& part : stroke_widths.geometry)
			require(std::abs(area(part) - 20.0f) < 0.001f,
			        "Invalid stylesheet and inline stroke-width values must retain the earlier width");
	}

	glm::vec4 transformed_bounds(const svg::geometry_data& part)
	{
		glm::vec2 minimum(std::numeric_limits<float>::max());
		glm::vec2 maximum(std::numeric_limits<float>::lowest());
		for (const auto point : part.triangle_view())
		{
			const auto transformed = svg::apply_transform(part.transform, point);
			minimum = glm::min(minimum, transformed);
			maximum = glm::max(maximum, transformed);
		}
		return {minimum, maximum - minimum};
	}

	void root_viewport_mapping()
	{
		const auto close_bounds = [](glm::vec4 actual, glm::vec4 expected)
		{
			return std::abs(actual.x - expected.x) < 0.001f && std::abs(actual.y - expected.y) < 0.001f &&
			       std::abs(actual.z - expected.z) < 0.001f && std::abs(actual.w - expected.w) < 0.001f;
		};
		const auto meet = svg::parse(R"svg(<svg width="200" height="100" viewBox="0 0 100 100"
		    preserveAspectRatio="xMaxYMid meet"><rect width="50" height="50"/></svg>)svg");
		require(meet.viewport_size == glm::vec2(200, 100), "Root dimensions must define the rendering viewport");
		require(meet.geometry.size() == 1, "Root viewport mapping must preserve shape geometry");
		const auto meet_bounds = transformed_bounds(meet.geometry[0]);
		require(close_bounds(meet_bounds, {100, 50, 50, 50}),
		        "xMaxYMid meet must align the square at x=100 in the 200 by 100 viewport");

		const auto nonzero_view_box_origin = svg::parse(R"svg(<svg width="200" height="100" viewBox="10 20 100 100"
		    preserveAspectRatio="xMaxYMid meet"><rect x="10" y="20" width="50" height="50"/></svg>)svg");
		require(nonzero_view_box_origin.geometry.size() == 1 &&
		            close_bounds(transformed_bounds(nonzero_view_box_origin.geometry.front()), {100, 50, 50, 50}),
		        "Root viewBox min-x/min-y must map its origin to the aligned viewport origin");

		const auto slice = svg::parse(R"svg(<svg width="200" height="100" viewBox="0 0 100 100"
		    preserveAspectRatio="xMidYMid slice"><rect y="25" width="50" height="50"/></svg>)svg");
		const auto slice_bounds = transformed_bounds(slice.geometry[0]);
		require(close_bounds(slice_bounds, {0, 0, 100, 100}),
		        "slice must scale viewBox coordinates uniformly into the viewport");

		const auto clipped_slice = svg::parse(R"svg(<svg width="200" height="100" viewBox="0 0 100 100"
		    preserveAspectRatio="xMidYMid slice"><rect id="overflow" width="100" height="100"/></svg>)svg");
		require(clipped_slice.geometry.size() == 1 && clipped_slice.geometry.front().effect,
		        "Overflowing slice content must composite through the root viewport clip");
		const auto& clip_layer = *clipped_slice.geometry.front().effect;
		require(clip_layer.mask && clip_layer.hit_clip && clip_layer.source.size() == 1 &&
		            clip_layer.region.pos == glm::vec2(0, 25) && clip_layer.region.size == glm::vec2(100, 50) &&
		            clip_layer.mask_source.size() == 1,
		        "Slice clipping must restrict both compositing and hit geometry to the visible viewBox crop");

		const auto none = svg::parse(R"svg(<svg width="200" height="100" viewBox="0 0 100 100"
		    preserveAspectRatio="none"><rect width="50" height="50"/></svg>)svg");
		const auto none_bounds = transformed_bounds(none.geometry[0]);
		require(close_bounds(none_bounds, {0, 50, 100, 50}),
		        "none must scale viewBox coordinates independently on each axis");

		svg::view_tessellation_context context{};
		context.viewport_size = {200, 100};
		const auto percentage = svg::parse(R"svg(<svg width="100%" height="100%" viewBox="0 0 100 100"
		    preserveAspectRatio="xMaxYMid meet"><rect width="50" height="50"/></svg>)svg",
		                                  0.25f, 1'000'000, &context);
		require(percentage.viewport_size == glm::vec2(200, 100),
		        "Percentage root dimensions must resolve against the supplied viewport");
		const auto fallback = svg::parse(R"svg(<svg width="100%" height="100%" viewBox="0 0 100 100"/>)svg");
		require(fallback.viewport_size == glm::vec2(300, 150),
		        "Percentage root dimensions must use the documented 300 by 150 fallback viewport");
		require(percentage.geometry.size() == 1 &&
		            close_bounds(transformed_bounds(percentage.geometry.front()), {100, 50, 50, 50}),
		        "Percentage root dimensions must also drive viewBox fitting and preserveAspectRatio mapping");
	}

	void tight_group_geometry_bounds()
	{
		const auto root_filter = svg::parse(R"svg(<svg viewBox="0 0 64 64" filter="url(#f)">
		    <defs><filter id="f"><feFlood flood-color="red"/></filter></defs>
		    <rect x="8" y="8" width="25" height="25" fill="none"/>
		    </svg>)svg");
		require(root_filter.geometry.size() == 1 && root_filter.geometry.front().effect &&
		            root_filter.geometry.front().effect->filter &&
		            root_filter.geometry.front().effect->source.empty(),
		        "A root SVG filter must use geometric bounds when its child emits no paint");
		require(glm::length(root_filter.geometry.front().effect->region.pos - glm::vec2(5.5f)) < .001f &&
		            glm::length(root_filter.geometry.front().effect->region.size - glm::vec2(30.0f)) < .001f,
		        "Root objectBoundingBox filter defaults must use unpainted child bounds");

		const auto root_mask = svg::parse(R"svg(<svg viewBox="0 0 64 64" mask="url(#m)">
		    <defs><mask id="m" mask-type="alpha" maskUnits="objectBoundingBox"
		              maskContentUnits="objectBoundingBox">
		        <rect width="50%" height="100%" fill="white"/>
		    </mask></defs>
		    <rect x="8" y="8" width="25" height="25" fill="none"/>
		    </svg>)svg");
		require(root_mask.geometry.size() == 1 && root_mask.geometry.front().effect &&
		            root_mask.geometry.front().effect->mask &&
		            root_mask.geometry.front().effect->mask_source.size() == 1,
		        "A root SVG mask must use geometric bounds when its child emits no paint");
		const auto root_mask_point = svg::apply_transform(
		    root_mask.geometry.front().effect->mask_source.front().transform, {.5f, 1.0f});
		require(root_mask_point == glm::vec2(20.5f, 33.0f),
		        "Root objectBoundingBox mask content must map against unpainted child bounds");

		const auto unpainted = svg::parse(R"svg(<svg viewBox="0 0 64 64">
		    <defs><filter id="f"><feFlood flood-color="red"/></filter></defs>
		    <g filter="url(#f)"><rect x="8" y="8" width="25" height="25" fill="none"/></g>
		    </svg>)svg");
		require(unpainted.geometry.size() == 1 && unpainted.geometry.front().effect &&
		            unpainted.geometry.front().effect->filter &&
		            unpainted.geometry.front().effect->source.empty(),
		        "A filter group must use child geometry bounds even when fill emits no paint");
		require(glm::length(unpainted.geometry.front().effect->region.pos - glm::vec2(5.5f)) < .001f &&
		            glm::length(unpainted.geometry.front().effect->region.size - glm::vec2(30.0f)) < .001f,
		        "ObjectBoundingBox filter defaults must use the unpainted rectangle bounds");

		const auto hidden_child = svg::parse(R"svg(<svg viewBox="0 0 64 64">
		    <defs><filter id="f"><feFlood flood-color="red"/></filter></defs>
		    <g filter="url(#f)"><rect x="8" y="8" width="25" height="25" visibility="hidden"/></g>
		    </svg>)svg");
		const auto collapsed_child = svg::parse(R"svg(<svg viewBox="0 0 64 64">
		    <defs><filter id="f"><feFlood flood-color="red"/></filter></defs>
		    <g filter="url(#f)"><rect x="8" y="8" width="25" height="25" visibility="collapse"/></g>
		    </svg>)svg");
		for (const auto* hidden : {&hidden_child, &collapsed_child})
		{
			require(hidden->geometry.size() == 1 && hidden->geometry.front().effect &&
			            hidden->geometry.front().effect->filter && hidden->geometry.front().effect->source.empty() &&
			            hidden->geometry.front().triangle_view().empty(),
			        "Hidden geometry must contribute bounds without emitting paint or hit geometry");
			require(glm::length(hidden->geometry.front().effect->region.pos - glm::vec2(5.5f)) < .001f &&
			            glm::length(hidden->geometry.front().effect->region.size - glm::vec2(30.0f)) < .001f,
			        "Hidden geometry must retain objectBoundingBox filter bounds");
		}

		const auto tight_rotation = svg::parse(R"svg(<svg viewBox="-20 0 64 64">
		    <defs><filter id="f"><feFlood flood-color="red"/></filter></defs>
		    <g filter="url(#f)"><path transform="rotate(45)" d="M0 10H20V20Z" fill="red"/></g>
		    </svg>)svg");
		require(tight_rotation.geometry.size() == 1 && tight_rotation.geometry.front().effect &&
		            tight_rotation.geometry.front().effect->filter,
		        "A rotated triangle must retain its group filter");
		const auto region_size = tight_rotation.geometry.front().effect->region.size;
		require(std::abs(region_size.x - 14.142136f * 1.2f) < .01f &&
		            std::abs(region_size.y - 21.213203f * 1.2f) < .01f,
		        "Rotated triangle filter bounds must use transformed vertices without AABB corner inflation");

		const auto masked = svg::parse(R"svg(<svg viewBox="0 0 64 64">
		    <defs><mask id="m" mask-type="alpha" maskUnits="objectBoundingBox"
		              maskContentUnits="objectBoundingBox">
		        <rect width="50%" height="100%" fill="white"/>
		    </mask></defs>
		    <g mask="url(#m)"><rect x="8" y="8" width="25" height="25" fill="none"/></g>
		    </svg>)svg");
		require(masked.geometry.size() == 1 && masked.geometry.front().effect &&
		            masked.geometry.front().effect->mask && masked.geometry.front().effect->mask_source.size() == 1,
		        "ObjectBoundingBox masks must be built from unpainted child geometry");
		const auto mask_point = svg::apply_transform(masked.geometry.front().effect->mask_source.front().transform,
		                                             {.5f, 1.0f});
		require(mask_point == glm::vec2(20.5f, 33.0f),
		        "ObjectBoundingBox mask content must map against the unpainted rectangle bounds");

		const auto stroked = svg::parse(R"svg(<svg viewBox="0 0 64 64">
		    <defs><filter id="f"><feFlood flood-color="blue"/></filter></defs>
		    <g filter="url(#f)"><rect x="8" y="8" width="25" height="25"
		        fill="none" stroke="red" stroke-width="2"/></g>
		    </svg>)svg");
		require(stroked.geometry.size() == 1 && stroked.geometry.front().effect &&
		            stroked.geometry.front().effect->filter &&
		            stroked.geometry.front().effect->source.size() == 1,
		        "Stroked children must continue to contribute geometry bounds through group effects");

		const auto empty = svg::parse(R"svg(<svg><g opacity=".5"><rect width="10" height="10" fill="none"/></g></svg>)svg");
		require(empty.geometry.empty(), "An unpainted opacity group must not emit a paint layer");
	}

	void svg_use_reuses_compatible_tessellation()
	{
		const auto source = R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
		  <defs><g id="shape">
		    <path id="curved" d="M0 10 C0 0 20 0 20 10 C20 20 0 20 0 10 Z"/>
		    <ellipse id="oval" cx="30" cy="10" rx="10" ry="8"/>
		  </g></defs>
		  <use href="#shape" x="0"/>
		  <use href="#shape" x="20"/>
		  <use href="#shape" transform="translate(40 0) scale(2)"/>
		</svg>)svg";
		svg::view_tessellation_context context{};
		context.curve_tolerance_pixels = 0.05f;
		context.pixels_per_world_unit = 1.0;
		const auto document = svg::parse(source, 0.25f, 100'000, &context);
		require(document.geometry.size() == 6,
		        "Each use instance must preserve its two paint parts in document order");
		const std::array<std::string_view, 6> expected_order{
		    "curved", "oval", "curved", "oval", "curved", "oval"};
		for (std::size_t index = 0; index < expected_order.size(); ++index)
			require(document.geometry[index].svg_element.id == expected_order[index],
			        "Tessellation reuse must preserve SVG use paint order");
		for (const auto id : {"curved", "oval"})
		{
			std::vector<const svg::geometry_data*> instances;
			for (const auto& part : document.geometry)
				if (part.svg_element.id == id)
					instances.push_back(&part);
			require(instances.size() == 3, "Each use instance must keep its own SVG hit identity");
			require(instances[0]->shared_triangles &&
			            instances[0]->shared_triangles == instances[1]->shared_triangles,
			        "Equal-scale use instances must share their tessellated vertices");
			require(instances[0]->svg_element.index != instances[1]->svg_element.index &&
			            instances[1]->svg_element.index != instances[2]->svg_element.index,
			        "Shared tessellation must not merge per-instance hit identities");
			require(instances[0]->shared_triangles != instances[2]->shared_triangles,
			        "A scale change that changes screen-space tolerance must get separate tessellation");
			if (id == "curved")
				require(instances[2]->triangle_view().size() > instances[0]->triangle_view().size(),
				        "The scale-specific tessellation must retain enough additional curve detail");
			require(instances[0]->transform[3].x != instances[1]->transform[3].x,
			        "Shared local vertices must retain each use instance's layout transform");
		}

		const auto effect = svg::parse(R"svg(<svg><defs><rect id="tile" width="8" height="8" fill="red"/></defs>
		    <g opacity=".5"><use href="#tile"/><use href="#tile" x="12"/></g></svg>)svg");
		require(effect.geometry.size() == 1 && effect.geometry.front().effect &&
		            effect.geometry.front().effect->source.size() == 2,
		        "An effect group must preserve each reused element as a source part");
		const auto& effect_parts = effect.geometry.front().effect->source;
		require(effect_parts[0].shared_triangles &&
		            effect_parts[0].shared_triangles == effect_parts[1].shared_triangles,
		        "Repeated use geometry inside effects must retain shared tessellation ownership");
	}
} // namespace

int main()
{
	try
	{
		compact_arc_flags();
		post_closepath_continuation();
		intersecting_fills();
		zero_length_path_strokes();
		rectangle_corner_radii();
		nested_fill_rules();
		disjoint_compound_output();
		closed_contour_dash_seam();
		path_length_calibrates_stroke_dashes();
		clip_path_support();
		svg_clip_and_symbol_regressions();
		marker_and_stroke();
		css_descendant_selectors();
		css_cascade_resolution_is_cached_per_tag();
		css_structural_indexes_are_precomputed();
		svg_filter_dependency_pruning();
		sync_view_reuses_decoded_external_images();
		svg_object_sync_view_preserves_layout_viewport();
		style_media_attributes_filter_rules();
		presentation_attributes_are_lowest_priority_hints();
		css_attribute_selectors();
		inherited_current_color();
		css_media_orientation();
		css_media_conditions_are_cached_with_nested_order();
		css_pseudo_classes();
		svg_issue_128_css_explicit_inherit();
		svg_issues_131_132_133_134_135_136_139_140();
		image_layout();
		effect_graph();
		svg_shader_format_selection();
		opacity_compositing();
		namespace_aware_svg_dispatch();
		foreign_namespace_attributes_are_not_svg_properties();
		css_lang_identifier_escapes();
		css_escaped_identifiers_and_attribute_values();
		css_media_query_negation();
		svg_href_precedence();
		css_color_and_percentage_opacity();
		css_named_srgb_colors();
		paint_server_fallbacks();
		definition_site_inherited_styles();
		empty_gradients_produce_no_paint();
		pointer_events_none_preserves_paint_without_hit_geometry();
		gaussian_blur_edge_modes();
		svg_temporary_targets_share_a_bounded_cache();
		css_forgiving_selector_lists();
		css_rule_index_preserves_cascade_candidates();
		xml_character_references();
		css_geometry_properties_and_empty_paths();
		use_references_and_symbol_viewports();
		percentage_use_coordinates_in_marker_viewport();
		gradient_templates();
		gradient_color_interpolation();
		invalid_css_values_do_not_override_valid_declarations();
		root_viewport_mapping();
		tight_group_geometry_bounds();
		svg_use_reuses_compatible_tessellation();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
