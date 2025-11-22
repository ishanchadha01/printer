#include "include/workers/path_plan.hpp"
#include "include/stl_helpers.hpp"
#include <limits>
#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr float kPlaneEps = 1e-5f;
constexpr float kSnapEps = 1e-4f;
constexpr float kMinSpan = 1e-6f;

float dist2d_sq(const vec3_t& a, const vec3_t& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool close2d(const vec3_t& a, const vec3_t& b, float eps = kSnapEps) {
    return dist2d_sq(a, b) <= eps * eps;
}

float signed_area(const polygon_t& poly) {
    if (poly.size() < 3) return 0.0f;
    const bool closed = poly.front() == poly.back();
    const std::size_t limit = closed ? poly.size() - 1 : poly.size();
    float area = 0.0f;
    for (std::size_t i = 0; i < limit; ++i) {
        const auto& p0 = poly[i];
        const auto& p1 = poly[(i + 1) % limit];
        area += p0.x * p1.y - p1.x * p0.y;
    }
    return 0.5f * area;
}

vec3_t polygon_centroid(const polygon_t& poly) {
    if (poly.empty()) return vec3_t{0.0f, 0.0f, 0.0f};
    const bool closed = poly.front() == poly.back();
    const std::size_t limit = closed && poly.size() > 1 ? poly.size() - 1 : poly.size();
    vec3_t accum{0.0f, 0.0f, 0.0f};
    for (std::size_t i = 0; i < limit; ++i) {
        accum.x += poly[i].x;
        accum.y += poly[i].y;
        accum.z += poly[i].z;
    }
    float inv_count = limit > 0 ? 1.0f / static_cast<float>(limit) : 0.0f;
    return {accum.x * inv_count, accum.y * inv_count, accum.z * inv_count};
}

bool point_in_polygon(const polygon_t& poly, const vec3_t& p) {
    if (poly.size() < 3) return false;
    const bool closed = poly.front() == poly.back();
    const std::size_t limit = closed ? poly.size() - 1 : poly.size();
    bool inside = false;
    for (std::size_t i = 0, j = limit - 1; i < limit; j = i++) {
        const auto& pi = poly[i];
        const auto& pj = poly[j];
        bool intersects = ((pi.y > p.y) != (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y + 1e-12f) + pi.x);
        if (intersects) inside = !inside;
    }
    return inside;
}

polygon_t ensure_closed(polygon_t poly) {
    if (poly.size() < 2) return poly;
    if (!(poly.front() == poly.back())) {
        poly.push_back(poly.front());
    }
    return poly;
}

std::optional<vec3_t> interior_point(const polygon_t& poly) {
    if (poly.size() < 2) return std::nullopt;
    polygon_t closed = ensure_closed(poly);
    if (closed.size() < 3) return std::nullopt;
    const auto& p0 = closed[0];
    const auto& p1 = closed[1];
    float dx = p1.x - p0.x;
    float dy = p1.y - p0.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < kMinSpan) return std::nullopt;
    float nx = -dy / len;
    float ny = dx / len;
    float step = std::max(1e-4f, kSnapEps * 2.0f);
    float orient = signed_area(closed) >= 0.0f ? 1.0f : -1.0f;
    vec3_t mid{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f, p0.z};
    return vec3_t{mid.x + orient * nx * step, mid.y + orient * ny * step, mid.z};
}

std::vector<segment_t> polygon_to_segments(const polygon_t& poly, float z) {
    std::vector<segment_t> segments;
    if (poly.size() < 2) return segments;
    const bool closed = poly.front() == poly.back();
    const std::size_t limit = closed && poly.size() > 1 ? poly.size() - 1 : poly.size();
    segments.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
        vec3_t p0 = poly[i];
        vec3_t p1 = poly[(i + 1) % limit];
        p0.z = z;
        p1.z = z;
        segments.push_back({p0, p1});
    }
    return segments;
}

