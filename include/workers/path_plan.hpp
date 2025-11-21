
// include macros - ramps, gcodes

// look at marlin stepper and planner

// A+B moves diagonal, -A-B moves diagonal
// A-B or B-A moves one  direction

// is given initial gcode
// modifies gcode based on sensed data

#pragma once

#include "include/containers/mesh.hpp"
#include "include/containers/worker_thread.hpp"

#include <filesystem>
#include <vector>
#include <memory>
#include <cstddef>
#include <utility>

class PathPlanner : public WorkerThread
{

public:
    PathPlanner(uint8_t id, std::shared_ptr<DefaultBuffer> buffer) :
        WorkerThread(id, std::move(buffer)) {};

    PathPlanner() : WorkerThread(0, std::make_shared<DefaultBuffer>()) {}

    void set_cad(std::filesystem::path cad_file);

    // planar-only slice + infill builder
    void slice_planar(int layer_height_mm, float infill_spacing);

    struct LayerPlan {
        float z = 0.0f;
        std::vector<std::pair<vec3_t, vec3_t>> contours;
        std::vector<std::pair<vec3_t, vec3_t>> infill;
    };

    const std::vector<LayerPlan>& get_plan() const { return plan_; }
    std::size_t layer_count() const { return plan_.size(); }
    const LayerPlan& get_layer(std::size_t idx) const { return plan_.at(idx); }
    std::vector<std::pair<vec3_t, vec3_t>> get_layer_contours(std::size_t idx) const { return plan_.at(idx).contours; }
    std::vector<std::pair<vec3_t, vec3_t>> get_layer_infill(std::size_t idx) const { return plan_.at(idx).infill; }
    const std::vector<Mesh>& get_meshes() const { return meshes; }
    const std::vector<std::vector<vec3_t>>& get_raw_layers() const { return raw_layers_; }
    std::vector<vec3_t> get_raw_layer_points(std::size_t idx) const { return raw_layers_.at(idx); }

    void run() override {};

private:
    std::vector<std::vector<vec3_t>> populate_layer_lists(const Mesh& mesh, int layer_height_mm);
    std::vector<std::pair<vec3_t, vec3_t>> contour_layer(const std::vector<vec3_t>& raw_pts, float z);
    std::vector<std::pair<vec3_t, vec3_t>> compute_infill(const std::vector<vec3_t>& perimeter_pts, float z, float spacing);
    void shift_meshes_to_build_plate();

    std::vector<Mesh> meshes;
    std::vector<LayerPlan> plan_;
    std::vector<std::vector<vec3_t>> raw_layers_;
};
