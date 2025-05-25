#include <vector>
#include <iostream>


// Bookkeeping

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

struct point_t {
    float x,y,z;

    point_t operator+(point_t& other) const {
        return {x+other.x, y+other.y, z+other.z};
    }

    point_t operator+(point_t& other) const {
        return {x-other.x, y-other.y, z-other.z};
    }

    point_t operator*(float scalar) const {
        return {scalar*x, scalar*y, scalar*z};
    }

    bool operator==(const point_t& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    float dot(const point_t& other) const {
        return x*other.x + y*other.y + z*other.z;
    }

    point_t cross(const point_t& other) const {
        return {
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    friend std::ostream& operator<<(std::ostream& os, const point_t& p) {
        return os << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    }
};



// Triangle

struct triangle_t {
    uint32_t A,B,C;
    point_t normal_vec;
    point_t centroid;

    void compute_normal(const std::vector<point_t>& vertices) {
        point_t pA = vertices[A];
        point_t pB = vertices[B];
        point_t pC = vertices[C];
        point_t AB = pB-pA;
        point_t AC = pC-pA;
        normal_vec = AB.cross(AC);
    }

    void compute_centroid(const std::vector<point_t>& vertices) {
        point_t pA = vertices[A];
        point_t pB = vertices[B];
        point_t pC = vertices[C];
        centroid = (pA+pB+pC) * (1.0f/3.0f);
    }
};