polygon_t offset_polygon(const polygon_t& poly, float offset, bool outward) {
    if (poly.size() < 3) return {};
    polygon_t closed = ensure_closed(poly);
    const std::size_t limit = closed.size() - 1;
    float area = signed_area(closed);
    if (std::abs(area) < kMinSpan) return {};

    float orientation = area >= 0.0f ? 1.0f : -1.0f;
    float signed_offset = outward ? -orientation * offset : orientation * offset;

    polygon_t result;
    result.reserve(closed.size());

    for (std::size_t i = 0; i < limit; ++i) {
        const auto& prev = closed[(i + limit - 1) % limit];
        const auto& curr = closed[i];
        const auto& next = closed[(i + 1) % limit];

        float e1x = curr.x - prev.x;
        float e1y = curr.y - prev.y;
        float e2x = next.x - curr.x;
        float e2y = next.y - curr.y;

        float len1 = std::sqrt(e1x * e1x + e1y * e1y);
        float len2 = std::sqrt(e2x * e2x + e2y * e2y);
        if (len1 < kMinSpan || len2 < kMinSpan) continue;

        float n1x = -e1y / len1;
        float n1y = e1x / len1;
        float n2x = -e2y / len2;
        float n2y = e2x / len2;

        vec3_t p1{curr.x + signed_offset * n1x, curr.y + signed_offset * n1y, curr.z};
        vec3_t p2{curr.x + signed_offset * n2x, curr.y + signed_offset * n2y, curr.z};

        float det = e1x * e2y - e1y * e2x;
        if (std::abs(det) < kMinSpan) {
            result.push_back({
                curr.x + signed_offset * (n1x + n2x) * 0.5f,
                curr.y + signed_offset * (n1y + n2y) * 0.5f,
                curr.z
            });
            continue;
        }

        float t = ((p2.x - p1.x) * e2y - (p2.y - p1.y) * e2x) / det;
        result.push_back({
            p1.x + t * e1x,
            p1.y + t * e1y,
            curr.z
        });
    }

    result = ensure_closed(result);
    return result;
}

struct GraphEdge {
    std::size_t a;
    std::size_t b;
    bool used = false;
};

struct EdgeKey {
    std::size_t a;
    std::size_t b;
};

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& key) const {
        return (key.a * 73856093u) ^ (key.b * 19349663u);
    }
};

struct EdgeKeyEq {
    bool operator()(const EdgeKey& lhs, const EdgeKey& rhs) const {
        return lhs.a == rhs.a && lhs.b == rhs.b;
    }
};

struct QuantizedKey {
    long long x;
    long long y;
};

long long hash_key(const QuantizedKey& key) {
    return key.x * 73856093LL ^ key.y * 19349663LL;
}

