#include "include/workers/path_plan.hpp"
#include "include/stl_helpers.hpp" 
#include <limits>
#include <algorithm>
#include <cmath>


void PathPlanner::set_cad(std::filesystem::path cad_file) {
    if (is_stl_ascii(cad_file.string())) {
        this->meshes = read_stl_ascii(cad_file.string());
    } else {
        this->meshes.clear();
        this->meshes.emplace_back(read_stl_binary(cad_file.string()));
    }
    shift_meshes_to_build_plate();
}


std::vector<vec3_t> Mesh::intersect_triangle_with_plane(const triangle_t& tri, float z_plane) const {
    std::vector<vec3_t> intersection_pts;
    intersection_pts.reserve(2); // at most 2 useful intersection points per triangle

    for (int i = 0; i < 3; ++i) {
        auto v0_idx = tri.vertices[i];
        auto v1_idx = tri.vertices[(i + 1) % 3];

        const vec3_t& v0 = this->points[v0_idx];
        const vec3_t& v1 = this->points[v1_idx];

        // if theyre both on plane, just add both
        // also prevents div by 0
        if (v1.z - v0.z == 0) {
            intersection_pts.emplace_back(vec3_t{v0.x, v0.y, z_plane});
            intersection_pts.emplace_back(vec3_t{v1.x, v1.y, z_plane});
            continue;
        }

        // Check if the segment crosses the plane
        if ((v0.z <= z_plane && v1.z >= z_plane) || (v0.z >= z_plane && v1.z <= z_plane)) {
            float t = (z_plane - v0.z) / (v1.z - v0.z);
            if (t<0.0f || t>1.0f) continue; // guard the case where they're not on segment due to floating point error
            float x = v0.x + t * (v1.x - v0.x);
            float y = v0.y + t * (v1.y - v0.y);
            intersection_pts.emplace_back(vec3_t{x, y, z_plane});
        }
    }

    return intersection_pts;
}


void Mesh::populate_layer_lists(int layer_height_mm) {
    // take minz and maxz for current layer height, and find all overlapping intervals
    // once intervals are found, then intersection needs to be computed between triangle and z value
    size_t num_layers = static_cast<size_t>(MAX_PART_HEIGHT_MM / layer_height_mm);
    std::vector<std::vector<vec3_t>> layers_by_line_segment(num_layers);
    for (triangle_t tri : this->triangles) {
        float z0 = points[tri.vertices[0]].z;
        float z1 = points[tri.vertices[1]].z;
        float z2 = points[tri.vertices[2]].z;
        auto max_z = std::max({z0, z1, z2});
        auto min_z = std::min({z0, z1, z2});
        auto start_layer = static_cast<int>(std::floor(min_z / layer_height_mm));
        auto end_layer = static_cast<int>(std::ceil(max_z / layer_height_mm));

        // Clamp start and end layer just in case min_z<0 or max_z>MAX_HEIGHT
        start_layer = std::max(0, start_layer);
        end_layer = std::min(static_cast<int>(num_layers) - 1, end_layer);

        //find intersecting points for each layer that this triangle intersects with
        for (int l=start_layer; l<=end_layer; l++) {
            auto layer_z = l*layer_height_mm;
            auto intersection_pts = this->intersect_triangle_with_plane(tri, layer_z);
            auto& layer = layers_by_line_segment[l];
            if (!intersection_pts.empty()) {
                layer.insert(layer.end(), intersection_pts.begin(), intersection_pts.end());
            }
        }
    }

    // TODO: stitch together line segments for each layer
}

std::vector<std::vector<vec3_t>> PathPlanner::populate_layer_lists(const Mesh& mesh, int layer_height_mm) {
    size_t num_layers = static_cast<size_t>(MAX_PART_HEIGHT_MM / layer_height_mm);
    std::vector<std::vector<vec3_t>> layers_by_line_segment(num_layers);
    for (triangle_t tri : mesh.triangles) {
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
            auto layer_z = l * layer_height_mm;
            auto intersection_pts = mesh.intersect_triangle_with_plane(tri, layer_z);
            auto& layer = layers_by_line_segment[static_cast<std::size_t>(l)];
            if (!intersection_pts.empty()) {
                layer.insert(layer.end(), intersection_pts.begin(), intersection_pts.end());
            }
        }
    }
    return layers_by_line_segment;
}

std::vector<std::pair<vec3_t, vec3_t>> PathPlanner::contour_layer(const std::vector<vec3_t>& raw_pts, float z) {
    std::vector<std::pair<vec3_t, vec3_t>> contours;
    if (raw_pts.size() < 2) return contours;

    contours.reserve(raw_pts.size() / 2);
    for (std::size_t i = 0; i + 1 < raw_pts.size(); i += 2) {
        vec3_t start = raw_pts[i];
        vec3_t end = raw_pts[i + 1];
        start.z = z;
        end.z = z;
        contours.emplace_back(start, end);
    }
    return contours;
}

std::vector<std::pair<vec3_t, vec3_t>> PathPlanner::compute_infill(const std::vector<vec3_t>& perimeter_pts, float z, float spacing) {
    std::vector<std::pair<vec3_t, vec3_t>> infill_lines;
    if (perimeter_pts.empty() || spacing <= 0.0f) return infill_lines;

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();

    for (const auto& pt : perimeter_pts) {
        min_x = std::min(min_x, pt.x);
        min_y = std::min(min_y, pt.y);
        max_x = std::max(max_x, pt.x);
        max_y = std::max(max_y, pt.y);
    }

    if (min_x == std::numeric_limits<float>::max()) return infill_lines;

    for (float y = min_y; y <= max_y; y += spacing) {
        vec3_t start{min_x, y, z};
        vec3_t end{max_x, y, z};
        infill_lines.emplace_back(start, end);
    }
    return infill_lines;
}

void PathPlanner::slice_planar(int layer_height_mm, float infill_spacing) {
    plan_.clear();
    if (meshes.empty() || layer_height_mm <= 0) return;

    size_t num_layers = static_cast<size_t>(MAX_PART_HEIGHT_MM / layer_height_mm);
    plan_.resize(num_layers);
    raw_layers_.clear();
    raw_layers_.resize(num_layers);

    for (const auto& mesh : meshes) {
        auto layers_raw = populate_layer_lists(mesh, layer_height_mm);
        for (std::size_t l = 0; l < layers_raw.size(); ++l) {
            auto z = static_cast<float>(l * layer_height_mm);
            auto contours = contour_layer(layers_raw[l], z);
            auto infill = compute_infill(layers_raw[l], z, infill_spacing);

            if (!layers_raw[l].empty()) {
                raw_layers_[l].insert(raw_layers_[l].end(), layers_raw[l].begin(), layers_raw[l].end());
            }

            if (!contours.empty() || !infill.empty()) {
                plan_[l].z = z;
                plan_[l].contours.insert(plan_[l].contours.end(), contours.begin(), contours.end());
                plan_[l].infill.insert(plan_[l].infill.end(), infill.begin(), infill.end());
            }
        }
    }

    std::vector<LayerPlan> trimmed;
    for (const auto& layer : plan_) {
        if (!layer.contours.empty() || !layer.infill.empty()) {
            trimmed.push_back(layer);
        }
    }
    plan_.swap(trimmed);
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
