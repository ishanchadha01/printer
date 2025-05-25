class Mesh {
public:
    Mesh(std::filesystem::path cad_file);

private:
    // Planar, cartesian

    std::vector<point_t> points;
    std::vector<triangle_t> triangles; // sorted by layer max z-vals
    std::vector<std::vector<triangle_t>> layers_by_triangle;
}