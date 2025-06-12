#include "include/mesh.hpp"

// Mesh::Mesh(std::filesystem::path cad_file) {
//     // read cad file

//     // populate points

//     // if triangles exist, populate; else create

//     // compute normals
// }

Mesh::Mesh() {
    // initialize data structures
}


void Mesh::populate_layer_lists(int layer_height_mm) {
    // sort z by layer height and arrange into layer bins
    std::sort(triangles.begin(), triangles.end(), [](const triangle_t& a, const triangle_t& b) {
        auto max_z1 = std::max({a.vertices[0].z, a.vertices[1].z a.vertices[1].z});
        auto max_z2 = std::max({b.vertices[0].z, b.vertices[1].z, b.vertices[1].z});
        return max_z1 > max_z2;
    });

    layers_by_triangle = std::vector<std::vector<triangle_t>>();
    int cumulative_height = layer_height_mm;
    for (auto tri : triangles) {
        auto max_z = std::max({tri.vertices[0].z, tri.vertices[1].z tri.vertices[1].z});
        while
    }

    // above approach is not correct
    // brute force is take minz and maxz for current layer height, and find all overlapping intervals
    // once intervals are found, then intersection needs to be computed between triangle and z value, which can require interpolation
    size_t num_layers = static_cast<size_t>(MAX_PART_HEIGHT_MM / layer_height_mm);
    layers_by_triangle = std::vector<std::vector<triangle_t>>(num_layers, std::vector<triangle_t>());
    for (triangle_t tri : this->triangles) {
        auto max_z = std::max({tri.vertices[0].z, tri.vertices[1].z tri.vertices[1].z});
        auto min_z = std::min({tri.vertices[0].z, tri.vertices[1].z tri.vertices[1].z});
        auto start_layer = std::round(min_z * 10.0f) / 10.0f; // can't be lower than .1mm
        auto end_layer = std::round(max_z * 10.0f) / 10.0f;
        for (int i=start_layer; i<=end_layer; i++) {
            // find intersecting pts ? is it even necessary to store which triangles belong to which layer?
        }
    }
}