#include "include/workers/path_plan.hpp"


void PathPlanner::set_cad(std::filesystem::path cad_file) {
    this->meshes = read_stl_ascii(cad_file);
}


std::vector<vec3_t> Mesh::intersect_triangle_with_plane(const triangle_t& tri, float z_plane) {
    std::vector<vec3_t> intersection_pts;

    for (int i = 0; i < 3; ++i) {
        auto v0_idx = tri.vertices[i];
        auto v1_idx = tri.vertices[(i + 1) % 3];

        // Check if the segment crosses the plane
        if ((v0.z <= z_plane && v1.z >= z_plane) || (v0.z >= z_plane && v1.z <= z_plane)) {
            float t = (z_plane - v0.z) / (v1.z - v0.z);
            float x = v0.x + t * (v1.x - v0.x);
            float y = v0.y + t * (v1.y - v0.y);
            intersection_pts.emplace_back(vec3_t{x, y});
        }
    }

    return intersection_pts;
}


void Mesh::populate_layer_lists(int layer_height_mm) {
    // take minz and maxz for current layer height, and find all overlapping intervals
    // once intervals are found, then intersection needs to be computed between triangle and z value
    size_t num_layers = static_cast<size_t>(MAX_PART_HEIGHT_MM / layer_height_mm);
    std::vector<std::vector<triangle_t>> layers_by_line_segment;
    for (mesh_t mesh : this->meshes) {
        for (triangle_t tri : this->triangles) {
            auto max_z = std::max({tri.vertices[0].z, tri.vertices[1].z tri.vertices[1].z});
            auto min_z = std::min({tri.vertices[0].z, tri.vertices[1].z tri.vertices[1].z});
            auto start_layer = static_cast<int>(std::floor(min_z / layer_height_mm));
            auto end_layer = static_cast<int>(std::ceil(max_z / layer_height_mm));
            for (int l=start_layer; l<=end_layer; l++) {
                // find intersecting pts
                auto layer_z = l*layer_height_mm;
                auto intersection_pts = this->intersect_triangle_with_plane(tri, layer_z);
                layers_by_line_segment.push_back(intersection_pts);
            }
        }
    }

    // TODO: stitch together line segments for each layer
}