std::vector<polygon_t> build_polygons_from_segments(const std::vector<segment_t>& segments, float eps) {
    std::vector<vec3_t> nodes;
    std::unordered_map<long long, std::vector<std::size_t>> buckets;
    std::vector<GraphEdge> edges;
    edges.reserve(segments.size());
    std::unordered_set<EdgeKey, EdgeKeyHash, EdgeKeyEq> seen_edges;

    auto quantize = [&](const vec3_t& p) {
        return QuantizedKey{std::llround(p.x / eps), std::llround(p.y / eps)};
    };

    auto add_node = [&](const vec3_t& p) -> std::size_t {
        auto key = quantize(p);
        long long hash = hash_key(key);
        auto& bucket = buckets[hash];
        for (auto idx : bucket) {
            if (close2d(nodes[idx], p, eps)) {
                return idx;
            }
        }
        nodes.push_back(p);
        bucket.push_back(nodes.size() - 1);
        return nodes.size() - 1;
    };

    for (const auto& seg : segments) {
        auto a_idx = add_node(seg.first);
        auto b_idx = add_node(seg.second);
        if (a_idx == b_idx) continue;
        EdgeKey key{std::min(a_idx, b_idx), std::max(a_idx, b_idx)};
        if (seen_edges.find(key) != seen_edges.end()) {
            continue;
        }
        seen_edges.insert(key);
        edges.push_back({a_idx, b_idx, false});
    }

    std::vector<std::vector<std::size_t>> adjacency(nodes.size());
    for (std::size_t i = 0; i < edges.size(); ++i) {
        adjacency[edges[i].a].push_back(i);
        adjacency[edges[i].b].push_back(i);
    }

    auto choose_next_edge = [&](std::size_t vertex, std::size_t prev_vertex) -> std::optional<std::size_t> {
        std::optional<std::size_t> fallback;
        for (auto edge_idx : adjacency[vertex]) {
            if (edges[edge_idx].used) continue;
            std::size_t other = edges[edge_idx].a == vertex ? edges[edge_idx].b : edges[edge_idx].a;
            if (other != prev_vertex) {
                return edge_idx;
            }
            fallback = edge_idx;
        }
        return fallback;
    };

    std::vector<polygon_t> polygons;
    for (std::size_t start_edge = 0; start_edge < edges.size(); ++start_edge) {
        if (edges[start_edge].used) continue;

        polygon_t poly;
        edges[start_edge].used = true;
        std::size_t start = edges[start_edge].a;
        std::size_t current = edges[start_edge].b;
        std::size_t prev_vertex = start;
        poly.push_back(nodes[start]);
        poly.push_back(nodes[current]);

        std::size_t guard = 0;
        while (guard++ < edges.size() * 2) {
            auto next_edge_idx = choose_next_edge(current, prev_vertex);
            if (!next_edge_idx.has_value()) break;
            auto& edge = edges[*next_edge_idx];
            edge.used = true;
            std::size_t next_vertex = edge.a == current ? edge.b : edge.a;
            if (next_vertex == start) {
                poly.push_back(nodes[start]);
                break;
            }
            prev_vertex = current;
            current = next_vertex;
            poly.push_back(nodes[current]);
        }

        if (poly.size() >= 4 && poly.front() == poly.back()) {
            polygons.push_back(std::move(poly));
        }
    }

    return polygons;
}

struct ClassifiedPolygon {
    polygon_t poly;
    float area = 0.0f;
    int depth = 0;
    bool is_hole = false;
};

std::vector<ClassifiedPolygon> classify_polygons(std::vector<polygon_t> polys) {
    std::vector<polygon_t> closed;
    closed.reserve(polys.size());
    for (auto& poly : polys) {
        closed.push_back(ensure_closed(std::move(poly)));
    }

    std::vector<ClassifiedPolygon> result;
    result.reserve(closed.size());
    std::vector<std::optional<vec3_t>> interior_pts(closed.size());
    for (std::size_t i = 0; i < closed.size(); ++i) {
        interior_pts[i] = interior_point(closed[i]);
        if (!interior_pts[i].has_value()) {
            interior_pts[i] = polygon_centroid(closed[i]);
        }
    }

    for (std::size_t i = 0; i < closed.size(); ++i) {
        int depth = 0;
        for (std::size_t j = 0; j < closed.size(); ++j) {
            if (i == j) continue;
            if (interior_pts[i].has_value() && point_in_polygon(closed[j], *interior_pts[i])) {
                depth++;
            }
        }
        float area = signed_area(closed[i]);
        bool is_hole = (depth % 2) == 1;
        polygon_t oriented = closed[i];
        if (is_hole && area > 0.0f) {
            std::reverse(oriented.begin(), oriented.end());
            area = -area;
        } else if (!is_hole && area < 0.0f) {
            std::reverse(oriented.begin(), oriented.end());
            area = -area;
        }
        result.push_back({std::move(oriented), area, depth, is_hole});
    }

    std::sort(result.begin(), result.end(), [](const ClassifiedPolygon& a, const ClassifiedPolygon& b) {
        return std::abs(a.area) > std::abs(b.area);
    });
    return result;
}

struct Island {
    polygon_t outer;
    std::vector<polygon_t> holes;
};

std::vector<Island> build_islands(const std::vector<ClassifiedPolygon>& polys) {
    std::vector<Island> islands;
    std::vector<bool> hole_used(polys.size(), false);

    for (std::size_t i = 0; i < polys.size(); ++i) {
        if (polys[i].is_hole) continue;
        Island island;
        island.outer = polys[i].poly;
        for (std::size_t h = 0; h < polys.size(); ++h) {
            if (!polys[h].is_hole || hole_used[h]) continue;
            vec3_t ref_pt = polygon_centroid(polys[h].poly);
            if (point_in_polygon(island.outer, ref_pt)) {
                island.holes.push_back(polys[h].poly);
                hole_used[h] = true;
            }
        }
        islands.push_back(std::move(island));
    }

    return islands;
}

