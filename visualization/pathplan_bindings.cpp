#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "include/workers/path_plan.hpp"
#include "include/stl_helpers.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pathplan_bindings, m) {
    m.doc() = "Bindings for PathPlanner slicing and mesh inspection";

    py::class_<vec3_t>(m, "Vec3")
        .def(py::init<>())
        .def_readwrite("x", &vec3_t::x)
        .def_readwrite("y", &vec3_t::y)
        .def_readwrite("z", &vec3_t::z)
        .def("__repr__", [](const vec3_t& v) {
            return "<Vec3 (" + std::to_string(v.x) + ", " +
                   std::to_string(v.y) + ", " + std::to_string(v.z) + ")>";
        });

    py::class_<triangle_t>(m, "Triangle")
        .def(py::init<>())
        .def_readwrite("vertices", &triangle_t::vertices)
        .def_readwrite("normal_vec", &triangle_t::normal_vec)
        .def_readwrite("centroid", &triangle_t::centroid);

    py::class_<Mesh>(m, "Mesh")
        .def(py::init<>())
        .def_readwrite("points", &Mesh::points)
        .def_readwrite("triangles", &Mesh::triangles);

    py::class_<PathPlanner::LayerPlan>(m, "LayerPlan")
        .def(py::init<>())
        .def_readwrite("z", &PathPlanner::LayerPlan::z)
        .def_readwrite("contours", &PathPlanner::LayerPlan::contours)
        .def_readwrite("infill", &PathPlanner::LayerPlan::infill);

    py::class_<PathPlanner, std::shared_ptr<PathPlanner>>(m, "PathPlanner")
        .def(py::init<>())
        .def("set_cad", &PathPlanner::set_cad, py::arg("cad_file"))
        .def("slice_planar", &PathPlanner::slice_planar, py::arg("layer_height_mm"), py::arg("infill_spacing"))
        .def("layer_count", &PathPlanner::layer_count)
        .def("get_layer", &PathPlanner::get_layer, py::return_value_policy::reference_internal)
        .def("get_layer_contours", &PathPlanner::get_layer_contours)
        .def("get_layer_infill", &PathPlanner::get_layer_infill)
        .def("get_plan", &PathPlanner::get_plan, py::return_value_policy::reference_internal)
        .def("get_meshes", &PathPlanner::get_meshes, py::return_value_policy::reference_internal)
        .def("get_raw_layers", &PathPlanner::get_raw_layers, py::return_value_policy::reference_internal)
        .def("get_raw_layer_points", &PathPlanner::get_raw_layer_points);

    m.def("is_stl_ascii", &is_stl_ascii, py::arg("filename"));
}
