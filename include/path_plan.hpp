#include "include/mesh.hpp"

#include <filesystem>

class PathPlan
{

public:
    PathPlan(std::filesystem::path cad_file);

    int plan_path();

private:

    std::vector<vec3_t> intersect_triangle_with_plane(const triangle_t& tri, float z_plane);
    void populate_layer_lists(int layer_height_mm) {

    // int slice_planar();
    // int compute_infill();
    // int compute_brim();

    std::vector<mesh_t> meshes;
}