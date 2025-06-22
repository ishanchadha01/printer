#pragma once

#include "include/printer_types.hpp"

#include <filesystem>
#include <cmath>


class Mesh {
public:
    Mesh(const std::string cad_filepath);
    void slice_nonplanar(int layer_height_mm);

    // Planar, cartesian
    std::vector<vec3_t> points;
    std::vector<triangle_t> triangles; // sorted by layer max z-vals

    std::vector<vec3_t> intersect_triangle_with_plane(const triangle_t& tri, float z_plane);

};