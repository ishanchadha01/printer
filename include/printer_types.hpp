#include <boost/functional/hash.hpp> // TODO: optimize hash function

#include <vector>
#include <iostream>


// Bookkeeping
const double PT_EQUAL_THRESH = 1e-6;

enum class status_t {
    ERROR=-1,
    SUCCESS=0,
};

enum class state_t {
    INPUT_CAD=0,
    CREATE_PATH=1,
    CALIBRATE=2,
    MOVE_TO_START=3,
    EXECUTE_MOTION=4,
    FINISH_PATH=5,
};


// Point

struct vec3_t {
    float x,y,z;

    vec3_t operator+(vec3_t& other) const {
        return {x+other.x, y+other.y, z+other.z};
    }

    vec3_t operator+(vec3_t& other) const {
        return {x-other.x, y-other.y, z-other.z};
    }

    vec3_t operator*(float scalar) const {
        return {scalar*x, scalar*y, scalar*z};
    }

    // check if theyre all close
    bool operator==(const vec3_t& other) const {
        return std::abs(x - other.x) < PT_EQUAL_THRESH &&
            std::abs(y - other.y) < PT_EQUAL_THRESH &&
            std::abs(z - other.z) < PT_EQUAL_THRESH;
    }

    bool operator!=(const vec3_t& other) const {
        return !(*this == other);
    }

    float dot(const vec3_t& other) const {
        return x*other.x + y*other.y + z*other.z;
    }

    vec3_t cross(const vec3_t& other) const {
        return {
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    friend std::ostream& operator<<(std::ostream& os, const vec3_t& p) {
        return os << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    }

    friend size_t hash_value(const vec3_t& p) {
        size_t seed = 0;
        seed = boost::hash_combine(seed, p.x);
        seed = boost::hash_combine(seed, p.y);
        seed = boost::hash_combine(seed, p.z);
        return seed;
    }
};



// Triangle

struct triangle_t {
    std::array<uint32_t, 3> vertices; // references to points in mesh
    vec3_t normal_vec;
    vec3_t centroid;

    void compute_normal(const std::array<vec3_t, 3>& vertices) {
        vec3_t pA = vertices[A];
        vec3_t pB = vertices[B];
        vec3_t pC = vertices[C];
        vec3_t AB = pB-pA;
        vec3_t AC = pC-pA;
        normal_vec = AB.cross(AC);
    }

    void compute_centroid(const std::array<vec3_t, 3>& vertices) {
        vec3_t pA = vertices[A];
        vec3_t pB = vertices[B];
        vec3_t pC = vertices[C];
        centroid = (pA+pB+pC) * (1.0f/3.0f);
    }
};