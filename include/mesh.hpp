#pragma once

#include "include/printer_types.hpp"

#include <filesystem>


class Mesh {
public:
    Mesh();
    void slice_nonplanar(int layer_height_mm);

    // Planar, cartesian
    std::vector<vec3_t> points;
    std::vector<triangle_t> triangles; // sorted by layer max z-vals
    std::vector<std::vector<triangle_t>> layers_by_triangle;
};