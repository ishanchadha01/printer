#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <vector>
#include <chrono>
#include <atomic>

#include "include/stl_helpers.hpp"

namespace {

std::filesystem::path test_data_path(const std::string& filename) {
    auto here = std::filesystem::path(__FILE__).parent_path();
    return here / "data" / filename;
}

std::filesystem::path temp_stl_path(const std::string& suffix) {
    static std::atomic<uint32_t> counter{0};
    auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
    auto name = "stl_test_" + std::to_string(ts) + "_" + std::to_string(counter++) + suffix;
    return std::filesystem::temp_directory_path() / name;
}

std::filesystem::path write_ascii_file(const std::string& contents) {
    auto path = temp_stl_path(".stl");
    std::ofstream out(path);
    out << contents;
    return path;
}

struct BinaryTriangle {
    vec3_t normal;
    std::array<vec3_t, 3> vertices;
    uint16_t attribute_byte_count{0};
};

std::filesystem::path write_binary_file(const std::vector<BinaryTriangle>& tris) {
    auto path = temp_stl_path(".bin.stl");
    std::ofstream out(path, std::ios::binary);

    char header[80] = {0};
    out.write(header, sizeof(header));

    uint32_t tri_count = static_cast<uint32_t>(tris.size());
    out.write(reinterpret_cast<const char*>(&tri_count), sizeof(tri_count));

    for (const auto& tri : tris) {
        out.write(reinterpret_cast<const char*>(&tri.normal.x), sizeof(float));
        out.write(reinterpret_cast<const char*>(&tri.normal.y), sizeof(float));
        out.write(reinterpret_cast<const char*>(&tri.normal.z), sizeof(float));
        for (const auto& v : tri.vertices) {
            out.write(reinterpret_cast<const char*>(&v.x), sizeof(float));
            out.write(reinterpret_cast<const char*>(&v.y), sizeof(float));
            out.write(reinterpret_cast<const char*>(&v.z), sizeof(float));
        }
        out.write(reinterpret_cast<const char*>(&tri.attribute_byte_count), sizeof(uint16_t));
    }

    return path;
}

} // namespace

class StlReaderTest : public testing::Test {};

TEST_F(StlReaderTest, DetectsAsciiExample) {
    auto ascii_path = test_data_path("ascii_example.stl");
    ASSERT_TRUE(is_stl_ascii(ascii_path.string()));
}

TEST_F(StlReaderTest, DetectsBinaryAsNonAscii) {
    BinaryTriangle tri{
        /*normal*/ {0.0f, 0.0f, 1.0f},
        /*verts*/ {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}},
        /*attr*/  2 // non-zero attribute should be skipped
    };
    auto path = write_binary_file({tri});
    EXPECT_FALSE(is_stl_ascii(path.string()));
}

TEST_F(StlReaderTest, ParsesMultipleAsciiSolidsWithDeduplication) {
    auto ascii_path = test_data_path("ascii_example.stl");
    auto solids = read_stl_ascii(ascii_path.string());

    ASSERT_EQ(solids.size(), 2u);
    EXPECT_EQ(solids[0].triangles.size(), 4u);
    EXPECT_EQ(solids[0].points.size(), 4u);
    EXPECT_EQ(solids[1].triangles.size(), 4u);
    EXPECT_EQ(solids[1].points.size(), 4u);
}

TEST_F(StlReaderTest, FixesWindingWhenNormalDisagreesAscii) {
    const std::string ascii_stl = R"(
solid single
  facet normal 0 0 1
    outer loop
      vertex 0 0 0
      vertex 0 1 0
      vertex 1 0 0
    endloop
  endfacet
endsolid single
)";
    auto path = write_ascii_file(ascii_stl);
    auto solids = read_stl_ascii(path.string());
    ASSERT_EQ(solids.size(), 1u);
    ASSERT_EQ(solids[0].triangles.size(), 1u);

    const auto& tri = solids[0].triangles[0];
    auto& pts = solids[0].points;
    std::array<vec3_t, 3> verts = {
        pts[tri.vertices[0]],
        pts[tri.vertices[1]],
        pts[tri.vertices[2]]
    };
    auto measured = tri.compute_normal(verts).normalize();
    auto stored = tri.normal_vec;
    EXPECT_NEAR(measured.x, stored.x, 1e-6);
    EXPECT_NEAR(measured.y, stored.y, 1e-6);
    EXPECT_NEAR(measured.z, stored.z, 1e-6);
}

TEST_F(StlReaderTest, ThrowsOnUnknownAsciiKeyword) {
    const std::string bad_ascii = R"(
solid bad
  foo 0 0 0
endsolid bad
)";
    auto path = write_ascii_file(bad_ascii);
    EXPECT_THROW(read_stl_ascii(path.string()), std::runtime_error);
}

TEST_F(StlReaderTest, ParsesBinaryWithWindingFixAndCentroid) {
    BinaryTriangle tri{
        /*normal*/ {0.0f, 0.0f, 1.0f},
        /*verts (intentionally flipped)*/
        {{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}},
        /*attr*/  0
    };
    auto path = write_binary_file({tri});
    auto mesh = read_stl_binary(path.string());

    ASSERT_EQ(mesh.triangles.size(), 1u);
    ASSERT_EQ(mesh.points.size(), 3u);

    const auto& parsed = mesh.triangles[0];
    std::array<vec3_t, 3> verts = {
        mesh.points[parsed.vertices[0]],
        mesh.points[parsed.vertices[1]],
        mesh.points[parsed.vertices[2]]
    };

    auto measured = parsed.compute_normal(verts).normalize();
    auto stored = parsed.normal_vec;
    EXPECT_NEAR(measured.x, stored.x, 1e-6);
    EXPECT_NEAR(measured.y, stored.y, 1e-6);
    EXPECT_NEAR(measured.z, stored.z, 1e-6);

    EXPECT_NEAR(parsed.centroid.x, 1.0f / 3.0f, 1e-6);
    EXPECT_NEAR(parsed.centroid.y, 1.0f / 3.0f, 1e-6);
    EXPECT_NEAR(parsed.centroid.z, 0.0f, 1e-6);
}

TEST_F(StlReaderTest, DeduplicatesBinaryVerticesAcrossTriangles) {
    BinaryTriangle tri1{
        {0.0f, 0.0f, 1.0f},
        {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}},
        0
    };
    BinaryTriangle tri2{
        {0.0f, 0.0f, 1.0f},
        {{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}},
        1 // attribute byte count ignored but present
    };
    auto path = write_binary_file({tri1, tri2});
    auto mesh = read_stl_binary(path.string());

    EXPECT_EQ(mesh.triangles.size(), 2u);
    EXPECT_EQ(mesh.points.size(), 4u); // (0,0,0), (1,0,0), (0,1,0), (1,1,0)
}

TEST_F(StlReaderTest, ThrowsOnTruncatedBinaryFile) {
    auto path = temp_stl_path(".truncated.stl");
    std::ofstream out(path, std::ios::binary);
    char header[80] = {0};
    out.write(header, sizeof(header));
    uint32_t tri_count = 1;
    out.write(reinterpret_cast<const char*>(&tri_count), sizeof(tri_count));
    // do not write triangle bytes

    EXPECT_THROW(read_stl_binary(path.string()), std::runtime_error);
}
