#include "include/workers/path_plan.hpp"
#include "include/stl_helpers.hpp" 


void PathPlanner::set_cad(std::filesystem::path cad_file) {
    this->meshes = read_stl_ascii(cad_file);
}


std::vector<vec3_t> Mesh::intersect_triangle_with_plane(const triangle_t& tri, float z_plane) {
    std::vector<vec3_t> intersection_pts;
    intersection_pts.reserve(2); // at most 2 useful intersection points per triangle

    for (int i = 0; i < 3; ++i) {
        auto v0_idx = tri.vertices[i];
        auto v1_idx = tri.vertices[(i + 1) % 3];

        const vec3_t& v0 = this->points[v0_idx];
        const vec3_t& v1 = this->points[v1_idx];

        // if theyre both on place, just add both
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
    std::vector<std::vector<vec3_t>> layers_by_line_segment;
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
            layers_by_line_segment.push_back(intersection_pts);
        }
    }

    // TODO: stitch together line segments for each layer
}