struct Bounds {
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
};

Bounds bounds_for_polygon(const polygon_t& poly) {
    Bounds bounds;
    if (poly.empty()) return bounds;
    const bool closed = poly.front() == poly.back();
    const std::size_t limit = closed && poly.size() > 1 ? poly.size() - 1 : poly.size();
    for (std::size_t i = 0; i < limit; ++i) {
        bounds.min_x = std::min(bounds.min_x, poly[i].x);
        bounds.max_x = std::max(bounds.max_x, poly[i].x);
        bounds.min_y = std::min(bounds.min_y, poly[i].y);
        bounds.max_y = std::max(bounds.max_y, poly[i].y);
    }
    return bounds;
}

std::vector<std::pair<float, float>> spans_for_polygon(const polygon_t& poly, float y_line) {
    std::vector<float> intersections;
    if (poly.size() < 2) return {};
    const bool closed = poly.front() == poly.back();
    const std::size_t limit = closed && poly.size() > 1 ? poly.size() - 1 : poly.size();

    for (std::size_t i = 0; i < limit; ++i) {
        const auto& p0 = poly[i];
        const auto& p1 = poly[(i + 1) % limit];
        if (std::abs(p0.y - p1.y) < kMinSpan) continue; // skip horizontal edges
        bool crosses = (p0.y <= y_line && p1.y > y_line) || (p1.y <= y_line && p0.y > y_line);
        if (!crosses) continue;
        float t = (y_line - p0.y) / (p1.y - p0.y);
        float x = p0.x + t * (p1.x - p0.x);
        intersections.push_back(x);
    }

    std::sort(intersections.begin(), intersections.end());
    std::vector<std::pair<float, float>> spans;
    for (std::size_t i = 0; i + 1 < intersections.size(); i += 2) {
        float x0 = intersections[i];
        float x1 = intersections[i + 1];
        if (x1 - x0 >= kMinSpan) {
            spans.emplace_back(x0, x1);
        }
    }
    return spans;
}

std::vector<std::pair<float, float>> subtract_spans(std::vector<std::pair<float, float>> base, const std::vector<std::pair<float, float>>& cuts) {
    if (base.empty() || cuts.empty()) return base;
    std::vector<std::pair<float, float>> ordered_cuts = cuts;
    std::sort(ordered_cuts.begin(), ordered_cuts.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    std::vector<std::pair<float, float>> result;
    for (const auto& span : base) {
        float start = span.first;
        float end = span.second;
        float cursor = start;
        for (const auto& cut : ordered_cuts) {
            if (cut.second <= cursor || cut.first >= end) continue;
            if (cut.first > cursor + kMinSpan) {
                result.emplace_back(cursor, std::min(cut.first, end));
            }
            cursor = std::max(cursor, cut.second);
            if (cursor >= end) break;
        }
        if (cursor < end - kMinSpan) {
            result.emplace_back(cursor, end);
        }
    }
    return result;
}

std::vector<segment_t> clip_infill(const polygon_t& outer, const std::vector<polygon_t>& holes, float spacing, float z) {
    std::vector<segment_t> infill;
    if (outer.empty() || spacing <= 0.0f) return infill;
    Bounds bounds = bounds_for_polygon(outer);
    if (bounds.min_x == std::numeric_limits<float>::max()) return infill;

    for (float y = bounds.min_y; y <= bounds.max_y + kSnapEps; y += spacing) {
        auto spans = spans_for_polygon(outer, y);
        if (spans.empty()) continue;
        for (const auto& hole : holes) {
            auto hole_spans = spans_for_polygon(hole, y);
            spans = subtract_spans(std::move(spans), hole_spans);
            if (spans.empty()) break;
        }
        for (const auto& span : spans) {
            vec3_t start{span.first, y, z};
            vec3_t end{span.second, y, z};
            infill.push_back({start, end});
        }
    }

    return infill;
}

} // namespace


