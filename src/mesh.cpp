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

void Mesh::slice_nonplanar(int layer_height_mm) {
    // sort z by layer height and arrange into layer bins
}