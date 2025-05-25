#include <filesystem>

class PathPlan
{

public:
    PathPlan(std::filesystem::path cad_file);

private:
    int plan_path();

    int slice_planar();
    int compute_infill();
    int compute_brim();

    std::unique_ptr<mesh_t> mesh;
}