void PathPlanner::set_cad(std::filesystem::path cad_file) {
    if (is_stl_ascii(cad_file.string())) {
        this->meshes = read_stl_ascii(cad_file.string());
    } else {
        this->meshes.clear();
        this->meshes.emplace_back(read_stl_binary(cad_file.string()));
    }
    shift_meshes_to_build_plate();
}


std::vector<segment_t> Mesh::intersect_triangle_with_plane(const triangle_t& tri, float z_plane) const {
    const vec3_t& v0 = this->points[tri.vertices[0]];
    const vec3_t& v1 = this->points[tri.vertices[1]];
    const vec3_t& v2 = this->points[tri.vertices[2]];

    float d0 = v0.z - z_plane;
    float d1 = v1.z - z_plane;
    float d2 = v2.z - z_plane;

    auto on_plane = [](float d) { return std::abs(d) < kPlaneEps; };

    std::vector<segment_t> segments;
    std::vector<vec3_t> intersections;
    intersections.reserve(3);
    std::vector<segment_t> coplanar_edges;

    auto add_point = [&](const vec3_t& v) {
        vec3_t p{v.x, v.y, z_plane};
        for (const auto& existing : intersections) {
            if (close2d(existing, p, kPlaneEps)) return;
        }
        intersections.push_back(p);
    };

    auto add_edge = [&](const vec3_t& a, const vec3_t& b) {
        vec3_t p0{a.x, a.y, z_plane};
        vec3_t p1{b.x, b.y, z_plane};
        if (!close2d(p0, p1, kPlaneEps)) {
            coplanar_edges.push_back({p0, p1});
        }
    };

    auto handle_edge = [&](const vec3_t& a, const vec3_t& b, float da, float db) {
        if (on_plane(da) && on_plane(db)) {
            add_edge(a, b);
            return;
        }
        if (on_plane(da)) {
            add_point(a);
            return;
        }
        if (on_plane(db)) {
            add_point(b);
            return;
        }
        if ((da < 0.0f && db > 0.0f) || (da > 0.0f && db < 0.0f)) {
            float t = da / (da - db);
            if (t < -kPlaneEps || t > 1.0f + kPlaneEps) return;
            vec3_t p{
                a.x + t * (b.x - a.x),
                a.y + t * (b.y - a.y),
                z_plane
            };
            add_point(p);
        }
    };

    handle_edge(v0, v1, d0, d1);
    handle_edge(v1, v2, d1, d2);
    handle_edge(v2, v0, d2, d0);

    segments.insert(segments.end(), coplanar_edges.begin(), coplanar_edges.end());

    if (intersections.size() == 2) {
        segments.push_back({intersections[0], intersections[1]});
    } else if (intersections.size() == 3) {
        segments.push_back({intersections[0], intersections[1]});
        segments.push_back({intersections[1], intersections[2]});
    }

    return segments;
}

std::vector<std::vector<segment_t>> PathPlanner::populate_layer_lists(const Mesh& mesh, int layer_height_mm) const {
    size_t num_layers = static_cast<size_t>(MAX_PART_HEIGHT_MM / layer_height_mm);
    std::vector<std::vector<segment_t>> layers(num_layers);
    for (const triangle_t& tri : mesh.triangles) {
        float z0 = mesh.points[tri.vertices[0]].z;
        float z1 = mesh.points[tri.vertices[1]].z;
        float z2 = mesh.points[tri.vertices[2]].z;
        auto max_z = std::max({z0, z1, z2});
        auto min_z = std::min({z0, z1, z2});
        auto start_layer = static_cast<int>(std::floor(min_z / layer_height_mm));
        auto end_layer = static_cast<int>(std::ceil(max_z / layer_height_mm));

        start_layer = std::max(0, start_layer);
        end_layer = std::min(static_cast<int>(num_layers) - 1, end_layer);

        for (int l = start_layer; l <= end_layer; l++) {
            float layer_z = static_cast<float>(l * layer_height_mm);
            auto segs = mesh.intersect_triangle_with_plane(tri, layer_z);
            if (!segs.empty()) {
                auto& layer = layers[static_cast<std::size_t>(l)];
                layer.insert(layer.end(), segs.begin(), segs.end());
            }
        }
    }
    return layers;
}

