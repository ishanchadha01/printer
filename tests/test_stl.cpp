#include <gtest/gtest.h>
#include "include/stl_helpers.hpp" 

class StlReaderTest : public testing::Test {
    protected:
        StlReaderTest() {}
};

TEST_F(StlReaderTest, TestAll) {
    const std::string filename = "../tests/data/ascii_example.stl";
    ASSERT_EQ(is_stl_ascii(filename), true);

    std::vector<Mesh> solids = read_stl_ascii(filename);
    ASSERT_EQ(solids.size(), 2);

    ASSERT_EQ(solids[0].triangles.size(), 4);
    ASSERT_EQ(solids[0].points.size(), 4);
    ASSERT_EQ(solids[1].triangles.size(), 4);
    ASSERT_EQ(solids[1].points.size(), 4);
};