void PathPlanner::slice_planar(int layer_height_mm, float infill_spacing) {
    plan_.clear();
    raw_layers_.clear();
    if (meshes.empty() || layer_height_mm <= 0) return;

    const int perimeter_count = 2;
    const float shell_width = std::max(0.25f, static_cast<float>(layer_height_mm) * 0.5f);

    size_t num_layers = static_cast<size_t>(MAX_PART_HEIGHT_MM / layer_height_mm);
    std::vector<std::vector<segment_t>> per_layer_segments(num_layers);
    raw_layers_.resize(num_layers);

    for (const auto& mesh : meshes) {
        auto layers_raw = populate_layer_lists(mesh, layer_height_mm);
        for (std::size_t l = 0; l < layers_raw.size(); ++l) {
            if (layers_raw[l].empty()) continue;
            per_layer_segments[l].insert(per_layer_segments[l].end(), layers_raw[l].begin(), layers_raw[l].end());
            for (const auto& seg : layers_raw[l]) {
                raw_layers_[l].push_back(seg.first);
                raw_layers_[l].push_back(seg.second);
            }
        }
    }

    std::vector<LayerPlan> built_layers;
    built_layers.reserve(per_layer_segments.size());

    for (std::size_t l = 0; l < per_layer_segments.size(); ++l) {
        if (per_layer_segments[l].empty()) continue;
        float z = static_cast<float>(l * layer_height_mm);

        auto polygons = build_polygons_from_segments(per_layer_segments[l], kSnapEps);
        if (polygons.empty()) continue;

        auto classified = classify_polygons(std::move(polygons));
        auto islands = build_islands(classified);
        if (islands.empty()) continue;

        LayerPlan layer_plan;
        layer_plan.z = z;

        for (const auto& island : islands) {
            polygon_t outer_for_infill = island.outer;
            polygon_t working_outer = outer_for_infill;
            for (int p = 0; p < perimeter_count; ++p) {
                auto contour_segments = polygon_to_segments(working_outer, z);
                layer_plan.contours.insert(layer_plan.contours.end(), contour_segments.begin(), contour_segments.end());
                if (p + 1 < perimeter_count) {
                    auto inset = offset_polygon(working_outer, shell_width, false);
                    if (inset.size() < 3) break;
                    working_outer = std::move(inset);
                }
            }
            outer_for_infill = working_outer;

            std::vector<polygon_t> hole_polys_for_infill;
            for (const auto& hole : island.holes) {
                polygon_t working_hole = hole;
                for (int p = 0; p < perimeter_count; ++p) {
                    auto contour_segments = polygon_to_segments(working_hole, z);
                    layer_plan.contours.insert(layer_plan.contours.end(), contour_segments.begin(), contour_segments.end());
                    if (p + 1 < perimeter_count) {
                        auto outset = offset_polygon(working_hole, shell_width, true);
                        if (outset.size() < 3) break;
                        working_hole = std::move(outset);
                    }
                }
                hole_polys_for_infill.push_back(working_hole);
            }

            auto infill_segments = clip_infill(outer_for_infill, hole_polys_for_infill, infill_spacing, z);
            layer_plan.infill.insert(layer_plan.infill.end(), infill_segments.begin(), infill_segments.end());
        }

        if (!layer_plan.contours.empty() || !layer_plan.infill.empty()) {
            built_layers.push_back(std::move(layer_plan));
        }
    }

    plan_.swap(built_layers);
}

void PathPlanner::shift_meshes_to_build_plate() {
    if (meshes.empty()) return;

    float min_coord = std::numeric_limits<float>::max();
    for (const auto& mesh : meshes) {
        for (const auto& pt : mesh.points) {
            min_coord = std::min({min_coord, pt.x, pt.y, pt.z});
        }
    }

    if (min_coord >= 0.0f || min_coord == std::numeric_limits<float>::max()) return;

    float offset = -min_coord;
    for (auto& mesh : meshes) {
        for (auto& pt : mesh.points) {
            pt.x += offset;
            pt.y += offset;
            pt.z += offset;
        }
    }
}
